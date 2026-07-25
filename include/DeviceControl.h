/**
 * @file DeviceControl.h
 * @brief 设备控制模块头文件
 * @details 负责控制硬件设备，包括喇叭、LED灯光和报警功能。
 */

#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H

#include <Arduino.h>

// ==================== 硬件引脚定义 ====================

/**
 * @def BUZZER_PIN
 * @brief 喇叭控制引脚
 * @note GPIO4，高电平触发响铃
 */
#define BUZZER_PIN 4

/**
 * @def LED_RED_PIN
 * @brief 红色LED控制引脚
 * @note GPIO5，高电平点亮
 */
#define LED_RED_PIN 5

/**
 * @def LED_BLUE_PIN
 * @brief 蓝色LED控制引脚
 * @note GPIO6，高电平点亮
 */
#define LED_BLUE_PIN 6

// ==================== 报警参数定义 ====================

/**
 * @def ALARM_DURATION
 * @brief 报警持续时间(毫秒)
 * @note 默认10000ms = 10秒，到期自动停止
 */
#define ALARM_DURATION 10000

/**
 * @def LED_BLINK_INTERVAL
 * @brief LED闪烁间隔(毫秒)
 * @note 默认200ms，报警时红蓝LED交替闪烁
 */
#define LED_BLINK_INTERVAL 200

/**
 * @class DeviceControl
 * @brief 设备控制类
 * @details 管理喇叭、LED灯光和报警功能，支持报警触发、停止、LED闪烁等操作。
 */
class DeviceControl {
public:
    /**
     * @brief 构造函数
     * @details 初始化所有控制状态为关闭，时间戳为0
     */
    DeviceControl();
    
    /**
     * @brief 初始化设备控制模块
     * @details 配置引脚为输出模式，初始化所有设备为关闭状态
     */
    void begin();
    
    /**
     * @brief 播放报警语音
     * @param message 报警消息文本
     * @details 启动喇叭和LED，记录报警开始时间
     */
    void playAlarm(const char* message);
    
    /**
     * @brief 停止报警
     * @details 关闭喇叭和LED，重置报警状态
     */
    void stopAlarm();
    
    /**
     * @brief 触发报警(标签检测)
     * @param epc 触发报警的标签EPC
     * @details 构建报警消息并调用playAlarm
     */
    void triggerAlarm(const char* epc);
    
    /**
     * @brief 更新设备状态(周期性调用)
     * @details 处理报警超时和LED闪烁逻辑
     */
    void update();
    
    /**
     * @brief 检查报警是否激活
     * @return true-报警中，false-未报警
     */
    bool isAlarmActive();
    
    /**
     * @brief 设置喇叭状态
     * @param enable true-开启，false-关闭
     */
    void setBuzzer(bool enable);
    
    /**
     * @brief 设置LED状态
     * @param red 红色LED状态(true-亮，false-灭)
     * @param blue 蓝色LED状态(true-亮，false-灭)
     */
    void setLED(bool red, bool blue);
    
    /**
     * @brief 控制LED闪烁
     * @param enable true-开始闪烁，false-停止闪烁
     */
    void blinkLED(bool enable);
    
private:
    /**
     * @var _buzzerEnabled
     * @brief 喇叭启用状态
     */
    bool _buzzerEnabled;
    
    /**
     * @var _redLEDEnabled
     * @brief 红色LED启用状态
     */
    bool _redLEDEnabled;
    
    /**
     * @var _blueLEDEnabled
     * @brief 蓝色LED启用状态
     */
    bool _blueLEDEnabled;
    
    /**
     * @var _alarmActive
     * @brief 报警激活状态
     */
    bool _alarmActive;
    
    /**
     * @var _blinkState
     * @brief LED闪烁状态(用于交替闪烁)
     */
    bool _blinkState;
    
    /**
     * @var _alarmStartTime
     * @brief 报警开始时间戳
     */
    unsigned long _alarmStartTime;
    
    /**
     * @var _lastBlinkTime
     * @brief 上次LED闪烁时间戳
     */
    unsigned long _lastBlinkTime;
    
    /**
     * @var _alarmMessage
     * @brief 当前报警消息文本
     */
    char _alarmMessage[64];
};

#endif