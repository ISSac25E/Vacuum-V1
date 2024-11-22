////////////////////////////////////////////////////////////
//MQTT AND NETWORK SETUP:

//WIFI Network Setup:
#define VACUUM_NETWORK_SSID         "*****"
#define VACUUM_NETWORK_PASSWORD     "*****"

//MQTT Server Setup:
#define VACUUM_MQTT_SERVER_IP       "10.0.0.100"
#define VACUUM_MQTT_PORT            1883
#define VACUUM_MQTT_USERNAME        "mq"
#define VACUUM_MQTT_PASSWORD        "*****"

//MQTT Client Setup:
#define VACUUM_MQTT_CLIENT_NAME     "Vacuum"

//OTA Setup:
#define VACUUM_OTA_USERNAME         "Vacuum"

////////////////////////////////////////////////////////////


//All Required Libraries:
#include <ESP8266WiFi.h>  //Make Sure ESP8266 Board Manager is Installed
#include <ESP8266mDNS.h>  //Make Sure ESP8266 Board Manager is Installed
#include <PubSubClient.h> //https://github.com/knolleary/pubsubclient
#include <ArduinoOTA.h>   //https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA

//All Required CORE Files:

//VACUUM_GYRO_INTER REV 1.0.1
//.h
#ifndef VACUUM_GYRO_INTER_h
#define VACUUM_GYRO_INTER_h

#include "Arduino.h"
//MPU5060_ESP REV 1.0.2
//.h
#ifndef MPU6050_ESP_h
#define MPU6050_ESP_h

#include "Arduino.h"
#include "Wire.h"

//Register/Values for MPU5060
#define MPU6050_ADDR         0x68
#define MPU6050_SMPLRT_DIV   0x19
#define MPU6050_CONFIG       0x1a
#define MPU6050_GYRO_CONFIG  0x1b
#define MPU6050_ACCEL_CONFIG 0x1c
#define MPU6050_PWR_MGMT_1   0x6b
#define MPU6050_TEMP_H       0x41
#define MPU6050_TEMP_L       0x42
#define MPU6050_FIFO_EN      0x23

class MPU6050_ESP {
  public:

    void Init(uint8_t PowerPin);

    void Run();
    void PowerOn();
    void PowerOff();
    bool Error() {
      return _MPU_Error;
    };

    //Raw Gyro, Accel, Temp Vals:
    int16_t RawGyro[3] = {0, 0, 0}; //X,Y,Z
    int16_t RawAccel[3] = {0, 0, 0}; //X,Y,Z
    uint16_t RawTemp = 0;

  private:
    //If we are Init
    bool _Init = false;

    //We only want to periodically configure the MPU:
    uint32_t _MPU_ConfigTimer = 0;

    //These Val are used to check if MPU is functioning properly:
    uint8_t _DataCompareCount = 0;
    int16_t _PrevRawGyro[3];
    int16_t _PrevRawAccel[3];

    //These are used to manage the power to the MPU
    uint8_t _PowerPin;
    bool _MPU_Power = false;
    bool _MPU_PowerCycle = false;
    uint32_t _MPU_PowerCycleTimer;

    //High if error with MPU
    bool _MPU_Error = true;

    //Use this to write to Individual MPU Registers
    void _WriteMPU6050(byte reg, byte data) {
      Wire.beginTransmission(MPU6050_ADDR);
      Wire.write(reg);
      Wire.write(data);
      Wire.endTransmission();
    }
};

//.cpp
//#include "MPU5060_ESP.h"
//#include "Arduino.h"

void MPU6050_ESP::Init(uint8_t PowerPin) {
  _Init = true;
  _DataCompareCount = 0;
  _MPU_Error = true;
  _PowerPin = PowerPin;
  this->PowerOn();
  Wire.begin();
  {
    _MPU_ConfigTimer = millis();
    this->_WriteMPU6050(MPU6050_SMPLRT_DIV, 0x00);
    this->_WriteMPU6050(MPU6050_CONFIG, 0x00);
    this->_WriteMPU6050(MPU6050_GYRO_CONFIG, 0x08);
    this->_WriteMPU6050(MPU6050_ACCEL_CONFIG, 0x00);
    this->_WriteMPU6050(MPU6050_PWR_MGMT_1, 0x01);
    this->_WriteMPU6050(MPU6050_FIFO_EN, 0xFF);
  }
}

