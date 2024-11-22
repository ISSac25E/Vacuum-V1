////////////////////////////////////////////////////////////
//MQTT AND NETWORK SETUP:

//ACCESS POINT:
#define VACUUM_AP_SSID              "Vacuum_AP"
#define VACUUM_AP_PASSWORD          "12345678"

//WIFI Network Setup:
#define VACUUM_NETWORK_SSID         "*****"
#define VACUUM_NETWORK_PASSWORD     "*****"

//MQTT Server Setup:
#define VACUUM_MQTT_SERVER_IP       "10.157.1.100"
#define VACUUM_MQTT_PORT            1883
#define VACUUM_MQTT_USERNAME        "mq"
#define VACUUM_MQTT_PASSWORD        "*****"

//MQTT Client Setup:
#define VACUUM_MQTT_CLIENT_NAME     "Vacuum"

//OTA Setup:
#define VACUUM_OTA_USERNAME         "Vacuum"

////////////////////////////////////////////////////////////


//All Required Libraries:
#include <ESP8266WiFi.h>       //Make Sure ESP8266 Board Manager is Installed
#include <ESP8266mDNS.h>       //Make Sure ESP8266 Board Manager is Installed
#include <ESP8266WebServer.h>  //Make Sure ESP8266 Board Manager is Installed
#include <PubSubClient.h>      //https://github.com/knolleary/pubsubclient
#include <ArduinoOTA.h>        //https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA

//All Required CORE Files:
#include "VACUUM_GYRO_INTER.h"
#include "ESP_BUTTON_INTERFACE.h"
#include "LED_MACROS.h"


//NETWORK Objects:
ESP8266WebServer AP_Server(80);      //AP WebServer At Port 80
WiFiClient ESP_CLIENT;
PubSubClient CLIENT(ESP_CLIENT);



//GYRO, OUTPUT, INPUT OBJECTS:

VAC_GYRO VACUUM_MPU(15);        //D8: Power Pin of The MPU

ESP_PIN_DRIVER ORANGE_IN(12);   //D6: INPUT Pin Of the Orange LED
ESP_PIN_DRIVER WHITE_IN(13);    //D7: INPUT Pin Of the White LED

PIN_MACRO ORANGE_IN_MACRO;
PIN_MACRO WHITE_IN_MACRO;

//These Objects will be used for running a Long, Double and Short Press for the VAC:
MACROS BUTTON_OUT;            //Not Really Needed Scince Button Is Bool ON And OFF, Not 8-BIT, Only Used for Timing
MACROS_BUILD BUTTON_OUT_BUILD;



//GLOBAL VARS:

//All Vars For Button:
bool BUTTON_OUT_STATE = false; //Used to Keep Track of Button State
uint8_t BUTTON_OUT_PIN = 14;    //D5: Pin For OUTPUT Pin For Triggering the Button On the VAC
//uint8_t BUTTON_OUT_PIN = LED_BUILTIN; //TEMP, Just For TESTING

//All LED Vars To Keep Track Of Their States:
uint8_t WHITE_IN_STATE = 0;    //State of White LED: 0 = OFF, 1 = ON, 2 = BLINK
uint8_t ORANGE_IN_STATE = 0;    //State of Orange LED: 0 = OFF, 1 = ON, 2 = BLINK

//If the Gyro Is Moving:
bool MPU_Moving = false;

//Used to Mesure the Battery Voltage:
uint16_t BatteryVoltage = 0;

//Keep Track Of How Long The Vacuum Is Cleaning For:
uint32_t TimeCleaning = 0;
uint32_t TimerCleanTimer;

//KeepCount Of How Many Errors We Encounter:
uint32_t ErrorButtonCount = 0;

//If Connected To Broker:
bool MQTT_Connected = false;


