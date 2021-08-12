#include <TimeLib.h>

// Include Wire Library for I2C
#include <Wire.h>
 
// Include Adafruit Graphics & OLED libraries
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
 
// Include Adafruit AM2320 Temp Humid Library
 
// Reset pin not used but needed for library
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);


const int micPin =  0; 

int buttonState = 0;

const int BUTTON_PIN = 5; // the number of the pushbutton pin
const int SHORT_PRESS_TIME = 500; // 500 milliseconds

// Variables will change:
int lastState = LOW;  // the previous state from the input pin
int currentState;     // the current reading from the input pin
unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;

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
#include <ArduinoJson.h>
#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <ArduinoMqttClient.h>
#include <WiFiNINA.h> // change to #include <WiFi101.h> for MKR1000

#include "arduino_secrets.h"

/////// Enter your sensitive data in arduino_secrets.h
const char ssid[]        = SECRET_SSID;
const char pass[]        = SECRET_PASS;
const char broker[]      = SECRET_BROKER;
const char* certificate  = SECRET_CERTIFICATE;

WiFiClient    wifiClient;            // Used for the TCP socket connection
BearSSLClient sslClient(wifiClient); // Used for SSL/TLS connection, integrates with ECC508
MqttClient    mqttClient(sslClient);

unsigned long lastMillis = 0;

const int buzzer = 0;

void setup() {
  Serial.begin(115200);
  // ----------- buzzer settings ---------------
  pinMode(buzzer, OUTPUT);
  // ----------- screen settings ---------------
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(micPin, OUTPUT);
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

  // publish a message roughly every 5 seconds.
  currentState = digitalRead(BUTTON_PIN);

  if(lastState == HIGH && currentState == LOW)        // button is pressed
    pressedTime = millis();
  else if(lastState == LOW && currentState == HIGH) { // button is released
    releasedTime = millis();

    long pressDuration = releasedTime - pressedTime;

    if( pressDuration < SHORT_PRESS_TIME ){
      Serial.println("A short press is detected");
      digitalWrite(micPin, HIGH);
      delay(1000);
      displayblue();
      display.display();
      publishMessage();
    } else {
      digitalWrite(micPin, LOW);
      delay(1000);
      displayred();
      display.display();  
      
    }
  }

  // save the the last state
  lastState = currentState;

  
//  if (millis() - lastMillis > 10000) {
//    lastMillis = millis();
//
//    publishMessage();
//  }
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
  
  
  unsigned epoch = getTime() - 4*3600; //est time from utc
  
  
  StaticJsonDocument<256> doc;
  doc["item"] = "ASPIRIN";
  doc["taken"] = true;
  doc["quantity"] = 2;
  doc["patient_id"] = 3;
  doc["timestamp"] = String(month(epoch)) +"-"+ String(day(epoch))+ "-" + String(year(epoch)) + "," + String(hour(epoch)) +":"+ String(minute(epoch)) + ":"+ String(second(epoch));
  // Add an array.
  //
//  JsonArray data = doc.createNestedArray("data");
//  data.add(48.756080);
//  data.add(2.302038);
  //doc["data"]=data;
  // Generate the minified JSON and send it to the Serial port.
  //
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

  // use the Stream interface to print the contents
  while (mqttClient.available()) {
    Serial.print((char)mqttClient.read());
  }
  Serial.println();

  Serial.println();
}



// --------- screen functions ----------
void displayblue(){
  
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
  display.print("Location: BLUE");
  display.setCursor(0,10); 
  display.print("Name: ASPIRIN"); 
  display.setCursor(0,20); 
  display.print("Dosage: 2 pills"); 
  
}

void displayred(){
  
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
  display.print("Location: RED");
  display.setCursor(0,10); 
  display.print("Name: Ibuprofen"); 
  display.setCursor(0,20); 
  display.print("Dosage: 1 pill"); 
  
}



// ----------- buzzer ------------
void buzzer(){
  for (int i = 0; i < 3; i++){
    tone(buzzer, 400); // Send 0.4KHz sound signal...
    delay(400);        // ...for 1 sec
    noTone(buzzer);     // Stop sound...
    delay(800);        // ...for 1sec  
  }
}