void MPU6050_ESP::PowerOn() {
  if (_Init) {
    digitalWrite(_PowerPin, HIGH);
    pinMode(_PowerPin, OUTPUT);
    _MPU_Power = true;
  }
}

void  MPU6050_ESP::PowerOff() {
  if (_Init) {
    digitalWrite(_PowerPin, LOW);
    pinMode(_PowerPin, INPUT);
    _MPU_Power = false;
  }
}

void MPU6050_ESP::Run() {
  if (_Init) {
    if (_MPU_Power) {
      //If MPU Dissconnects and Reconnects, This will ensure it will keep working:
      //We only do this 10 times a second:
      if (millis() - _MPU_ConfigTimer >= 100) {
        _MPU_ConfigTimer = millis();
        this->_WriteMPU6050(MPU6050_SMPLRT_DIV, 0x00);
        this->_WriteMPU6050(MPU6050_CONFIG, 0x00);
        this->_WriteMPU6050(MPU6050_GYRO_CONFIG, 0x08);
        this->_WriteMPU6050(MPU6050_ACCEL_CONFIG, 0x00);
        this->_WriteMPU6050(MPU6050_PWR_MGMT_1, 0x01);
        this->_WriteMPU6050(MPU6050_FIFO_EN, 0xFF);
      }

      //Collect the Raw Data:
      Wire.beginTransmission(MPU6050_ADDR);
      Wire.write(0x3B); //Start of Raw Data Reg, Accel X,Y,Z TEMP, Gyro X,Y,Z: H-L
      Wire.endTransmission(); //default True, Release Line After Transmission
      Wire.requestFrom(MPU6050_ADDR, 14);
      for (uint8_t X = 0; X < 3; X++) {
        RawAccel[X] = ((Wire.read() << 8) | Wire.read());
      }
      RawTemp = ((Wire.read() << 8) | Wire.read());
      for (uint8_t X = 0; X < 3; X++) {
        RawGyro[X] = ((Wire.read() << 8) | Wire.read());
      }
      bool CompareMatch = true;
      for (uint8_t X = 0; X < 3; X++) {
        if (_PrevRawGyro[X] != RawGyro[X]) CompareMatch = false;
        if (_PrevRawAccel[X] != RawAccel[X]) CompareMatch = false;
        _PrevRawGyro[X] = RawGyro[X];
        _PrevRawAccel[X] = RawAccel[X];
      }
      if (CompareMatch) {
        _DataCompareCount++;
      }
      else {
        _DataCompareCount = 0;
        _MPU_Error = false;
      }
      if (_DataCompareCount >= 100) {
        _MPU_Error = true;
        _MPU_PowerCycle = true;
        this->PowerOff();
        _MPU_PowerCycleTimer = millis();
      }
    }
    else {
      if (_MPU_PowerCycle) {
        if (millis() - _MPU_PowerCycleTimer >= 100) {
          this->PowerOn();
          _MPU_PowerCycle = false;
          _DataCompareCount = 0;
        }
      }
    }
  }
  else _MPU_Error = true;
}

//Undefine All Defined Values to prevent problems in future Code:
#undef MPU6050_ADDR
#undef MPU6050_SMPLRT_DIV
#undef MPU6050_CONFIG
#undef MPU6050_GYRO_CONFIG
#undef MPU6050_ACCEL_CONFIG
#undef MPU6050_PWR_MGMT_1
#undef MPU6050_TEMP_H
#undef MPU6050_TEMP_L
#undef MPU6050_FIFO_EN

#endif