void setup() {

  //DEBUGGING:
  //  Serial.begin(115200);
  //  Serial.println("INIT");

  //Set ButtonPin to False and INPUT:
  digitalWrite(BUTTON_OUT_PIN, BUTTON_OUT_STATE);
  pinMode(BUTTON_OUT_PIN, INPUT);

  //SetUp WiFi:
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(VACUUM_AP_SSID, VACUUM_AP_PASSWORD);
  WiFi.begin(VACUUM_NETWORK_SSID, VACUUM_NETWORK_PASSWORD);
  //  while (WiFi.status() != WL_CONNECTED) delay(500); //TODO: Is This Really Necessary?

  //SetUp OTA:
  ArduinoOTA.setHostname(VACUUM_OTA_USERNAME);
  ArduinoOTA.begin();

  //SetUp MQTT:
  CLIENT.setServer(VACUUM_MQTT_SERVER_IP, VACUUM_MQTT_PORT);
  CLIENT.setCallback(ParseMQTT);

  //SetUp WebServer:
  AP_Server.on("/", Handle_AP_Root);
  AP_Server.on("/Submit", Handle_AP_Submit);
  AP_Server.onNotFound(Handle_AP_NotFound);
  AP_Server.begin();

  //Automatically Set MPU to Calibrate From the Start:
  VACUUM_MPU.CalcOffsets = true;

}

void loop() {
  AP_Server.handleClient(); //Run WebServer Handle
  MQTT_Run();
  LED_Run();
  BUTTON_Run();
  MPU_Moving = VACUUM_MPU.Run();
  RunCleanTimer();
  RunVoltageCheck();
  RunErrorButton();
  MQTT_UpdateStatus();

  ArduinoOTA.handle();
}

void MQTT_Run() {
  //MQTT:
  {
    //Make Sure MQTT is Connected and Run The Loop:
    CLIENT.loop();
    MQTT_Connected = CLIENT.connected();
    if (!MQTT_Connected) {
      //Try to Reconnect:
      if (CLIENT.connect(VACUUM_MQTT_CLIENT_NAME, VACUUM_MQTT_USERNAME, VACUUM_MQTT_PASSWORD)) {
        CLIENT.publish(VACUUM_MQTT_CLIENT_NAME"/checkIn", "Rebooted");
        CLIENT.subscribe(VACUUM_MQTT_CLIENT_NAME"/ShortPress");
        CLIENT.subscribe(VACUUM_MQTT_CLIENT_NAME"/DoublePress");
        CLIENT.subscribe(VACUUM_MQTT_CLIENT_NAME"/LongPress");
        CLIENT.subscribe(VACUUM_MQTT_CLIENT_NAME"/CleanTimerReset");
        CLIENT.subscribe(VACUUM_MQTT_CLIENT_NAME"/ErrorReset");
        CLIENT.subscribe(VACUUM_MQTT_CLIENT_NAME"/CalibrateGyro");
      }
    }
    //MQTT CheckIn Every 15 Seconds, 1 Min Max:
    {
      if (MQTT_Connected) {
        static uint32_t CheckInTimer = millis();
        if (millis() - CheckInTimer >= 15000) {
          CLIENT.publish(VACUUM_MQTT_CLIENT_NAME"/checkIn", "OK");
          CheckInTimer = millis();
        }
      }
    }
  }
}

void LED_Run() {
  //LED Update:
  {
    //ThreshHold(ms) For Blinking:
    const uint16_t LED_THRES = 2300;
    //WHITE LED:
    {
      if (WHITE_IN_MACRO.Run(WHITE_IN.Run())) {
        if (WHITE_IN_MACRO.PrevInterval() < LED_THRES) WHITE_IN_STATE = 2; //LED Blinking
        else {
          if (WHITE_IN_MACRO.State()) {
            WHITE_IN_STATE = 1;  //LED OFF
          }
          else {
            WHITE_IN_STATE = 0;  //LED ON
          }
        }
      }
      if (WHITE_IN_STATE == 2 && WHITE_IN_MACRO.Interval() >= LED_THRES)
        WHITE_IN_STATE = WHITE_IN_MACRO.State();
    }
    //ORANGE LED:
    {
      if (ORANGE_IN_MACRO.Run(ORANGE_IN.Run())) {
        if (ORANGE_IN_MACRO.PrevInterval() < LED_THRES) ORANGE_IN_STATE = 2; //LED Blinking
        else {
          if (ORANGE_IN_MACRO.State()) {
            ORANGE_IN_STATE = 1;  //LED OFF
          }
          else {
            ORANGE_IN_STATE = 0;  //LED ON
          }
        }
      }
      if (ORANGE_IN_STATE == 2 && ORANGE_IN_MACRO.Interval() >= LED_THRES)
        ORANGE_IN_STATE = ORANGE_IN_MACRO.State();
    }
  }
}

