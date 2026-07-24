#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H

#include <Arduino.h>

#define BUZZER_PIN 4
#define LED_RED_PIN 5
#define LED_BLUE_PIN 6

#define ALARM_DURATION 10000
#define LED_BLINK_INTERVAL 200

class DeviceControl {
public:
    DeviceControl();
    
    void begin();
    
    void playAlarm(const char* message);
    
    void stopAlarm();
    
    void triggerAlarm(const char* epc);
    
    void update();
    
    bool isAlarmActive();
    
    void setBuzzer(bool enable);
    
    void setLED(bool red, bool blue);
    
    void blinkLED(bool enable);
    
private:
    bool _buzzerEnabled;
    bool _redLEDEnabled;
    bool _blueLEDEnabled;
    bool _alarmActive;
    bool _blinkState;
    
    unsigned long _alarmStartTime;
    unsigned long _lastBlinkTime;
    
    char _alarmMessage[64];
};

#endif