#ifndef VACUUM_GYRO_INTER_ACCEL_ORNT_FACT                 //FACTOR FOR ANGLE DETECTION, DEFAULT GOOD
#define VACUUM_GYRO_INTER_ACCEL_ORNT_FACT 500
#endif

#ifndef VACUUM_GYRO_INTER_ACCEL_ORNT_ANG                  //ANGLE AT WHICH ACCEL WILL ASSUME UPSIDE DOWN
#define VACUUM_GYRO_INTER_ACCEL_ORNT_ANG 4000
#endif

#ifndef VACUUM_GYRO_INTER_ACCEL_VIB_COMP_FACT              //COMP FACTOR FOR ACCEL VIBRATION DETECTION, DEFAULT IS ALREADY GOOD
#define VACUUM_GYRO_INTER_ACCEL_VIB_COMP_FACT 50
#endif

#ifndef VACUUM_GYRO_INTER_ACCEL_VIB_ABS_COMP_FACT          //COMP FACTOR FOR ACCEL VIBRATION DETECTION, DEFAULT IS ALREADY GOOD
#define VACUUM_GYRO_INTER_ACCEL_VIB_ABS_COMP_FACT 10
#endif

#ifndef VACUUM_GYRO_INTER_ACCEL_VIB_ABS_MV_AVG_THRES       //VIBRATION TRIGGER THRESHOLD
#define VACUUM_GYRO_INTER_ACCEL_VIB_ABS_MV_AVG_THRES 300
#endif

#ifndef VACUUM_GYRO_INTER_ACCEL_VIB_ON_CNT_THRES           //ON COUNT TRIGGER THRESHOLD
#define VACUUM_GYRO_INTER_ACCEL_VIB_ON_CNT_THRES 200
#endif

#ifndef VACUUM_GYRO_INTER_ACCEL_VIB_OFF_CNT_THRES          //OFF COUNT TRIGGER THRESHOLD
#define VACUUM_GYRO_INTER_ACCEL_VIB_OFF_CNT_THRES -600
#endif

class VAC_GYRO {
  public:

    VAC_GYRO(uint8_t Pin) {
      _MPU.Init(Pin);
    };

    //    void SetAccelOffset(int16_t X, int16_t Y, int16_t Z);
    //    void SetGyroOffset(int16_t X, int16_t Y, int16_t Z);

    //Calculates MPU Offsets, checks for Errors, returns True if Vac Moving:
    bool Run();

    //Return weather MPU is having a problem:
    bool Error() {
      return _MPU.Error();
    }

    //Offsets for Gyro and Accel:
    int16_t _AccelOffset[3] = {0, 0, 0};
    int16_t _GyroOffset[3] = {0, 0, 0};

    //To know if we are calculating offsets
    //This is also How you Trigger Calc Externally:
    bool CalcOffsets = false;

  private:
    //Create private Object For MPU:
    MPU6050_ESP _MPU;

};

//.cpp
//#include "VACUUM_GYRO_INTER.h"
//#include "Arduino.h"

//void VAC_GYRO::SetAccelOffset(int16_t X, int16_t Y, int16_t Z) {
//  _AccelOffset[0] = X;
//  _AccelOffset[1] = Y;
//  _AccelOffset[2] = Z;
//}
//void VAC_GYRO::SetGyroOffset(int16_t X, int16_t Y, int16_t Z) {
//  _GyroOffset[0] = X;
//  _GyroOffset[1] = Y;
//  _GyroOffset[2] = Z;
//}