void BUTTON_Run() {
  //BUTTON Run:
  {

    //TEMP, Only For BuiltIn LED:
    //    if (BUTTON_OUT_STATE) {
    //      pinMode(BUTTON_OUT_PIN, OUTPUT);
    //      digitalWrite(BUTTON_OUT_PIN, LOW);
    //    }
    //    else {
    //      digitalWrite(BUTTON_OUT_PIN, HIGH);
    //      pinMode(BUTTON_OUT_PIN, INPUT);
    //    }

    if (BUTTON_OUT_STATE) {
      pinMode(BUTTON_OUT_PIN, OUTPUT);
      digitalWrite(BUTTON_OUT_PIN, HIGH);
    }
    else {
      digitalWrite(BUTTON_OUT_PIN, LOW);
      pinMode(BUTTON_OUT_PIN, INPUT);
    }


    switch (BUTTON_OUT_BUILD.Macro) {
      case 0:
        //Nothing
        if (BUTTON_OUT_BUILD.MacroChange()) {
          BUTTON_OUT_STATE = false;
        }
        break;
      case 1:
        //Short Press
        {
          if (BUTTON_OUT_BUILD.MacroChange()) {
            BUTTON_OUT_STATE = true;
            // BUTTON_OUT_BUILD.MacroStage = 0;  //Not Needed Since this Macro Only Has One Stage
            BUTTON_OUT.SetDelay(200);
          }
          if (BUTTON_OUT.Run()) {
            BUTTON_OUT_STATE = false;
            BUTTON_OUT_BUILD.Macro = 0;
          }
        }
        break;
      case 2:
        //Long Press
        {
          if (BUTTON_OUT_BUILD.MacroChange()) {
            BUTTON_OUT_STATE = true;
            // BUTTON_OUT_BUILD.MacroStage = 0;  //Not Needed Since this Macro Only Has One Stage
            BUTTON_OUT.SetDelay(1000);
          }
          if (BUTTON_OUT.Run()) {
            BUTTON_OUT_STATE = false;
            BUTTON_OUT_BUILD.Macro = 0;
          }
        }
        break;
      case 3:
        //Double Press
        {
          if (BUTTON_OUT_BUILD.MacroChange()) {
            BUTTON_OUT_STATE = true;
            BUTTON_OUT_BUILD.MacroStage = 0;
            BUTTON_OUT.SetDelay(200);
          }
          if (BUTTON_OUT.Run()) {
            switch (BUTTON_OUT_BUILD.MacroStage) {
              case 0:
                BUTTON_OUT_STATE = false;
                BUTTON_OUT.SetDelay(200);
                BUTTON_OUT_BUILD.MacroStage = 1;
                break;
              case 1:
                BUTTON_OUT_STATE = true;
                BUTTON_OUT.SetDelay(200);
                BUTTON_OUT_BUILD.MacroStage = 2;
                break;
              case 2:
                BUTTON_OUT_STATE = false;
                BUTTON_OUT_BUILD.Macro = 0;
                break;
            }
          }
        }
        break;
      case 4:
        //Error Reset Double Press
        {
          if (BUTTON_OUT_BUILD.MacroChange()) {
            BUTTON_OUT_STATE = true;
            BUTTON_OUT_BUILD.MacroStage = 0;
            BUTTON_OUT.SetDelay(200);
          }
          if (BUTTON_OUT.Run()) {
            switch (BUTTON_OUT_BUILD.MacroStage) {
              case 0:
                BUTTON_OUT_STATE = false;
                BUTTON_OUT.SetDelay(200);
                BUTTON_OUT_BUILD.MacroStage = 1;
                break;
              case 1:
                BUTTON_OUT_STATE = true;
                BUTTON_OUT.SetDelay(200);
                BUTTON_OUT_BUILD.MacroStage = 2;
                break;
              case 2:
                BUTTON_OUT_STATE = false;
                BUTTON_OUT_BUILD.Macro = 0;
                break;
            }
          }
        }
        break;
    }
  }
}

