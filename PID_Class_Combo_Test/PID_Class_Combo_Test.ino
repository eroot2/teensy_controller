/********************************************************
 * PID RelayOutput Example
 * Same as basic example, except that this time, the output
 * is going to a digital pin which (we presume) is controlling
 * a relay.  the pid is designed to output an analog value,
 * but the relay can only be On/Off.
 *
 *   to connect them together we use "time proportioning
 * control"  it's essentially a really slow version of PWM.
 * first we decide on a window size (5000mS say.) we then
 * set the pid to adjust its output between 0 and that window
 * size.  lastly, we add some logic that translates the PID
 * output into "Relay On Time" with the remainder of the
 * window being "Relay Off Time"
 ********************************************************/
//MODIFIED FOR TESTING & COMPARISON
#include <PID_v1.h>
#include <SPI.h>
#include <SD.h>
#include <QNEthernet.h>
//#include <new>
//#include <std>
String filename = "test.txt";  //MISSING: Save data to SD functionality, eventually store webpage html on sd card
//for string literals for file names and html, consider a raw string
File webPage;

#define PIN_INPUT 20
#define RELAY_PIN LED_BUILTIN  // currently 13, formerly 5

//INTERNAL AND EXTERNAL PID VARS
double setpoint, input, raw_input, input_C, smoothed_input, output;
double Kp = 4, Ki = 4, Kd = 1;                    //default aggressive k values
double consKp = 1, consKi = 0.05, consKd = 0.25;  //precisionMode active, smaller k values
PID myPID(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT);
bool heatOn = false;  //tracker for whether the heat control has been set to high. that way, it's only set once when it needs to change instead of every tick
//double error, error_prev, integral, output_unbounded;
//double goalConfines, maxOvershoot
//CONSIDER trying a proportional on measurement implementation, can be tuned during runtime

//vars for simple smoothing algorithm
const int recentSamples = 50;    //how many recent input values to save and average for smoothing - probably don't use more than would be generated in one pwm period
double readings[recentSamples];  // the readings from the analog input
int readIndex = 0;               // the index of the current reading
double smoothTtl = 0;            // the running total
double smoothAvg = 0;            // the average


//TIMING VARS
const int PWM_len = 5000;   //cycle length in ms
const int sampleTime = 50;  //polling rate (ms)
//int outputChangeTime = sampleTime;
const int printRate = 250;  //print this often (ms)
int count = 0;
elapsedMillis curTime;
elapsedMillis sinceSample;
//elapsedMillis sinceOutputChange;
elapsedMillis sincePrint;

//misc. other vars
String incomingSerial = "";

//OPTION SELECT VARS
bool printStats = true;
bool precisionMode = false;
bool takeSerialInput = true;
bool autoTuning = false;
int printStyle = 0;  //decides what format print_stats() uses in its serial statements 0 = verbose/readable, 1 = csv, 2 = serial graph labels, ...



void setup() {
  pinMode(PIN_INPUT, INPUT);
  pinMode(RELAY_PIN, OUTPUT);

  //initialize the variables we're linked to
  setpoint = 45;
  myPID.SetOutputLimits(0, PWM_len);
  myPID.SetSampleTime(sampleTime);

  //turn the PID on
  myPID.SetMode(AUTOMATIC);


  Serial.begin(9600);
  while (!Serial && (millis() < 3000)) {}

  if (printStyle == 1) {  //header line for a csv formatted file output (prints to serial for now though)
    Serial.printf("loop, time (ms), input/1023, input (C), smoothed input over last %d samples, output\%, output(ms/%dms)", recentSamples, PWM_len);
  } else if (printStyle == 2) {  //if serial graph output is desired, print out header line to assign variables
    Serial.printf("");
  }
}

void loop() {

  if (sinceSample >= ((long unsigned int)sampleTime)) {  //consider implementing this check identically to the internal compute() check to make sure they synch better
    sinceSample -= sampleTime;                           //ALT IDEA: call compute every time, if it returns positive then sample and smooth the next val immediately, for the next triggered compute
                                                         // - essentially always be computing with the last sample and immediately grab the next data point. offset would be bad w long sample time though
                                                         //OR compute a few times around when it should trigger, and take readings each time but only keep the one from the successful compute
    //GATHER INPUT and convert and smooth it
    raw_input = analogRead(PIN_INPUT);
    input_C = inputToCelcius(raw_input);
    input = smoothInput(input_C);
    //Decide whether to use near or far optimized K values
    double gap = abs(setpoint - input);  //distance away from setpoint
    if (gap < 10 && !precisionMode) {    //we're close to setpoint, use conservative tuning parameters
      myPID.SetTunings(consKp, consKi, consKd);
      precisionMode = true;
    } else if (gap >= 10 && precisionMode) {
      //we're far from setpoint, use aggressive tuning parameters
      myPID.SetTunings(Kp, Ki, Kd);
      precisionMode = false;
    }
    myPID.Compute();  //CONSIDER: COMPUTE EVERY LOOP? or keep attached to sample rate
  }

  //DISPLAY PRINTOUTS
  if (printStats && (sincePrint >= ((long unsigned int)printRate))) {
    sincePrint -= printRate;
    printSerial();
  }

  //SCAN SERIAL USER INPUT DURING RUNTIME
  if (Serial.available()) {
    receiveSerial();
  }

  /************************************************
   * turn the output pin on/off based on pid output
   ************************************************/
  if (sinceSample >= (unsigned long)PWM_len) {  //time to shift the Relay Window
    sinceSample -= PWM_len;
  }
  if (sinceSample < output) digitalWrite(RELAY_PIN, HIGH);
  else digitalWrite(RELAY_PIN, LOW);

  count++;
  delay(1);
}