bool VAC_GYRO::Run() {
  static int32_t CompAccel[3] = {0, 0, 0};
  static int32_t CompGyro[3] = {0, 0, 0};

  _MPU.Run();
  if (!_MPU.Error()) {
    bool OrientComp = true;
    //Calc Orientation
    {
      static int32_t CompAccelOrient[3] = {0, 0, 0};
      //We are using 500 because we only need orientation, not movment:
      for (uint8_t X = 0; X < 3; X++) {
        CompAccelOrient[X] -= (CompAccelOrient[X] / VACUUM_GYRO_INTER_ACCEL_ORNT_FACT);
        CompAccelOrient[X] += (_MPU.RawAccel[X] - _AccelOffset[X]);
        if (abs(CompAccelOrient[X] / VACUUM_GYRO_INTER_ACCEL_ORNT_FACT) > VACUUM_GYRO_INTER_ACCEL_ORNT_ANG) OrientComp = false;
      }
    }
    //Calc Accel Vibration and if its moving:
    bool VibrateComp;
    {
      static int32_t CompAccelAvg[3] = {0, 0, 0};
      static int32_t AccelAbsAvg[3] = {0, 0, 0};
      static int32_t PeakCount = 0;
      static bool MoveState = false;
      {
        CompAccelAvg[0] -= CompAccelAvg[0] / VACUUM_GYRO_INTER_ACCEL_VIB_COMP_FACT;
        CompAccelAvg[1] -= CompAccelAvg[1] / VACUUM_GYRO_INTER_ACCEL_VIB_COMP_FACT;
        CompAccelAvg[2] -= CompAccelAvg[2] / VACUUM_GYRO_INTER_ACCEL_VIB_COMP_FACT;

        CompAccelAvg[0] += _MPU.RawAccel[0];
        CompAccelAvg[1] += _MPU.RawAccel[1];
        CompAccelAvg[2] += _MPU.RawAccel[2];
      }
      {
        AccelAbsAvg[0] -= AccelAbsAvg[0] / VACUUM_GYRO_INTER_ACCEL_VIB_ABS_COMP_FACT;
        AccelAbsAvg[1] -= AccelAbsAvg[1] / VACUUM_GYRO_INTER_ACCEL_VIB_ABS_COMP_FACT;
        AccelAbsAvg[2] -= AccelAbsAvg[2] / VACUUM_GYRO_INTER_ACCEL_VIB_ABS_COMP_FACT;

        AccelAbsAvg[0] += abs((CompAccelAvg[0] / VACUUM_GYRO_INTER_ACCEL_VIB_COMP_FACT) - _MPU.RawAccel[0]);
        AccelAbsAvg[1] += abs((CompAccelAvg[1] / VACUUM_GYRO_INTER_ACCEL_VIB_COMP_FACT) - _MPU.RawAccel[1]);
        AccelAbsAvg[2] += abs((CompAccelAvg[2] / VACUUM_GYRO_INTER_ACCEL_VIB_COMP_FACT) - _MPU.RawAccel[2]);
      }
      int32_t MoveAvg = (((AccelAbsAvg[0] / VACUUM_GYRO_INTER_ACCEL_VIB_ABS_COMP_FACT) +
                          (AccelAbsAvg[1] / VACUUM_GYRO_INTER_ACCEL_VIB_ABS_COMP_FACT) +
                          (AccelAbsAvg[2] / VACUUM_GYRO_INTER_ACCEL_VIB_ABS_COMP_FACT)) / 3);
      if (MoveAvg > VACUUM_GYRO_INTER_ACCEL_VIB_ABS_MV_AVG_THRES) {
        if (PeakCount < 0) PeakCount = 0;
        else if (PeakCount < 10000)PeakCount++;
      }
      else {
        if (PeakCount > 0)
          PeakCount = 0;
        else if (PeakCount > -10000) PeakCount--;
      }
      if (MoveState) {
        if (PeakCount < VACUUM_GYRO_INTER_ACCEL_VIB_OFF_CNT_THRES) {
          MoveState = false;
        }
      }
      else {
        if (PeakCount > VACUUM_GYRO_INTER_ACCEL_VIB_ON_CNT_THRES) {
          MoveState = true;
        }
      }
      VibrateComp = MoveState;
    }
    //Run Offset Calc Loop:
    if (CalcOffsets) {
      //Use for Calculating Offsets:
      static int32_t OffsetAccelAvg[3] = {0, 0, 0};
      static int32_t OffsetGyroAvg[3] = {0, 0, 0};
      static int16_t PrevOffsetAccelAvg[3] = {0, 0, 0};
      static int16_t PrevOffsetGyroAvg[3] = {0, 0, 0};
      static uint16_t OffsetCount = 0;
      static bool BeginOffsetCalc = false;

      if (!BeginOffsetCalc) {
        BeginOffsetCalc = true;
        for (uint8_t X = 0; X < 3; X++) {
          OffsetAccelAvg[X] = 0;
          OffsetGyroAvg[X] = 0;
          PrevOffsetAccelAvg[X] = 0;
          PrevOffsetGyroAvg[X] = 0;
        }
        OffsetCount = 0;
      }

      for (uint8_t X = 0; X < 3; X++) {
        //Use 30 for a more reactive and precise measurement:
        OffsetAccelAvg[X] -= (OffsetAccelAvg[X] / 30);
        OffsetAccelAvg[X] += _MPU.RawAccel[X];

        OffsetGyroAvg[X] -= (OffsetGyroAvg[X] / 30);
        OffsetGyroAvg[X] += _MPU.RawGyro[X];

        if (abs((OffsetAccelAvg[0] / 30) - PrevOffsetAccelAvg[0]) <= 5 &&
            abs((OffsetAccelAvg[1] / 30) - PrevOffsetAccelAvg[1]) <= 5 &&
            abs((OffsetAccelAvg[2] / 30) - PrevOffsetAccelAvg[2]) <= 5) {
          OffsetCount++;
          if (OffsetCount >= 200) {
            CalcOffsets = false;
            BeginOffsetCalc = false;
            for (uint8_t Y = 0; Y < 3; Y++) {
              _AccelOffset[Y] = (OffsetAccelAvg[Y] / 30);
            }
          }
        }
        else {
          OffsetCount = 0;
        }
        for (uint8_t Y = 0; Y < 3; Y++) {
          PrevOffsetAccelAvg[Y] = (OffsetAccelAvg[Y] / 30);
        }
      }
    }
    if (VibrateComp && OrientComp) return true;
    else return false;
  }
  else {
    return false;
  }
}