void RunCleanTimer() {
  //Update Cleaning Timer:
  {
    const uint16_t StartEndTimerThresh = 5000;  //Do 10 - 15 Seconds to be Safe

    static bool TimingClean = false;
    static bool VerifyingCleanStatus = false;
    static uint32_t VerifyingCleanStatusTimer = 0;
    if (TimingClean) {
      TimeCleaning = ((millis() - TimerCleanTimer) / 1000);
      if (!MPU_Moving || WHITE_IN_STATE != 1) {
        if (!VerifyingCleanStatus) {
          VerifyingCleanStatus = true;
          VerifyingCleanStatusTimer = millis();
        }
        else {
          if (millis() - VerifyingCleanStatusTimer >= StartEndTimerThresh) {
            TimingClean = false;
            VerifyingCleanStatus = false;
          }
        }
      }
      else {
        VerifyingCleanStatus = false;
      }
    }
    else {
      if (MPU_Moving && WHITE_IN_STATE == 1) {
        if (!VerifyingCleanStatus) {
          VerifyingCleanStatus = true;
          VerifyingCleanStatusTimer = millis();
        }
        else {
          if (millis() - VerifyingCleanStatusTimer >= StartEndTimerThresh) {
            TimingClean = true;
            TimerCleanTimer = millis();
            VerifyingCleanStatus = false;
          }
        }
      }
      else {
        VerifyingCleanStatus = false;
      }
    }
  }
}

void RunErrorButton() {
  static uint32_t ErrorButtonTimeOut = 0;
  //Only Run Maximume every 5 Seconds:
  if (ORANGE_IN_STATE && millis() - ErrorButtonTimeOut >= 10000) {
    ErrorButtonTimeOut = millis();
    //Run Error Button Press
    BUTTON_OUT_BUILD.Macro = 4;
    ErrorButtonCount++;
  }
}

void RunVoltageCheck() {
  //Update Voltage Reading:
  static uint32_t CheckVoltageTimer = millis();
  if (millis() - CheckVoltageTimer >= 500) {
    CheckVoltageTimer = millis();
    const uint16_t CompBatteryVoltageFact = 1;
    static uint32_t CompBatteryVoltage = 0;
    CompBatteryVoltage -= (CompBatteryVoltage / CompBatteryVoltageFact);
    CompBatteryVoltage += analogRead(A0);

    BatteryVoltage = map((CompBatteryVoltage / CompBatteryVoltageFact), 0, 1023, 0, 218);
  }
}

