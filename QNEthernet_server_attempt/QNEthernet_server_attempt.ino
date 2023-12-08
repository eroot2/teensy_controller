/*
  Teensy 4.1 WebServer Example

  This program reads 6 analog inputs on the Teensy 4.1 and sends the results to
  a webpage in a web browser

  Modify the IPAddress ip, sn and gw to match your own local network
  before downloading the program.
  
  Based on the WebServer example program
  This example uses the QNEthernet.h library
*/

#include <QNEthernet.h>
#include <SPI.h>
#include <SD.h>

using namespace qindesign::network;

//vars
String HTTP_req;  // stores the HTTP request
//for string literals for file names and html, consider a raw string

bool printMac = 0;
bool sendFile = true;
bool sdPresent = false; //don't manually select, it should detect whether the card is in
int selectRoute = 3;  //selects which ip info to use for
int count = 0;

IPAddress ip{ 192, 168, 1, 222 };  // Unique IP
IPAddress sn{ 255, 255, 255, 0 };  // Subnet Mask
IPAddress gw{ 192, 168, 1, 1 };    // Default Gateway


// Initialize the Ethernet server library with the IP address and port
// to use.  (port 80 is default for HTTP):
EthernetServer server(80);
//===============================================================================
//  Initialization
//===============================================================================
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 4000) {
    // Wait for Serial to start
  }
  Serial.printf("Starting QNEthernet Server...\r\n");
  pinMode(LED_BUILTIN, OUTPUT);


  if (selectRoute == 1) {                   //test router
    ip = IPAddress({ 192, 168, 10, 222 });  // Unique IP
    sn = IPAddress({ 255, 255, 255, 0 });   // Subnet Mask
    gw = IPAddress({ 192, 168, 10, 1 });    // Default Gateway
  } else if (selectRoute == 2) {            //home router
    ip = IPAddress({ 192, 168, 1, 222 });   // Unique IP
    sn = IPAddress({ 255, 255, 255, 0 });   // Subnet Mask
    gw = IPAddress({ 192, 168, 1, 1 });     // Default Gateway
  } else {                                  //direct connection, no router
    ip = IPAddress({ 169, 254, 1, 100 });   // Unique IP (try 192.168.0.2)
    sn = IPAddress({ 255, 255, 255, 0 });   // Subnet Mask
    gw = IPAddress({ 169, 254, 1, 1 });     // Default Gateway
  }

  // Initialize the library with a static IP so we can point a webpage to it
  if (!Ethernet.begin(ip, sn, gw)) {
    Serial.println("Failed to start Ethernet\n");
    return;
  }

  if (printMac) {
    // Just for fun we are going to fetch the MAC address out of the Teensy
    // and display it
    uint8_t mac[6];
    Ethernet.macAddress(mac);  // Retrieve the MAC address and print it out
    Serial.printf("MAC = %02x:%02x:%02x:%02x:%02x:%02x\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }
  // Show whether a cable is plugged in or not.
  Ethernet.onLinkState([](bool state) {
    Serial.printf("[Ethernet] Link %s\n", state ? "ON" : "OFF");
  });

  //checks whether sd card is in
  Serial.print("Initializing SD card...");
  // see if the card is present and can be initialized:
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    sdPresent = false;}
  else {
    sdPresent = true;
    Serial.println("card initialized.");}

  // start the webserver
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

  HTTP_req = "";
}