#endif


//ESP_BUTTON_INTERFACE REV 1.0.0
//.h
#ifndef ESP_BUTTON_INTERFACE_h
#define ESP_BUTTON_INTERFACE_h

#include "Arduino.h"

class ESP_PIN_DRIVER {
  public:

    ESP_PIN_DRIVER(uint8_t Pin);                //default PullUp, GPIO 16 PullDown
    ESP_PIN_DRIVER(uint8_t Pin, bool PullMode); //Mode = High, PullUp Enabled Exp. GPIO 16 PullDown

    //Returns Pin State and runs Button:
    bool Run();
    //Returns button State, does not run Button:
    bool ButtonState() {
      return _ButtonState;
    }
    void ButtonDebounce(uint16_t Debounce_us) {
      _ButtonDeBounceDelay = Debounce_us;
    }
    
  private:

    uint8_t _Pin;

    bool _ButtonState = false;
    bool _ButtonTest = false;
    uint32_t _ButtonTestTimer = 0;
    uint16_t _ButtonDeBounceDelay = 5000; //Default Value 5ms, 5000us
    //Function for Pin reading
    bool _PinRead() {
      return digitalRead(_Pin);
    }
};

class PIN_MACRO {
  public:
    //Returns true if StateChange
    bool Run(bool PinState);
    bool State() {
      return _MacroState;
    }
    uint32_t PrevInterval() {
      return _MacroPrevInterval;
    }
    uint32_t Interval() {
      return millis() - _MacroIntervalTimer;
    }
    void TimerReset() {
      _MacroIntervalTimer = millis();
    }
    void TimerSet(uint32_t TimerSet) {
      _MacroIntervalTimer = (millis() + TimerSet);
    }
  private:
    //Variables for Running Macro:
    bool _MacroState = false;
    uint32_t _MacroIntervalTimer = 0;
    uint32_t _MacroPrevInterval = 0;
};

//.cpp
//#include "ESP_BUTTON_INTERFACE.h"
//#include "Arduino.h"

