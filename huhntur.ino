#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "timer.h"
//#include <BetterOTA.h>
#include "arduino_secrets.h" 
#include <ESP8266WebServer.h>

ESP8266WebServer server(80);


// +------------------+  // Initial state when the door status is undefined
// |  DoorUndefined   |
// +------------------+
//         |
//         | "tuerauf" command with valid time
//         v
// +------------------+  // State when the door is open
// |    DoorOpen      |
// +------------------+
//         |
//         | "tuerzu" command with valid time
//         v
// +------------------+  // State when the door is closed
// |   DoorClosed     |
// +------------------+
//         |
//         | "resetdoor" command
//         v
// +------------------+  // State when the door status is reset to undefined
// |  DoorUndefined   |
// +------------------+






// Replace the next variables with your SSID/Password combination
String ssid = SECRET_SSID;
String password = SECRET_PASS;

int ENA = D7;
int IN1 = D1;
int IN2 = D2;

const int DoorUndefined = 0;
const int DoorOpen = 1;
const int DoorClosed = 2;

int DoorStatus = DoorUndefined;
//const int pwmMotorA = 5; //D1;
////const int pwmMotorB = D2;
//const int dirMotorA = 0; //D3;
////const int dirMotorB = D4;


const int pwmMotorA = D7;
int motorSpeed = 200;

Timer timer;
Timer pingTimer;

Timer openDoorTimer;
Timer closeDoorTimer;
int openDoorTime = 20*1000;
int closeDoorTime = 18*1000;
int autoDoorCycleTime = (24*60*60 + 30*60)*1000;

const int MAX_ALLOWED_TIME = 25*1000;

String mqtt_server = MQTT_SERVER_IP;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT); 
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(( char*)mqtt_server.c_str(), 1883);
  client.setCallback(callback);

//  pinMode(pwmMotorA , OUTPUT);
////  pinMode(pwmMotorB, OUTPUT);
//  pinMode(dirMotorA, OUTPUT);
////  pinMode(dirMotorB, OUTPUT);

  timer.setCallback(motorStopp);
  closeDoorTimer.setCallback(closeDoor);
  openDoorTimer.setCallback(openDoor);
  pingTimer.setCallback(pingMQTT);
  pingTimer.setInterval(60*1000); 
  pingTimer.start();
  //ArduinoOTA.setHostname("huhntuer");
  //ArduinoOTA.begin();
  //OTACodeUploader.begin(); // call this method if you want the code uploader to work

  server.on("/", handleRoot);
  server.begin();
  client.publish("huhnerstall/status", "started");
}

void motorAuf()
{
  Serial.println("Activate A auf");
  client.publish("huhnerstall/status", "motor auf");
  //digitalWrite(pwmMotorA, motorSpeed);
//  analogWrite(pwmMotorA,motorSpeed);
//  digitalWrite(dirMotorA, LOW);

  analogWrite(ENA, motorSpeed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
}

void motorZu()
{
  Serial.println("Activate A zu");
  client.publish("huhnerstall/status", "motor zu");
//  digitalWrite(pwmMotorA, motorSpeed);
//  analogWrite(pwmMotorA,motorSpeed);
//  digitalWrite(dirMotorA, HIGH);

  analogWrite(ENA, motorSpeed);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN1, LOW);

}

