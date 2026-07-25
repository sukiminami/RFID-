/**
 * @file DeviceControl.cpp
 * @brief 设备控制模块实现文件
 * @details 实现DeviceControl类的所有成员函数，控制喇叭、LED和报警功能。
 */

#include "DeviceControl.h"

/**
 * @brief 构造函数
 * @details 使用初始化列表将所有控制状态初始化为关闭(false)，
 *          时间戳初始化为0，报警消息缓冲区清空
 */
DeviceControl::DeviceControl()
    : _buzzerEnabled(false), _redLEDEnabled(false), _blueLEDEnabled(false),
      _alarmActive(false), _blinkState(false),
      _alarmStartTime(0), _lastBlinkTime(0) {
    memset(_alarmMessage, 0, sizeof(_alarmMessage));
}

/**
 * @brief 初始化设备控制模块
 * @details 配置喇叭和LED引脚为输出模式，初始化所有设备为关闭状态(低电平)
 */
void DeviceControl::begin() {
    // 配置喇叭引脚为输出模式
    pinMode(BUZZER_PIN, OUTPUT);
    // 配置红色LED引脚为输出模式
    pinMode(LED_RED_PIN, OUTPUT);
    // 配置蓝色LED引脚为输出模式
    pinMode(LED_BLUE_PIN, OUTPUT);
    
    // 初始化所有设备为关闭状态
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_RED_PIN, LOW);
    digitalWrite(LED_BLUE_PIN, LOW);
    
    Serial0.println("[设备控制] 喇叭和灯光模块初始化完成");
}

/**
 * @brief 播放报警语音
 * @param message 报警消息文本
 * @details 启动喇叭(高电平)和LED(全亮)，记录报警开始时间和消息，
 *          初始化闪烁状态
 */
void DeviceControl::playAlarm(const char* message) {
    Serial0.printf("[设备控制] 播放报警语音: %s\n", message);
    
    // 开启喇叭
    digitalWrite(BUZZER_PIN, HIGH);
    _buzzerEnabled = true;
    
    // 开启LED(全亮)
    digitalWrite(LED_RED_PIN, HIGH);
    digitalWrite(LED_BLUE_PIN, HIGH);
    _redLEDEnabled = true;
    _blueLEDEnabled = true;
    
    // 设置报警状态
    _alarmActive = true;
    _alarmStartTime = millis();
    _blinkState = true;
    _lastBlinkTime = millis();
    
    // 保存报警消息
    strncpy(_alarmMessage, message, sizeof(_alarmMessage) - 1);
}

/**
 * @brief 停止报警
 * @details 关闭喇叭(低电平)和LED(全灭)，重置所有报警状态和时间戳，
 *          清空报警消息
 */
void DeviceControl::stopAlarm() {
    Serial0.println("[设备控制] 停止报警");
    
    // 关闭喇叭
    digitalWrite(BUZZER_PIN, LOW);
    _buzzerEnabled = false;
    
    // 关闭LED
    digitalWrite(LED_RED_PIN, LOW);
    digitalWrite(LED_BLUE_PIN, LOW);
    _redLEDEnabled = false;
    _blueLEDEnabled = false;
    
    // 重置报警状态
    _alarmActive = false;
    _blinkState = false;
    
    // 清空报警消息
    memset(_alarmMessage, 0, sizeof(_alarmMessage));
}

/**
 * @brief 触发报警(标签检测)
 * @param epc 触发报警的标签EPC
 * @details 构建报警消息"检测到老人标签 {EPC}"，然后调用playAlarm播放报警
 */
void DeviceControl::triggerAlarm(const char* epc) {
    Serial0.printf("[设备控制] 触发报警 - 检测到标签: %s\n", epc);
    
    // 构建报警消息
    char message[64];
    snprintf(message, sizeof(message), "检测到老人标签 %s", epc);
    
    // 播放报警
    playAlarm(message);
}

/**
 * @brief 更新设备状态(周期性调用)
 * @details 在主循环中调用，执行以下任务：
 *          1. 检查报警是否超时(超过ALARM_DURATION自动停止)
 *          2. 控制LED闪烁(红蓝交替)
 */
void DeviceControl::update() {
    // 仅在报警激活时执行更新
    if (_alarmActive) {
        unsigned long now = millis();
        
        // 检查报警是否超时
        if (now - _alarmStartTime >= ALARM_DURATION) {
            stopAlarm();
            return;
        }
        
        // 控制LED闪烁(红蓝交替)
        if (now - _lastBlinkTime >= LED_BLINK_INTERVAL) {
            _lastBlinkTime = now;
            // 切换闪烁状态
            _blinkState = !_blinkState;
            
            // 根据闪烁状态控制LED
            // 状态true: 红灯亮，蓝灯灭
            // 状态false: 红灯灭，蓝灯亮
            digitalWrite(LED_RED_PIN, _blinkState ? HIGH : LOW);
            digitalWrite(LED_BLUE_PIN, _blinkState ? LOW : HIGH);
        }
    }
}

/**
 * @brief 检查报警是否激活
 * @return true-报警中，false-未报警
 * @details 返回_alarmActive成员变量的当前状态
 */
bool DeviceControl::isAlarmActive() {
    return _alarmActive;
}

/**
 * @brief 设置喇叭状态
 * @param enable true-开启，false-关闭
 * @details 直接控制喇叭引脚电平
 */
void DeviceControl::setBuzzer(bool enable) {
    digitalWrite(BUZZER_PIN, enable ? HIGH : LOW);
    _buzzerEnabled = enable;
}

/**
 * @brief 设置LED状态
 * @param red 红色LED状态(true-亮，false-灭)
 * @param blue 蓝色LED状态(true-亮，false-灭)
 * @details 直接控制LED引脚电平
 */
void DeviceControl::setLED(bool red, bool blue) {
    digitalWrite(LED_RED_PIN, red ? HIGH : LOW);
    digitalWrite(LED_BLUE_PIN, blue ? HIGH : LOW);
    _redLEDEnabled = red;
    _blueLEDEnabled = blue;
}

/**
 * @brief 控制LED闪烁
 * @param enable true-开始闪烁，false-停止闪烁
 * @details 开启闪烁时初始化报警状态和时间戳，停止时关闭LED
 */
void DeviceControl::blinkLED(bool enable) {
    if (enable) {
        // 开启闪烁：设置报警激活状态
        _alarmActive = true;
        _alarmStartTime = millis();
        _lastBlinkTime = millis();
        _blinkState = true;
    } else {
        // 停止闪烁：关闭报警和LED
        _alarmActive = false;
        digitalWrite(LED_RED_PIN, LOW);
        digitalWrite(LED_BLUE_PIN, LOW);
    }
}