/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "timer.h"
#include <ArduinoOTA.h>
#include "arduino_secrets.h" 

// Replace the next variables with your SSID/Password combination
String ssid = SECRET_SSID;
String password = SECRET_PASS;


const int pwmMotorA = D1;
const int pwmMotorB = D2;
const int dirMotorA = D3;
const int dirMotorB = D4;

int motorSpeed = 500;

Timer timer;

String mqtt_server = MQTT_SERVER_IP;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(( char*)mqtt_server.c_str(), 1883);
  client.setCallback(callback);

  pinMode(pwmMotorA , OUTPUT);
  pinMode(pwmMotorB, OUTPUT);
  pinMode(dirMotorA, OUTPUT);
  pinMode(dirMotorB, OUTPUT);

  timer.setCallback(motorStopp);

  ArduinoOTA.setHostname("huhntuer");
  ArduinoOTA.begin();
}

void motorAuf()
{
  Serial.println("Activate A auf");
  digitalWrite(pwmMotorA, motorSpeed);
  digitalWrite(dirMotorA, LOW);
}

void motorZu()
{
  Serial.println("Activate A zu");
  digitalWrite(pwmMotorA, motorSpeed);
  digitalWrite(dirMotorA, HIGH);
}

void motorStopp()
{
  Serial.println("Motor aus");
  digitalWrite(pwmMotorA, 0);
  digitalWrite(dirMotorA, LOW);
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(( char*)ssid.c_str(), (char*)password.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  if (String(topic).startsWith("huhnerstall") ) {
    Serial.print("Changing output to ");
    if(messageTemp.substring(0,7)== "tuerauf"){
      Serial.println("on");
      timer.stop();
      motorStopp();
      motorAuf();
      Serial.println(messageTemp.substring(7).toInt());
      timer.setTimeout(messageTemp.substring(7).toInt());
      timer.start();
    }
    else if(messageTemp.substring(0,6)== "tuerzu"){
      Serial.println("off");
      timer.stop();
      motorStopp();
      motorZu();
      Serial.println(messageTemp.substring(6).toInt());
      timer.setTimeout(messageTemp.substring(6).toInt());
      timer.start();
    }
    else {
      Serial.println("Error");
      timer.stop();
      motorStopp();      
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266ClientHuhn")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("huhnerstall/5576162");
      client.subscribe("huhnerstall");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  timer.update();
  ArduinoOTA.handle();

}
