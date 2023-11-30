#include <SPI.h>
#include <SD.h>
#include <QNEthernet.h>
String filename = "test.txt";  //MISSING: Save data to SD functionality, eventually store webpage html on sd card
File webPage;

double Kp = 2;                 // initialize Proportional Gain
double Ki = 0.5;               // initialize Integral Gain
const double interval = .1;  // wait ten miliseconds between cycles

double setpoint = 42.5;  // Change setpoint (C) as desired, goal temperature (assuming input is getting properly converted into a temp)

int current_val = 0;  //current analog input voltage, 0-1023
double current_val_C = 0;
double smooth_current_C = 0;
double error = 0;  //difference between current and target temps
double error_prev = 0;
unsigned long lastTime = 0;
double integral = 0;  //essentially sum of overall error over time, used for adjustment
//double derivative = 0;
double output = 0;    //initially formatted as a temperature to set the heating element to, then converted into a partial voltage
double output_unbounded = 0;

const int Input_Pin = 20;   // Change for respective pin
const int Output_Pin = 5;   // Change for respective pin
long count = 0;             // tick counter, used to only print once every [modBy] cycles
unsigned long curTime = 0;  //time tracker for records
const int modBy = 10;      //print outputs every -modBy- cycles
//CONSIDER switching to tracking the pwm exclusively by comparing millis readings instead of by count
const int pwmPeriod = 5000;      //similar to modby but applied to current time. Basically, one pwm cycle is this many ms long
const int recentSamples = 50;  //how many recent input values to save and average for smoothing
double readings[recentSamples];  // the readings from the analog input
int readIndex = 0;          // the index of the current reading
double smoothTtl = 0;              // the running total
double smoothAvg = 0;            // the average
bool heatOn = false;          //tracker for whether the heat control has been set to high. that way, it's only set once when it needs to change instead of every tick
//int modCheck = 0;
int printStyle = 0;  //decides what format print_stats() uses in its serial statements 0 = verbose/readable, 1 = csv, 2 = serial graph labels
int heatMode = 2;    //calibrate for different heating element outputs  0 = always off, 1 = always on, 2 = lamp, 3 = mini-furnace

//uses equation found through calibration to convert raw 0-3.3V --> 0-1023 --> ~0-100C
double inputToCelcius(double input) {
  double ret = (3.3*input - 976.8968)/6.0357;  
  return ret;
}

//simple smoothing algo, keeps last ten sensor measurements post conversion to celcius, and averages them 
//CONSIDER: using a weighted average
double smoothInput(){
  // remove last reading:
  smoothTtl = smoothTtl - readings[readIndex];
  // add recent input
  readings[readIndex] = current_val_C;
  // add the reading to the total:
  smoothTtl = smoothTtl + readings[readIndex];
  // advance to the next position in the array:
  readIndex = readIndex + 1;
  if (readIndex >= recentSamples) {
    readIndex = 0;
  }
  // calculate the average:
  if (readIndex != 0 && (readings[readIndex] == 0)){
  smoothAvg = smoothTtl / readIndex;
  }else{
  smoothAvg = smoothTtl / recentSamples;
  }
  //average = ((0.7 * current_val_V) + (0.3 * (total - readings[readIndex])))/recentSamples; weighted average
  return smoothAvg;
}

//helper method, just a simple celcius to fahrenheit conversion
double temp_convert(double celcius) {
  return (1.8 * celcius) + 32;
}

