/*
  AWS IoT WiFi

  This sketch securely connects to an AWS IoT using MQTT over WiFi.
  It uses a private key stored in the ATECC508A and a public
  certificate for SSL/TLS authetication.

  It publishes a message every 5 seconds to arduino/outgoing
  topic and subscribes to messages on the arduino/incoming
  topic.

  The circuit:
  - Arduino MKR WiFi 1010 or MKR1000

  The following tutorial on Arduino Project Hub can be used
  to setup your AWS account and the MKR board:

  https://create.arduino.cc/projecthub/132016/securely-connecting-an-arduino-mkr-wifi-1010-to-aws-iot-core-a9f365

  This example code is in the public domain.
*/


// Include Time Library for epoch conversion
#include <TimeLib.h>
// Include Wire Library for I2C
#include <Wire.h>
// Include Adafruit Graphics & OLED libraries
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> 
// Reset pin not used but needed for library
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#include <ArduinoJson.h>
#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <ArduinoMqttClient.h>
#include <WiFiNINA.h> // change to #include <WiFi101.h> for MKR1000

#include "arduino_secrets.h"


/// ------------- SECRETS ---------------/// 
/////// Enter your sensitive data in arduino_secrets.h
const char ssid[]        = SECRET_SSID;
const char pass[]        = SECRET_PASS;
const char broker[]      = SECRET_BROKER;
const char* certificate  = SECRET_CERTIFICATE;
/// ------------- SECRETS ---------------/// 

/// ------------ Variables --------------/// 
/// DISPLAY variables
char destination[] = "xxxxxxxxxxxx" ;
const char* item;     
const char* location;    
int quantity;

/// BUZZER variables
const int buzzer = 0;
const int BUTTON_PIN = 5; // the number of the pushbutton pin
const int SHORT_PRESS_TIME = 500; // 500 milliseconds

/// BUTTON variables
int buttonState = 0;
int lastState = LOW;  // the previous state from the input pin
int currentState;     // the current reading from the input pin
unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;

/// WiFi and MQTT variables
WiFiClient    wifiClient;            // Used for the TCP socket connection
BearSSLClient sslClient(wifiClient); // Used for SSL/TLS connection, integrates with ECC508
MqttClient    mqttClient(sslClient); // Used for the Mqtt instances
unsigned long lastMillis = 0;










void setup() {
  Serial.begin(115200);
  // ----------- buzzer and button settings ---------------
  pinMode(buzzer, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // ----------- screen settings ---------------
  // Start Wire library for I2C
  Wire.begin();
  // initialize OLED with I2C addr 0x3C
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  // ------------ network settings ------------
  while (!Serial);

  if (!ECCX08.begin()) {
    Serial.println("No ECCX08 present!");
    while (1);
  }

  // Set a callback to get the current time
  // used to validate the servers certificate
  ArduinoBearSSL.onGetTime(getTime);

  // Set the ECCX08 slot to use for the private key
  // and the accompanying public certificate for it
  sslClient.setEccSlot(0, certificate);

  // Optional, set the client id used for MQTT,
  // each device that is connected to the broker
  // must have a unique client id. The MQTTClient will generate
  // a client id for you based on the millis() value if not set
  //
  // mqttClient.setId("clientId");

  // Set the message callback, this function is
  // called when the MQTTClient receives a message
  mqttClient.onMessage(onMessageReceived);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!mqttClient.connected()) {
    // MQTT client is disconnected, connect
    connectMQTT();
  }

  // poll for new MQTT messages and send keep alives
  mqttClient.poll();

  
  currentState = digitalRead(BUTTON_PIN);
  if(lastState == HIGH && currentState == LOW)        // button is pressed
    pressedTime = millis();
  else if(lastState == LOW && currentState == HIGH) { // button is released
    releasedTime = millis();

    long pressDuration = releasedTime - pressedTime;
    
  // return to menu with LONG press and send confrimation with SHORT press
    if( pressDuration < SHORT_PRESS_TIME ){
      Serial.println("A short press is detected");
      delay(1000);
      
      publishMessage();
      buzzer_send();
      display_confirm();
      display.display();
    } else {
      delay(1000);
      display_proj();
      display.display();
    }
  }

  // save the the last state
  lastState = currentState;
}


unsigned long getTime() {
  // get the current time from the WiFi module  
  return WiFi.getTime();
}

