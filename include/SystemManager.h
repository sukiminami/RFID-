/**
 * @file SystemManager.h
 * @brief 系统管理器头文件
 * @details 作为整个系统的核心调度器，负责初始化所有模块、
 *          管理主循环任务、处理网络回调和数据流转。
 */

#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>
#include "AppConfig.h"
#include "NetworkInterface.h"
#include "NetworkManager.h"

// 前向声明各模块类，避免循环依赖
class Rs485Comm;
class CommandHandler;
class DeviceControl;
class ConfigManager;
class OtaManager;
class CommandProcessor;
class RfidFrameParser;

/**
 * @class SystemManager
 * @brief 系统管理器类
 * @details 负责系统初始化、模块协调、事件响应和主循环调度，
 *          是整个应用的核心控制器。
 */
class SystemManager {
public:
    /**
     * @brief 构造函数
     * @details 初始化所有成员指针为空，计数器清零
     */
    SystemManager();
    
    /**
     * @brief 系统初始化入口
     * @return true-初始化成功，false-初始化失败
     * @details 按顺序初始化所有模块：RS485、设备控制、配置管理、
     *          OTA管理、命令处理器、网络模块、帧解析器
     */
    bool begin();
    
    /**
     * @brief 主循环函数
     * @details 在Arduino loop()中调用，执行各模块的周期性更新任务
     */
    void loop();
    
private:
    /**
     * @var _rs485
     * @brief RS485通信模块指针
     */
    Rs485Comm* _rs485;
    
    /**
     * @var _cmdHandler
     * @brief RFID命令处理器指针
     * @details 负责构建和发送RFID读写器命令帧
     */
    CommandHandler* _cmdHandler;
    
    /**
     * @var _networkManager
     * @brief 网络管理器指针
     * @details 管理Wi-Fi和蓝牙网络连接，处理MQTT通信
     */
    NetworkManager* _networkManager;
    
    /**
     * @var _networkConfig
     * @brief 网络配置参数结构体
     * @details 存储服务器地址、端口、MQTT参数等网络配置
     */
    NetworkConfig _networkConfig;
    
    /**
     * @var _deviceControl
     * @brief 设备控制模块指针
     * @details 管理喇叭、LED灯光、报警功能
     */
    DeviceControl* _deviceControl;
    
    /**
     * @var _configManager
     * @brief 配置管理器指针
     * @details 管理系统配置和白名单的读写存储
     */
    ConfigManager* _configManager;
    
    /**
     * @var _otaManager
     * @brief OTA固件升级管理器指针
     * @details 处理固件下载、更新和版本检查
     */
    OtaManager* _otaManager;
    
    /**
     * @var _commandProcessor
     * @brief MQTT命令处理器指针
     * @details 解析和执行从MQTT接收到的远程命令
     */
    CommandProcessor* _commandProcessor;
    
    /**
     * @var _rfidFrameParser
     * @brief RFID帧解析器指针
     * @details 解析RFID读写器返回的帧数据
     */
    RfidFrameParser* _rfidFrameParser;
    
    /**
     * @var _receiveBuffer
     * @brief RFID数据接收缓冲区
     * @details 用于临时存储从RS485接收到的原始数据
     */
    uint8_t _receiveBuffer[RECEIVE_BUFFER_SIZE];
    
    /**
     * @var _receiveLen
     * @brief 当前接收缓冲区中的有效数据长度
     */
    uint16_t _receiveLen;
    
    /**
     * @var _lastReceiveTime
     * @brief 上次接收到数据的时间戳
     * @details 用于判断帧接收是否超时
     */
    unsigned long _lastReceiveTime;
    
    /**
     * @var _lastHeartbeatTime
     * @brief 上次发送心跳包的时间戳
     */
    unsigned long _lastHeartbeatTime;
    
    /**
     * @brief 初始化网络配置参数
     * @details 从ConfigManager读取配置，填充NetworkConfig结构体
     */
    void initNetworkConfig();
    
    /**
     * @brief 网络状态变化回调处理函数
     * @param status 网络状态(已连接/已断开/连接中/错误)
     * @param type 网络类型(Wi-Fi/蓝牙/4G)
     * @param message 状态描述消息
     */
    void onNetworkStatusChanged(NetworkStatus status, NetworkType type, const char* message);
    
    /**
     * @brief 网络数据接收回调处理函数
     * @param data 接收到的数据指针
     * @param length 数据长度
     */
    void onNetworkDataReceived(const uint8_t* data, uint16_t length);
    
    /**
     * @brief 发送心跳包到MQTT服务器
     * @details 构建JSON格式心跳消息，包含当前时间戳
     */
    void sendHeartbeat();
    
    /**
     * @brief 处理RFID数据接收和解析
     * @details 从RS485读取数据，检测帧头，调用帧解析器处理
     */
    void processRfidData();
    
    /**
     * @brief 静态网络状态回调转发函数
     * @param status 网络状态
     * @param type 网络类型
     * @param message 状态消息
     * @param userData 用户数据指针(指向SystemManager实例)
     * @details 将静态回调转换为成员函数调用
     */
    static void staticNetworkStatusCallback(NetworkStatus status, NetworkType type, const char* message, void* userData);
    
    /**
     * @brief 静态网络数据回调转发函数
     * @param data 数据指针
     * @param length 数据长度
     * @param userData 用户数据指针(指向SystemManager实例)
     * @details 将静态回调转换为成员函数调用
     */
    static void staticNetworkDataCallback(const uint8_t* data, uint16_t length, void* userData);
};

/**
 * @var networkReady
 * @brief 全局网络就绪标志
 * @details 用于判断网络是否已连接，供各模块访问
 */
extern bool networkReady;

#endif