void MQTT_UpdateStatus() {
  if (MQTT_Connected) {
    static uint32_t UpdateStatusTimer = millis();
    bool UpdateStatus = false;

    static bool Prev_MPU_Moving = false;
    static bool Prev_MPU_Error = true;
    static bool Prev_MPU_CalcOffsets = false;

    static uint8_t PREV_ORANGE_IN_STATE = 0;  //State of LED: 0 = OFF, 1 = ON, 2 = BLINK
    static uint8_t PREV_WHITE_IN_STATE = 0;   //State of LED: 0 = OFF, 1 = ON, 2 = BLINK

    static uint16_t PREV_BatteryVoltage = 0;

    static uint32_t Prev_TimeCleaning = 0;

    static uint32_t Prev_ErrorButtonCount = 0;


    //Check If Change in One of the Vals:
    if (MPU_Moving != Prev_MPU_Moving ||
        VACUUM_MPU.Error() != Prev_MPU_Error  ||
        VACUUM_MPU.CalcOffsets != Prev_MPU_CalcOffsets  ||
        TimeCleaning != Prev_TimeCleaning ||
        ORANGE_IN_STATE != PREV_ORANGE_IN_STATE ||
        WHITE_IN_STATE != PREV_WHITE_IN_STATE ||
        BatteryVoltage != PREV_BatteryVoltage ||
        ErrorButtonCount != Prev_ErrorButtonCount)
      UpdateStatus = true;

    //Update When Change or Every 0.3 Seconds:
    if (UpdateStatus || millis() - UpdateStatusTimer >= 300) {
      UpdateStatusTimer = millis();
      if (VACUUM_MPU.Error()) CLIENT.publish(VACUUM_MQTT_CLIENT_NAME"/Status", "GYRO ERROR");
      else if (VACUUM_MPU.CalcOffsets) CLIENT.publish(VACUUM_MQTT_CLIENT_NAME"/Status", "GYRO CALIBRATING");
      else {
        if (WHITE_IN_STATE > 0) {
          if (MPU_Moving) {
            if (WHITE_IN_STATE == 1) {
              //Cleaning
              CLIENT.publish(VACUUM_MQTT_CLIENT_NAME"/Status", "CLEANING");
            }
            else {
              //Going Home
              CLIENT.publish(VACUUM_MQTT_CLIENT_NAME"/Status", "GOING HOME");
            }
          }
          else {
            if (WHITE_IN_STATE == 1) {
              //Ready
              CLIENT.publish(VACUUM_MQTT_CLIENT_NAME"/Status", "READY");
            }
            else {
              //Charging
              CLIENT.publish(VACUUM_MQTT_CLIENT_NAME"/Status", "CHARGING");
            }
          }
        }
        else {
          if (MPU_Moving)CLIENT.publish(VACUUM_MQTT_CLIENT_NAME"/Status", "MOVING");
          else CLIENT.publish(VACUUM_MQTT_CLIENT_NAME"/Status", "NOT MOVING");
        }
      }

      //Publish Battery Voltage:
      {
        String TempStr = String(BatteryVoltage / 10) + "." + String(BatteryVoltage % 10);
        char CharPayLoad[50];
        TempStr.toCharArray(CharPayLoad, TempStr.length() + 1);
        CLIENT.publish(VACUUM_MQTT_CLIENT_NAME"/BatteryVoltage", CharPayLoad);
      }

      //Publish TimeCleaning: TEMP, For Now:
      {
        String TempStr = String((TimeCleaning / 60) / 60) + "h:" + String((TimeCleaning / 60) % 60) + "m:" + String(TimeCleaning % 60) + "s";
        char CharPayLoad[50];
        TempStr.toCharArray(CharPayLoad, TempStr.length() + 1);
        CLIENT.publish(VACUUM_MQTT_CLIENT_NAME"/TimeCleaning", CharPayLoad);
      }

      //Publish ErrorCount:
      {
        String TempStr = String(ErrorButtonCount);
        char CharPayLoad[50];
        TempStr.toCharArray(CharPayLoad, TempStr.length() + 1);
        CLIENT.publish(VACUUM_MQTT_CLIENT_NAME"/ErrorCount", CharPayLoad);
      }

      {
        Prev_MPU_Moving = MPU_Moving;
        Prev_MPU_Error = VACUUM_MPU.Error();
        Prev_MPU_CalcOffsets = VACUUM_MPU.CalcOffsets;
        PREV_ORANGE_IN_STATE = ORANGE_IN_STATE;
        PREV_WHITE_IN_STATE = WHITE_IN_STATE;
        PREV_BatteryVoltage = BatteryVoltage;
        Prev_TimeCleaning = TimeCleaning;
        Prev_ErrorButtonCount = ErrorButtonCount;
      }
    }
  }
}