void connectWiFi() {
  Serial.print("Attempting to connect to SSID: ");
  Serial.print(ssid);
  Serial.print(" ");

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(5000);
  }
  Serial.println();
  Serial.println("You're connected to the network");
  Serial.println();
}

void connectMQTT() {
  Serial.print("Attempting to MQTT broker: ");
  Serial.print(broker);
  Serial.println(" ");

  while (!mqttClient.connect(broker, 8883)) {
    // failed, retry
    Serial.print(mqttClient.connectError());
    delay(5000);
  }
  Serial.println();
  Serial.println("You're connected to the MQTT broker");
  Serial.println();

  // subscribe to a topic
  mqttClient.subscribe("arduino/incoming");
}

void publishMessage() {
  Serial.println("Publishing message");
  unsigned epoch = getTime() - 4*3600; // EST time conversion from UTC 

  // serialzation of JSON starts
  StaticJsonDocument<256> doc;
  doc["patient_id"] = 3;
  doc["item"] = destination;
  doc["taken"] = true;
  doc["quantity"] = quantity;
  doc["timestamp"] = String(month(epoch)) +"-"+ String(day(epoch))+ "-" + String(year(epoch)) + "," + String(hour(epoch)) +":"+ String(minute(epoch)) + ":"+ String(second(epoch));

  char out[128];
  int b =serializeJson(doc, out);
  Serial.print("bytes = ");
  Serial.println(b,DEC);
  mqttClient.beginMessage("arduino/outgoing");
  mqttClient.print(out);
  mqttClient.endMessage();
}

void onMessageReceived(int messageSize) {
  // we received a message, print out the topic and contents
  Serial.print("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");

  // deserialization of JSON start
  StaticJsonDocument<1024> doc;
  deserializeJson(doc, mqttClient);
  item          = doc["item"];
  location      = doc["location"];
  quantity      = doc["quantity"];
  Serial.println(item);
  Serial.println(location);
  Serial.println(quantity);

  displayMed(item, location, quantity);
  display.display();
  buzzer_recv();
  Serial.println();
}



// --------- screen functions ----------

void displayMed(const char* item, const char* loc, int quan){
  
  // Delay to allow sensor to stabalize
  delay(2000);
  // Clear the display
  display.clearDisplay();
  //Set the color - always use white despite actual display color
  display.setTextColor(WHITE);
  //Set the font size
  display.setTextSize(1);
  //Set the cursor coordinates
  display.setCursor(0,0);
  display.print("Location: " + String(loc));
  display.setCursor(0,10); 
  display.print("Name: " + String(item)); 
  memcpy(destination, item, sizeof(destination));
  display.setCursor(0,20); 
  display.print("Dosage: " + String(quan) +  " pills"); 
  
}


void display_confirm(){
 
  // Delay to allow sensor to stabalize
  delay(2000);
  // Clear the display
  display.clearDisplay();
  //Set the color - always use white despite actual display color
  display.setTextColor(WHITE);
  //Set the font size
  display.setTextSize(1);
  //Set the cursor coordinates
  display.setCursor(0,0);
  display.print("Confirmation");
  display.setCursor(0,10); 
  display.print("Name:" + String(destination)); 
  display.setCursor(0,20); 
  display.print("Done"); 
  
}

void display_proj(){
  
  // Delay to allow sensor to stabalize
  delay(2000);
  // Clear the display
  display.clearDisplay();
  //Set the color - always use white despite actual display color
  display.setTextColor(WHITE);
  //Set the font size
  display.setTextSize(1);
  //Set the cursor coordinates
  display.setCursor(0,0);
  display.print("493 Project");
  display.setCursor(0,10); 
  display.print("Medication Box"); 
  display.setCursor(0,20); 
  display.print("Group 95"); 
  
}


// ----------- buzzer ------------
void buzzer_recv(){
  for (int i = 0; i < 3; i++){
    tone(buzzer, 400); // Send 0.4KHz sound signal...
    delay(400);        // ...for 1 sec
    noTone(buzzer);     // Stop sound...
    delay(400);        // ...for 1sec  
  }
}

void buzzer_send(){
  for (int i = 0; i < 1; i++){
    tone(buzzer, 700); // Send 0.7KHz sound signal...
    delay(400);        // ...for 1 sec
    noTone(buzzer);     // Stop sound...
    delay(400);        // ...for 1sec  
  }
}