ESP_PIN_DRIVER::ESP_PIN_DRIVER(uint8_t Pin) {
  //Defualt Do PullUp, if GPIO 16 do PullDown:
  _Pin = Pin;
  if (_Pin == 16) {
    pinMode(_Pin, INPUT_PULLDOWN_16);
  }
  else {
    pinMode(_Pin, INPUT_PULLUP);
  }
}

ESP_PIN_DRIVER::ESP_PIN_DRIVER(uint8_t Pin, bool PullMode) {
  //Mode High: PullUp, if GPIO 16 PullDown
  _Pin = Pin;
  if (PullMode) {
    if (_Pin == 16) {
      pinMode(_Pin, INPUT_PULLDOWN_16);
    }
    else {
      pinMode(_Pin, INPUT_PULLUP);
    }
  }
  else {
    pinMode(_Pin, INPUT);
  }
}

bool ESP_PIN_DRIVER::Run() {
  if (_ButtonTest) {
    if (micros() - _ButtonTestTimer >= _ButtonDeBounceDelay) {
      if (_PinRead() != _ButtonState) {
        _ButtonState = !_ButtonState;
        _ButtonTest = false;
      }
      else {
        _ButtonTest = false;
      }
    }
  }
  else {
    if (_PinRead() != _ButtonState) {
      _ButtonTest = true;
      _ButtonTestTimer = micros();
    }
  }
  return _ButtonState;
}

bool PIN_MACRO::Run(bool PinState) {
  if (PinState != _MacroState) {
    //State Change:
    _MacroState = PinState;
    _MacroPrevInterval = millis() - _MacroIntervalTimer;
    _MacroIntervalTimer = millis();
    return true;
  }
  else return false;
}

#endif

//LED_MACROS REV 4.1.1_1
//.h
#ifndef LED_MACROS_h
#define LED_MACROS_h

#include "Arduino.h"

class MACROS {
  public:
    bool Run();
    bool Ready()
    {
      return !_MacroRun;
    }

    void RST() {
      _MacroRun = false;
    }

    void Fade(uint8_t Target, uint8_t Frames);
    void Set(uint8_t Target, uint16_t Delay);
    void SetDelay(uint16_t Delay);
    void SetVal(uint8_t Val);
    void SetFPS(uint16_t FPS);
    uint8_t Val() {
      return _Val;
    }

  private:
    bool _MacroRun = false;

    //Which Macro is Running: false = Set, high = Fade
    uint32_t _Timer;
    uint16_t _Delay;

    //Rate For Frames. Default is 33ms for 30 FPS:
    uint16_t _Rate = 33;
    uint8_t _Val;
    uint8_t _Target;
    uint8_t _Increment;
};

class MACROS_BUILD {
  public:
    bool MacroChange() {
      bool Change = false;
      if(PrevMacro != Macro) {
        Change = true;
        PrevMacro = Macro;
      }
      return Change;
    }
    uint8_t PrevMacro = 0;
    uint8_t Macro = 0;
    uint8_t MacroStage = 0;
};

//.cpp
//#include "LED_MACROS.h"
//#include "Arduino.h"

void MACROS::SetFPS(uint16_t FPS) {
  if (FPS) {
    if (FPS > 1000) {
      _Rate = 1;
    }
    else {
      _Rate = 1000 / FPS;
    }
  }
  else {
    _Rate = 0;
  }
}

void MACROS::SetVal(uint8_t Val) {
  _MacroRun = false;
  if (Val != _Val)
    _Val = Val;
  _Target = _Val;
}

void MACROS::SetDelay(uint16_t Delay) {
  if (Delay) {
    _Delay = Delay;
    _Timer = millis();
    _MacroRun = true;
  }
  else {
    _MacroRun = false;
  }
}

void MACROS::Set(uint8_t Target, uint16_t Delay) {
  if (Delay) {
    _Delay = Delay;
    _Timer = millis();
    _MacroRun = true;
  }
  else {
    _MacroRun = false;
  }
  if (Target != _Val)
    _Val = Target;
  _Target = _Val;
}