void motorStopp()
{
  Serial.println("Motor aus");
  client.publish("huhnerstall/status", "motor aus");
//  digitalWrite(pwmMotorA, 0);
//  digitalWrite(dirMotorA, LOW);
  analogWrite(ENA, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

String getFormattedTime(Timer& timer) {
  unsigned long timeRemaining = autoDoorCycleTime - timer.getElapsedTime();
  int hours = timeRemaining / 3600000;
  int minutes = (timeRemaining % 3600000) / 60000;
  return String(hours) + ":" + String(minutes);
}

void pingMQTT() 
{
  Serial.println("Ping...");
  String openDoorTimeStr = getFormattedTime(openDoorTimer);
  String closeDoorTimeStr = getFormattedTime(closeDoorTimer);
  String doorStatusText = getDoorStatus(DoorStatus);
  String message = "id=1,doorstatus=" + doorStatusText + ",openDoorTime=" + openDoorTimeStr + ",closeDoorTime=" + closeDoorTimeStr + ",openDoorTimerState=" + GetTimerState(openDoorTimer) + ",closeDoorTimerState=" + GetTimerState(closeDoorTimer);
  client.publish("huhnerstall/ping", message.c_str());
}

String getDoorStatus(int status) {
  switch (status) {
    case DoorOpen:
      return "Open (1)";
    case DoorClosed:
      return "Closed (2)";
    case DoorUndefined:
    default:
      return "Undefined (0)";
  }
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

  if (String(topic).startsWith("huhnerstall/cmd") ) {
    Serial.print("Changing output to ");
    if(messageTemp.substring(0,7)== "tuerauf"){
      Serial.println("on");
      timer.stop();
      motorStopp();
      int doorTime = messageTemp.substring(7).toInt();
      if (doorTime > 15*1000)  {
        if  (DoorStatus != DoorOpen) {
          openDoorTime = doorTime;
          client.publish("huhnerstall/log", String("door opening full with " + String(doorTime) + "ms").c_str());
          DoorStatus = DoorOpen;
          openDoorTimer.stop();
          motorAuf();
          Serial.println(doorTime);
          timer.setTimeout(doorTime);
          timer.start();
          //reset openDoorTime to 24h+30min
          openDoorTimer.stop();
          openDoorTimer.setTimeout(autoDoorCycleTime);
          openDoorTimer.start();
          client.publish("huhnerstall/log", String("set auto door opening in " + getFormattedTime(openDoorTimer) + "ms timer status " + GetTimerState(openDoorTimer)).c_str());
          return;
        }
        client.publish("huhnerstall/log", String("cmd to open door for " + String(doorTime) + "ms ignored, door is already open").c_str());
        return;
      } 
      client.publish("huhnerstall/log", String("door opening normal with " + String(doorTime) + "ms").c_str());
      motorAuf();
      Serial.println(doorTime);
      timer.setTimeout(doorTime);
      timer.start();
      return;
    }
    else if(messageTemp.substring(0,6)== "tuerzu"){
      Serial.println("off");
      timer.stop();
      motorStopp();
      int doorTime = messageTemp.substring(6).toInt();
      if (doorTime > 15*1000) {
        if (DoorStatus != DoorClosed) {
          closeDoorTime = doorTime;
          client.publish("huhnerstall/log", String("door closing full with " + String(doorTime) + "ms").c_str());
          motorZu();
          Serial.println(doorTime);
          timer.setTimeout(doorTime);
          timer.start();
          DoorStatus = DoorClosed;  
          closeDoorTimer.stop();
          closeDoorTimer.setTimeout(autoDoorCycleTime);
          closeDoorTimer.start();
          client.publish("huhnerstall/log", String("set auto door closing in " + getFormattedTime(closeDoorTimer) + "ms timer status " + GetTimerState(closeDoorTimer)).c_str());
          return;
        }
          client.publish("huhnerstall/log", String("cmd to close door for " + String(doorTime) + "ms ignored, door is already closed").c_str());
          return;
      }
      client.publish("huhnerstall/log", String("door closing normal with " + String(doorTime) + "ms").c_str());
      motorZu();
      Serial.println(doorTime);
      timer.setTimeout(doorTime);
      timer.start();
      return;
    } 
    else if (messageTemp.substring(0,9)== "resetdoor"){
      DoorStatus = DoorUndefined;
      client.publish("huhnerstall/log", "door status reset");
    } 
    else if (messageTemp.substring(0,10)== "resettimer"){
      openDoorTimer.stop();
      closeDoorTimer.stop();
      client.publish("huhnerstall/log", "timer reset");
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
      client.subscribe("huhnerstall/cmd");
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
  openDoorTimer.update();
  closeDoorTimer.update();
  pingTimer.update();
  //ArduinoOTA.handle();
  //BetterOTA.handle();
  server.handleClient();
}

void openDoor() {
  if (DoorStatus == DoorOpen) {
      client.publish("huhnerstall/log", "door already open, autoopen canceled");
      return;
  }
  if ((openDoorTime <= 0) || (openDoorTime > MAX_ALLOWED_TIME)) {
    client.publish("huhnerstall/alert", String("auto door opening with invalid time canceled, time was " + String(openDoorTime)).c_str());
    return;
  }
  client.publish("huhnerstall/alert", String("auto door opening with time " + String(openDoorTime)).c_str());
  motorStopp();
  motorAuf();
  Serial.println( "auto door opening with time " + String(openDoorTime));
  timer.setTimeout(openDoorTime);
  DoorStatus = DoorOpen;
  timer.start();
  openDoorTimer.stop();
  openDoorTimer.setTimeout(autoDoorCycleTime-30*60*1000);
  openDoorTimer.start();
  client.publish("huhnerstall/log", String("set auto door opening in " + getFormattedTime(openDoorTimer)).c_str());
}

void closeDoor() {
  if (DoorStatus == DoorClosed) {
    client.publish("huhnerstall/alert", "door already closed, auto close canceled");  
    return;
  }
  if ((closeDoorTime <= 0) || (closeDoorTime  > MAX_ALLOWED_TIME)) {
    client.publish("huhnerstall/alert", String("auto door closing with invalid time canceled, time was " + String(closeDoorTime)).c_str());
    return;
  }
  client.publish("huhnerstall/alert", String("auto door closing with time " + String(closeDoorTime)).c_str());
  motorStopp();
  motorZu();
  Serial.println( "auto door closing with time " + String(closeDoorTime));
  timer.setTimeout(closeDoorTime);
  DoorStatus = DoorClosed;
  timer.start();
  closeDoorTimer.stop();
  closeDoorTimer.setTimeout(autoDoorCycleTime-30*60*1000);
  closeDoorTimer.start();
  client.publish("huhnerstall/log", String("set auto door closing in " + getFormattedTime(closeDoorTimer)).c_str());
}



void handleRoot() {
  Serial.println("serve /");
  String openDoorTimeStr = getFormattedTime(openDoorTimer);
  String closeDoorTimeStr = getFormattedTime(closeDoorTimer);
  
  String html = "<html><body><h1>Huhnertuer V1</h1><p>Door status: " + getDoorStatus(DoorStatus) + "</p><p>Open door time: " 
  + String(openDoorTime) + "</p><p>Close door time: " + String(closeDoorTime) + "</p><p>Open door timer: " 
  + openDoorTimeStr + "</p><p>Close door timer: " + closeDoorTimeStr + "</p> "+
  "<p> CloseDoorTimerStatus "+ GetTimerState(closeDoorTimer)+"</p>"+
  "<p> OpenDoorTimerStatus "+ GetTimerState(openDoorTimer)+"</p>"+
  "</body></html>";
  server.send(200, "text/html", html);
}

String GetTimerState(Timer& timer) {
  if (timer.isRunning()) {
    return "Running";
  }
  if (timer.isStopped()) {
    return "Stopped";
  }
  if (timer.isPaused()) {
    return "Paused";
  }
  return "Unknown";
}