//prints out, in order, all relevant variables from current loop's scan & algo
void print_stats() {
  if (!(count % modBy)) {
    curTime = millis();
    switch (printStyle) {
      case 0:  //Verbose
        Serial.printf("Cycle %ld:\n", count);
        Serial.printf("Seconds passed:                      %lu\n", curTime / 1000);
        Serial.printf("Raw input val out of 1023:           %d\n", current_val);
        Serial.printf("Celcius input val is:                %.3f (= %.3f fahrenheit)\n", current_val_C, temp_convert(current_val_C));
        Serial.printf("Smoothed celcius input val is:       %.3f\n", smooth_current_C);
        //Serial.printf("Smoothed celcius arr total/%d  is: %.3f\n", recentSamples, smoothTtl);
        Serial.printf("Celcius error val is:                %.3f\n", error);
        Serial.printf("Integral:                            %.3f\n", integral);
        Serial.printf("Unbounded celcius output is:         %.3f\n", output_unbounded);
        Serial.printf("Percent output val is:               %.3f\n", output);
        break;
      case 1:  //CSV
        Serial.printf("%ld,", count);
        Serial.printf("%d,", curTime);
        Serial.printf("%.3d,", current_val);
        Serial.printf("%.3f, %.3f,", current_val_C, temp_convert(current_val_C));
        Serial.printf("%.3f,", error);
        Serial.printf("%.3f,", integral);
        Serial.printf("%.3f\n", output_unbounded);
        Serial.printf("%.3f, %.3f\n", output, temp_convert(output));
        break;
      case 2:  //Serial Graph
        Serial.printf("Cycle:%ld,", count);
        Serial.printf("Seconds:%lu,", curTime);
        Serial.printf("Raw_input:%d\n", current_val);
        Serial.printf("Celcius_input%.3f, Fahrenheit_input:%.3f\n", current_val_C, temp_convert(current_val_C));
        Serial.printf("Error:%.3f\n", error);
        Serial.printf("Integral:%.3f\n", integral);
        Serial.printf("Celcius_output_unbounded:%.3f\n", output_unbounded);
        Serial.printf("Celcius_output:%.3f, Fahrenheit_output:%.3f\n", output, temp_convert(output));
        break;
      default:
        Serial.println("Please select format for print statements of data :)");
        break;
    }
  }
  return;
}

void setup() {
  pinMode(Input_Pin, INPUT);
  pinMode(Output_Pin, OUTPUT);
  //pinMode(LED_BUILTIN, OUTPUT);
  for (int thisReading = 0; thisReading < recentSamples; thisReading++) { //initialize recent input val buffer for smoothing
    readings[thisReading] = 0;
  }
  Serial.begin(9600);
  while (!Serial) {}
  
  // initialize SD card
  // Serial.println("Checking SD card is accessible...");
  // if (!SD.begin(4)) {
  //   Serial.println("ERROR - SD card initialization failed!");
  //   return;    // init failed
  // }
  // Serial.println("SUCCESS - SD card initialized.");

  //prints out column headers for diff vars that are being recorded
  if (printStyle == 1 || printStyle == 2){
    Serial.print("Cycle, Time(mS), Input_Raw(/1023), Input_C, Input_F, Error(C), Integral, Output_C_Unbounded, Output_C, Output_F\n");
  }
}

void loop() {
  current_val = analogRead(Input_Pin);          // Value from 0 to 1023
  current_val_C = inputToCelcius(current_val);  //map(current_val, 0.0, 1023.0, -160, 400.0);  // convert to value from -160 to 400 C
  smooth_current_C = smoothInput();
  error = setpoint - smooth_current_C;
  integral = (integral + (error * interval));
  output = (Kp * error) + (Ki * integral);  //main PI equation
  output_unbounded = output;
  //restrict final output to real % bounds
  if (output < 0) output = 0;
  else if (output > 100) output = 100;
  //PWM at low Hz, different implementations for different output setups - 0 = always off, 1 = always on, 2 = lamp, 3 = mini-furnace
  switch (heatMode) {
    case 0:
        digitalWrite(Output_Pin, LOW);
        heatOn = false;
        break;
    case 1:
        digitalWrite(Output_Pin, HIGH);
        heatOn = true;
        break;
    case 2:  //lamp
      if ((curTime % pwmPeriod) < output * (pwmPeriod / 100)) {
        if(!heatOn){
          digitalWrite(Output_Pin, HIGH);
          heatOn = true;
        }
      } else {
        if (heatOn){
          digitalWrite(Output_Pin, LOW);
          heatOn = false;
        }
      }
      break;
    case 3:  //furnace
      if (curTime % 1000 < output) {   //effectively one tenth as often as it should be, since the furnace is so powerful
        if(!heatOn){
          digitalWrite(Output_Pin, HIGH);
          heatOn = true;
        }
      } else {
        if (heatOn){
          digitalWrite(Output_Pin, LOW);
          heatOn = false;
        }
      }
      break;
      default:
        Serial.println("Please select output heating element :)\n0 = always off, 1 = always on, 2 = lamp, 3 = mini-furnace");
      break;
  }
  // if (!(count%1000)){
  //   derivative = (error - error_prev) / (curTime - lastTime);
  //   error_prev = error;
  //   lastTime = curTime;
  // }
  count++;
  print_stats();
  delay(interval * 1000);
}