//===============================================================================
//  Main
//===============================================================================
void loop() {
  // listen for incoming clients
  Serial.printf("Loop #%d\n", count);
  EthernetClient client = server.available();
  if (client) {
    Serial.printf("new client %d\n", count);
    // an http request ends with a blank line
    bool currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (HTTP_req.length() < 120)
          HTTP_req += c;  // save the HTTP request 1 char at a time
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {


          // send a standard http response header
          // CONSIDER SWITCHING TO client.writeFully("_");
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          //client.println("Refresh: 5");         // refresh the page automatically every 5 sec
          client.println();
          if (HTTP_req.indexOf("ajaxrefresh") >= 0) {
            Serial.write("VALUE REFRESH AJAX REQUEST RECIEVED\n");
            // read switch state and analog input
            ajaxRequest(client);
            break;
          } else if (HTTP_req.indexOf("ledstatus") >= 0) {
            Serial.write("BUTTON REFRESH AJAX REQUEST RECIEVED\n");
            ledChangeStatus(client);
            break;
          }
          else{
          client.println("<!DOCTYPE HTML>");
          // Start HTML output to be displayed
          client.println("<html lang=\"en\">");
/////////////////////////////////////////////////////////////////////
            //client.println("<head>");
            client.println("<script>window.setInterval(function(){");
            client.println("nocache = \"&nocache=\" + Math.random() * 10;");
            client.println("var request = new XMLHttpRequest();");
            client.println("request.onreadystatechange = function() {");
            client.println("if (this.readyState == 4) {");
            client.println("if (this.status == 200) {");
            client.println("if (this.responseText != null) {");
            client.println("document.getElementById(\"analoge_data\")\
.innerHTML = this.responseText;");
            client.println("}}}}");
            client.println(
              "request.open(\"GET\", \"ajaxrefresh\" + nocache, true);");
            client.println("request.send(null);");
            client.println("}, 5000);");

            client.println("function changeLEDStatus() {");
            client.println(
              "nocache = \"&nocache=\" + Math.random() * 10;");
            client.println("var request = new XMLHttpRequest();");
            client.println("request.onreadystatechange = function() {");
            client.println("if (this.readyState == 4) {");
            client.println("if (this.status == 200) {");
            client.println("if (this.responseText != null) {");
            client.println("document.getElementById(\"led_status\")\
.innerHTML = this.responseText;");
            client.println("}}}}");
            client.println("request.open(\"GET\", \"?ledstatus=1\" + nocache, true);");
            client.println("request.send(null);");
            client.println("}");
            client.println("</script></head>");
            // output the value of each analog input pin
            client.print("<h1>Analogue Values</h1>");
            client.println("<div id=\"analoge_data\">Arduino analog input values loading.....</div>");
            client.println("<h1>Arduino LED Status</h1>");
            client.println("<div><span id=\"led_status\">");
            if(digitalRead(LED_BUILTIN) == 1)
             client.println("On");
            else
              client.println("Off");
            client.println("</span> | <button onclick=\"changeLEDStatus()\">Change Status</button> </div>");
/////////////////////////////////////////////////////////////////////
/*
          client.println("Teensy 4.1 WebServer<br />");
          // output the voltage value of each analog input pin
          for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
            int sensorReading = analogRead(analogChannel);
            // Convert reading to volts
            float volts = sensorReading * 3.3 / 1024.0;
            client.print("analog input ");
            client.print(analogChannel);
            client.print(" is ");
            client.print(volts);
            client.println("V<br />");
          }

          client.println("<button type=\"button\">Toggle LED!</button>");
          */
          client.println("</html>");
          client.flush();
          break;
          }
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    if (sendFile && sdPresent){useSD(client, "datalog.txt", nullptr, true);}
    // give the web browser time to receive the data
    count++;
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}

void ajaxRequest(EthernetClient client) {
  for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
    int sensorReading = analogRead(analogChannel);
    // Convert reading to volts
    float volts = sensorReading * 3.3 / 1024.0;
    client.print("analog input ");
    client.print(analogChannel);
    client.print(" is ");
    client.print(volts);
    client.println("<V\nbr />");
  }
}

void ledChangeStatus(EthernetClient client) {
  int state = !digitalRead(LED_BUILTIN);
  Serial.println(state);
  if (state == 1) {
    digitalWrite(LED_BUILTIN, LOW);
    client.print("OFF");
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
    client.print("ON");
  }
}

void sendTestData(EthernetClient& client) {
  for (int i = 0; i < 250; i++) {
    // 102-byte string (println appends CRLF)
    size_t written = client.println("1234567890"
                                    "1234567890"
                                    "1234567890"
                                    "1234567890"
                                    "1234567890"
                                    "1234567890"
                                    "1234567890"
                                    "1234567890"
                                    "1234567890"
                                    "1234567890");
    if (written != 102) {
      // This is not an error!
      Serial.println("Didn't write fully");
    }
  }
}

bool useSD(EthernetClient client, char[] fName, char[] inString = "", bool reading){
  Serial.print("Initializing SD card...");
  // see if the card is present and can be initialized:
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return false;}
  Serial.println("card initialized.");
  // open the file.
  File dataFile = SD.open(fName);
  // if the file is available, write to it:
  if (dataFile) {
  // ofstream myfile;
  // myfile.open ("example.txt");
  // myfile << "Writing this to a file.\n";
  // myfile.close();
    while (dataFile.available()) {
      Serial.write(dataFile.read());
    }
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening testsite.txt");
    return false;
  }
  return true;
}

void reset(){
  setup();
}