void MACROS::Fade(uint8_t Target, uint8_t Frames) {
  if (Frames) {
    if (Target > _Val) {
      _MacroRun = true;
      _Delay = _Rate;
      _Increment = (Target - _Val) / Frames;
      if (!_Increment) _Increment = 1;
      _Target = Target;
      _Timer = millis();
    }
    else if (Target < _Val) {
      _MacroRun = true;
      _Delay = _Rate;
      _Increment = (_Val - Target) / Frames;
      if (!_Increment) _Increment = 1;
      _Target = Target;
      _Timer = millis();
    }
    else {
      _MacroRun = false;
    }
  }
}

bool MACROS::Run() {
  if (_MacroRun) {
    if ((millis() - _Timer) >= _Delay) {
      if (_Val < _Target) {
        //Fading Up
        uint8_t ValHold = _Val + _Increment;
        if (ValHold >= _Target || ValHold <= _Val) {
          // Done Fading
          ValHold = _Target;
          _MacroRun = false;
        }
        _Val = ValHold;
      }
      else if (_Val > _Target) {
        //Fading Down
        uint8_t ValHold = _Val - _Increment;
        if (ValHold <= _Target || ValHold >= _Val) {
          // Done Fading
          ValHold = _Target;
          _MacroRun = false;
        }
        _Val = ValHold;
      }
      else {
        //PWM Set Macro Done
        _MacroRun = false;
      }
      _Timer = millis();
    }
  }
  return !_MacroRun;
}
#endif



//NETWORK Objects:
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

//These Object will be used for Running The OUTPUT LED:
MACROS BUILTIN_LED_MACROS;
MACROS_BUILD BUILTIN_LED_BUILD;



//GLOBAL VARS:

//All Vars For Button:
bool BUTTON_OUT_STATE = false; //Used to Keep Track of Button State
uint8_t BUTTON_OUT_PIN = 0;    //D3: Pin For OUTPUT Pin For Triggering the Button On the VAC
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
uint32_t TimeCleaningOffset = 0;
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

  //Set BUILTIN LED ON:
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  //SetUp WiFi:
  WiFi.mode(WIFI_STA);
  WiFi.begin(VACUUM_NETWORK_SSID, VACUUM_NETWORK_PASSWORD);
  //  while (WiFi.status() != WL_CONNECTED) delay(500); //TODO: Is This Really Necessary?

  //SetUp OTA:
  ArduinoOTA.setHostname(VACUUM_OTA_USERNAME);
  ArduinoOTA.begin();

  //SetUp MQTT:
  CLIENT.setServer(VACUUM_MQTT_SERVER_IP, VACUUM_MQTT_PORT);
  CLIENT.setCallback(ParseMQTT);

  //Automatically Set MPU to Calibrate From the Start:
  VACUUM_MPU.CalcOffsets = true;

}