//helper method, just a simple celcius to fahrenheit conversion
double temp_convert(double celcius) {
  return (1.8 * celcius) + 32;
}

//get Hz measurement of a given interval, initially in ms. rounds down to an int, consider whether double is needed
int hzOf(int waitPeriod) {
  return 1000 / waitPeriod;
}

//get period measurement in ms from a given hz of occurences. also rounds down to an int, consider whether double is better
int periodOf(int hz) {
  return 1000 / hz;
}

//uses equation found through calibration to convert raw 0-3.3V --> 0-1023 --> ~0-100C
//MUST RECALIBRATE FOR ACCURACY
double inputToCelcius(double input) {
  double ret = (3.3 * input - 976.8968) / 6.0357;
  return ret;
}

//simple smoothing algo, keeps last x sensor measurements post-conversion to celcius, and averages them.
//CONSIDER: using a weighted average
double smoothInput(double newVal) {
  // remove last reading:
  smoothTtl = smoothTtl - readings[readIndex];
  // add recent input
  readings[readIndex] = newVal;
  // add the reading to the total:
  smoothTtl = smoothTtl + readings[readIndex];
  // advance to the next position in the array:
  readIndex++;
  if (readIndex >= recentSamples) {
    readIndex = 0;
  }
  // calculate the average:
  if (readings[readIndex] == 0) {
    smoothAvg = smoothTtl / readIndex;
  } else {
    smoothAvg = smoothTtl / recentSamples;
  }
  //average = ((0.7 * current_val_V) + (0.3 * (total - readings[readIndex])))/recentSamples; weighted average
  return smoothAvg;
}

bool printSerial(Stream target, int pStyle) {
  switch (printStyle) {
    case 0:  // verbose
      Serial.printf("printing on loop cycle: %d at time: %u\n", count, curTime / 1000);
      Serial.printf("setpoint: %.1f, raw input/1023: %d, input in celcius: %.1f,\
     smoothed input over last %d vals: %.1f \n",
                    setpoint, raw_input, input_C, recentSamples, smoothed_input);
      Serial.printf("raw input: %f", raw_input);
      Serial.printf(" input in Celcius: %f\n", input);
      Serial.printf("output out of %d: %f ", PWM_len, output);
      Serial.printf(" output as a percent: %f\%\n\n", output / PWM_len * 100.0);
      break;
    case 1:  // csv for analysis
      Serial.printf("%d, %ld, %d, %d, %.2f, %.2f, %.1f\n", count, curTime, raw_input, input_C, smoothed_input, output * 100.0 / PWM_len, output);
      break;
    case 2:  //serial graph format for live monitoring until a graph is implemented
      Serial.printf("Setpoint:%.2f, Seconds:%.2f, input(C):%.2f, Smoothed_Input(C):%.2f, output(\%):%.2f", setpoint, curTime / 1000.0, input_C, smoothed_input);
      break;
    default:
      Serial.printf("select valid print option: 0 for verbose, 1 for csv, 2 to save to csv?");
  }
  return;
}

String receiveSerial() {
  incomingSerial = String("");
  bool understood = false;
  while (Serial.available() && incomingSerial.length() < 64) {
    char charIn = Serial.read();
    incomingSerial += charIn;
    Serial.print(charIn);
  }
  Serial.printf("Full serial input received at time(s): %.1f\n", curTime / 1000.0);
  Serial.println(incomingSerial);

  if (incomingSerial.indexOf("Poggers") != -1) {
    Serial.println("I understand! Secret message received");
  } else {
    Serial.println("Did not hear the secret passcode.");
  }
  if (incomingSerial.length() > 1) {
    if (incomingSerial[0] == '-') {
      switch (incomingSerial[1]) {
        case 'a':
          ofstream myfile;
          myfile.open("example.txt");
          myfile << "Writing this to a file.\n";
          myfile.close();
          break;

        case 'b':
          Serial.println("INPUT -b RECIEVED");
          break;

        default:
          break;
      }
    }
  }
  return incomingSerial;
}

/*
         TODO: 
         consider making a struct for data points
         consider system for serving multiple clients and keeping track
*/