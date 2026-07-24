#include "DeviceControl.h"

DeviceControl::DeviceControl()
    : _buzzerEnabled(false), _redLEDEnabled(false), _blueLEDEnabled(false),
      _alarmActive(false), _blinkState(false),
      _alarmStartTime(0), _lastBlinkTime(0) {
    memset(_alarmMessage, 0, sizeof(_alarmMessage));
}

void DeviceControl::begin() {
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_BLUE_PIN, OUTPUT);
    
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_RED_PIN, LOW);
    digitalWrite(LED_BLUE_PIN, LOW);
    
    Serial0.println("[设备控制] 喇叭和灯光模块初始化完成");
}

void DeviceControl::playAlarm(const char* message) {
    Serial0.printf("[设备控制] 播放报警语音: %s\n", message);
    
    digitalWrite(BUZZER_PIN, HIGH);
    _buzzerEnabled = true;
    
    digitalWrite(LED_RED_PIN, HIGH);
    digitalWrite(LED_BLUE_PIN, HIGH);
    _redLEDEnabled = true;
    _blueLEDEnabled = true;
    
    _alarmActive = true;
    _alarmStartTime = millis();
    _blinkState = true;
    _lastBlinkTime = millis();
    
    strncpy(_alarmMessage, message, sizeof(_alarmMessage) - 1);
}

void DeviceControl::stopAlarm() {
    Serial0.println("[设备控制] 停止报警");
    
    digitalWrite(BUZZER_PIN, LOW);
    _buzzerEnabled = false;
    
    digitalWrite(LED_RED_PIN, LOW);
    digitalWrite(LED_BLUE_PIN, LOW);
    _redLEDEnabled = false;
    _blueLEDEnabled = false;
    
    _alarmActive = false;
    _blinkState = false;
    
    memset(_alarmMessage, 0, sizeof(_alarmMessage));
}

void DeviceControl::triggerAlarm(const char* epc) {
    Serial0.printf("[设备控制] 触发报警 - 检测到标签: %s\n", epc);
    
    char message[64];
    snprintf(message, sizeof(message), "检测到老人标签 %s", epc);
    
    playAlarm(message);
}

void DeviceControl::update() {
    if (_alarmActive) {
        unsigned long now = millis();
        
        if (now - _alarmStartTime >= ALARM_DURATION) {
            stopAlarm();
            return;
        }
        
        if (now - _lastBlinkTime >= LED_BLINK_INTERVAL) {
            _lastBlinkTime = now;
            _blinkState = !_blinkState;
            
            digitalWrite(LED_RED_PIN, _blinkState ? HIGH : LOW);
            digitalWrite(LED_BLUE_PIN, _blinkState ? LOW : HIGH);
        }
    }
}

bool DeviceControl::isAlarmActive() {
    return _alarmActive;
}

void DeviceControl::setBuzzer(bool enable) {
    digitalWrite(BUZZER_PIN, enable ? HIGH : LOW);
    _buzzerEnabled = enable;
}

void DeviceControl::setLED(bool red, bool blue) {
    digitalWrite(LED_RED_PIN, red ? HIGH : LOW);
    digitalWrite(LED_BLUE_PIN, blue ? HIGH : LOW);
    _redLEDEnabled = red;
    _blueLEDEnabled = blue;
}

void DeviceControl::blinkLED(bool enable) {
    if (enable) {
        _alarmActive = true;
        _alarmStartTime = millis();
        _lastBlinkTime = millis();
        _blinkState = true;
    } else {
        _alarmActive = false;
        digitalWrite(LED_RED_PIN, LOW);
        digitalWrite(LED_BLUE_PIN, LOW);
    }
}