void loop() {
  MQTT_Run();
  LED_Run();
  BUTTON_Run();
  BUILTIN_LED_Run();
  MPU_Moving = VACUUM_MPU.Run();
  RunCleanTimer();
  RunVoltageCheck();
  // RunErrorButton();
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
      digitalWrite(BUTTON_OUT_PIN, LOW);
    }
    else {
      digitalWrite(BUTTON_OUT_PIN, HIGH);
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
            BUTTON_OUT.SetDelay(500);
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
            BUTTON_OUT.SetDelay(5000);
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
            BUTTON_OUT.SetDelay(400);
          }
          if (BUTTON_OUT.Run()) {
            switch (BUTTON_OUT_BUILD.MacroStage) {
              case 0:
                BUTTON_OUT_STATE = false;
                BUTTON_OUT.SetDelay(400);
                BUTTON_OUT_BUILD.MacroStage = 1;
                break;
              case 1:
                BUTTON_OUT_STATE = true;
                BUTTON_OUT.SetDelay(400);
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
            BUTTON_OUT_STATE = false;
            BUTTON_OUT_BUILD.MacroStage = 0;
            BUTTON_OUT.SetDelay(300);
          }
          if (BUTTON_OUT.Run()) {
            switch (BUTTON_OUT_BUILD.MacroStage) {
              case 0:
                BUTTON_OUT_STATE = true;
                BUTTON_OUT.SetDelay(300);
                BUTTON_OUT_BUILD.MacroStage = 1;
                break;
              case 1:
                BUTTON_OUT_STATE = false;
                BUTTON_OUT.SetDelay(300);
                BUTTON_OUT_BUILD.MacroStage = 2;
                break;
              case 2:
                BUTTON_OUT_STATE = true;
                BUTTON_OUT.SetDelay(300);
                BUTTON_OUT_BUILD.MacroStage = 3;
                break;
              case 3:
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

void BUILTIN_LED_Run() {
  static bool BUILTIN_LED_STATE = false;
  {
    if (BUILTIN_LED_STATE) {
      digitalWrite(LED_BUILTIN, LOW);
    }
    else {
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }
  {
    if (BUILTIN_LED_BUILD.MacroChange()) {
      BUILTIN_LED_STATE = false;
      BUILTIN_LED_BUILD.MacroStage = 0;
      BUILTIN_LED_MACROS.RST();
    }
    if (BUILTIN_LED_MACROS.Run()) {
      switch (BUILTIN_LED_BUILD.MacroStage) {
        case 0:
          BUILTIN_LED_STATE = true;
          BUILTIN_LED_MACROS.SetDelay(50);
          BUILTIN_LED_BUILD.MacroStage = 1;
          break;
        case 1:
          BUILTIN_LED_STATE = false;
          BUILTIN_LED_MACROS.SetDelay(50);
          BUILTIN_LED_BUILD.MacroStage = 2;
          break;
        case 2:
          BUILTIN_LED_STATE = true;
          BUILTIN_LED_MACROS.SetDelay(50);
          BUILTIN_LED_BUILD.MacroStage = 3;
          break;
        case 3:
          BUILTIN_LED_STATE = false;
          BUILTIN_LED_MACROS.SetDelay(5000);
          BUILTIN_LED_BUILD.MacroStage = 0;
          break;
      }
    }
  }
}

void RunCleanTimer() {
  //Update Cleaning Timer:
  {
    static bool TimingClean = false;
    if(TimingClean) {
      TimeCleaning = ((millis() - TimerCleanTimer) / 1000) + TimeCleaningOffset;
      if (!MPU_Moving) {
        TimingClean = false;
        TimeCleaningOffset = TimeCleaning;
      }
    }
    else {
      if (MPU_Moving) {
        TimingClean = true;
        TimerCleanTimer = millis();
      }
    }
  }
}

void RunErrorButton() {
  static uint32_t ErrorButtonTimeOut = 0;
  //Only Run Maximume every 5 Seconds:
  if (ORANGE_IN_STATE && millis() - ErrorButtonTimeOut >= 5000) {
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
    if (BatteryVoltage >= 2) BatteryVoltage -= 2;
    else BatteryVoltage = 0;
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

      //Publish TimeCleaning:
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

      //Publish ESP UP-TIME: Note, Does not Update on Change, to Avoid MQTT OverLoad
      {
        String TempStr = String((((millis() / 1000) / 60) / 60) / 24) + "d:" + String((((millis() / 1000) / 60) / 60) % 24) + "h:" + String(((millis() / 1000) / 60) % 60) + "m:" + String((millis() / 1000) % 60) + "s";
        char CharPayLoad[50];
        TempStr.toCharArray(CharPayLoad, TempStr.length() + 1);
        CLIENT.publish(VACUUM_MQTT_CLIENT_NAME"/UpTime", CharPayLoad);
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
      TimeCleaningOffset = 0;
      TimeCleaning = 0;
    }
    else if (NewPayLoad == "OFF") {
      TimerCleanTimer = millis();
      TimeCleaningOffset = 0;
      TimeCleaning = 0;
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