//Parse Incoming MQTT Messages:
void ParseMQTT(char* Topic, byte* PayLoad, uint16_t Length) {
  String NewTopic = Topic;
  PayLoad[Length] = '\0';
  String NewPayLoad = String((char*)PayLoad);
  int IntPayLoad = NewPayLoad.toInt();
  char CharPayLoad[50];
  NewPayLoad.toCharArray(CharPayLoad, NewPayLoad.length() + 1);

  //Compare Topic And PayLoad To Find Correct MSG:

  if (NewTopic == VACUUM_MQTT_CLIENT_NAME"/ShortPress") {
    if (NewPayLoad == "ON") {
      BUTTON_OUT_BUILD.Macro = 1;
    }
    else if (NewPayLoad == "OFF") {
      BUTTON_OUT_BUILD.Macro = 0;
    }
  }
  if (NewTopic == VACUUM_MQTT_CLIENT_NAME"/DoublePress") {
    if (NewPayLoad == "ON") {
      BUTTON_OUT_BUILD.Macro = 3;
    }
    else if (NewPayLoad == "OFF") {
      BUTTON_OUT_BUILD.Macro = 0;
    }
  }
  if (NewTopic == VACUUM_MQTT_CLIENT_NAME"/LongPress") {
    if (NewPayLoad == "ON") {
      BUTTON_OUT_BUILD.Macro = 2;
    }
    else if (NewPayLoad == "OFF") {
      BUTTON_OUT_BUILD.Macro = 0;
    }
  }
  if (NewTopic == VACUUM_MQTT_CLIENT_NAME"/ErrorReset") {
    if (NewPayLoad == "ON") {
      ErrorButtonCount = 0;
    }
    else if (NewPayLoad == "OFF") {
      ErrorButtonCount = 0;
    }
  }
  if (NewTopic == VACUUM_MQTT_CLIENT_NAME"/CleanTimerReset") {
    if (NewPayLoad == "ON") {
      TimerCleanTimer = millis();
      TimeCleaning = ((millis() - TimerCleanTimer) / 1000);
    }
    else if (NewPayLoad == "OFF") {
      TimerCleanTimer = millis();
      TimeCleaning = ((millis() - TimerCleanTimer) / 1000);
    }
  }
  if (NewTopic == VACUUM_MQTT_CLIENT_NAME"/CalibrateGyro") {
    if (NewPayLoad == "ON") {
      VACUUM_MPU.CalcOffsets = true;
    }
    else if (NewPayLoad == "OFF") {
      VACUUM_MPU.CalcOffsets = false;
    }
  }
}


//Functions for Handling WebServer:
void Handle_AP_Root() {
  String TempStr = (F("<!DOCTYPE html>\n<html>\n<body>\n\n<h1>ESP8266 WiFi RECOVERY</h1>\n\n<form action=\"/Submit\">\n  <label for=\"fname\">SSID:</label>\n  <input type=\"text\" id=\"SSID\" name=\"SSID\"><br><br>\n  <label for=\"lname\">Pasword:</label>\n  <input type=\"text\" id=\"PASS\" name=\"PASS\"><br><br>\n  <input type=\"submit\" value=\"Submit\">\n</form>\n\n<p>Click The \"Submit\" Button and the ESP will Reconnect to Recovery Network.\nThis is a Temporary Network Used For Recovery.</p>\n\n</body>\n</html>\n"));
  AP_Server.send(200, "text/html", TempStr);
}

void Handle_AP_Submit() {
  if (AP_Server.args() == 2) {
      String Temp_SSID = AP_Server.arg(0);
      String Temp_PASS = AP_Server.arg(1);
      WiFi.disconnect();
      WiFi.begin(Temp_SSID, Temp_PASS);
      String TempStr = ("<!DOCTYPE html>\n<html>\n<body>\n\n<h1>ESP8266 BEGINNING WiFi RECOVERY:</h1>\n\n<p>SSID: " + Temp_SSID + "</p>\n<p>Password: " + Temp_PASS + "</p>\n\n</body>\n</html>");
      AP_Server.send(200, "text/html", TempStr);
      return;
  }
  Handle_AP_NotFound();
}

void Handle_AP_NotFound() {
  AP_Server.send(404, "text/plain", "Not found");
}