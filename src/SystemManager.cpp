/**
 * @file SystemManager.cpp
 * @brief 系统管理器实现文件
 * @details 实现SystemManager类的所有成员函数，包括系统初始化、
 *          主循环调度、网络回调处理和RFID数据处理。
 */

#include "SystemManager.h"
#include <ArduinoJson.h>
#include "rs485.h"
#include "CommandHandler.h"
#include "DeviceControl.h"
#include "ConfigManager.h"
#include "OtaManager.h"
#include "CommandProcessor.h"
#include "RfidFrameParser.h"

/**
 * @var networkReady
 * @brief 全局网络就绪标志，初始化为false
 * @details 在网络连接成功时设为true，断开时设为false
 */
bool networkReady = false;

/**
 * @brief 构造函数
 * @details 使用初始化列表将所有模块指针初始化为nullptr，
 *          接收相关计数器初始化为0
 */
SystemManager::SystemManager()
    : _rs485(nullptr), _cmdHandler(nullptr), _networkManager(nullptr),
      _deviceControl(nullptr), _configManager(nullptr),
      _otaManager(nullptr), _commandProcessor(nullptr), _rfidFrameParser(nullptr),
      _receiveLen(0), _lastReceiveTime(0), _lastHeartbeatTime(0) {
}

/**
 * @brief 系统初始化入口函数
 * @return true-初始化成功，false-RS485初始化失败
 * @details 按顺序初始化所有系统模块：
 *          1. RS485通信模块(串口2, 115200波特率)
 *          2. 设备控制模块(喇叭、LED)
 *          3. 配置管理器(加载保存的配置)
 *          4. OTA管理器(固件升级管理)
 *          5. RFID命令处理器(RS485命令构建)
 *          6. 网络管理器(Wi-Fi/MQTT连接)
 *          7. MQTT命令处理器(远程命令解析)
 *          8. RFID帧解析器(数据帧解析)
 */
bool SystemManager::begin() {
    // 打印系统启动信息
    Serial0.println("========================================");
    Serial0.println("  RFID网关系统");
    Serial0.println("========================================");
    
    // 1. 初始化RS485通信模块
    Serial0.println("[系统] 初始化RS485模块...");
    _rs485 = new Rs485Comm(&Serial2, UART_TX_PIN, UART_RX_PIN, UART_DE_PIN);
    if (!_rs485->begin(BAUD_RATE)) {
        Serial0.println("[系统] RS485初始化失败!");
        return false;
    }
    Serial0.println("[系统] RS485初始化完成");
    
    // 2. 初始化设备控制模块
    Serial0.println("[系统] 初始化设备控制模块...");
    _deviceControl = new DeviceControl();
    _deviceControl->begin();
    Serial0.println("[系统] 设备控制模块初始化完成");
    
    // 3. 初始化配置管理器
    Serial0.println("[系统] 初始化配置管理器...");
    _configManager = new ConfigManager();
    _configManager->begin();
    Serial0.println("[系统] 配置管理器初始化完成");
    
    // 4. 初始化OTA管理器
    Serial0.println("[系统] 初始化OTA管理器...");
    _otaManager = new OtaManager();
    _otaManager->begin();
    _otaManager->setCurrentVersion(FIRMWARE_VERSION);
    Serial0.printf("[系统] 当前固件版本: %s\n", FIRMWARE_VERSION);
    Serial0.println("[系统] OTA管理器初始化完成");
    
    // 5. 初始化RFID命令处理器
    Serial0.println("[系统] 初始化命令处理器...");
    _cmdHandler = new CommandHandler(_rs485);
    
    // 6. 初始化网络管理器
    Serial0.println("[系统] 初始化网络模块...");
    _networkManager = new NetworkManager();
    initNetworkConfig();  // 从配置加载网络参数
    
    // 设置OTA管理器的网络管理器引用(用于MQTT进度上报)
    _otaManager->setNetworkManager(_networkManager);
    
    if (!_networkManager->begin(&_networkConfig)) {
        Serial0.println("[系统] 网络管理器初始化失败");
    } else {
        Serial0.println("[系统] 网络管理器初始化成功");
        // 注册网络状态和数据回调函数
        _networkManager->setStatusCallback(staticNetworkStatusCallback, this);
        _networkManager->setDataCallback(staticNetworkDataCallback, this);
        Serial0.println("[系统] 开始连接网络...");
        _networkManager->connect();
    }
    
    // 7. 初始化MQTT命令处理器
    Serial0.println("[系统] 初始化命令处理器...");
    _commandProcessor = new CommandProcessor(_configManager, _deviceControl, _otaManager, _networkManager);
    
    // 8. 初始化RFID帧解析器
    Serial0.println("[系统] 初始化RFID帧解析器...");
    _rfidFrameParser = new RfidFrameParser(_configManager, _deviceControl, _networkManager);
    
    Serial0.println("系统启动完成!");
    Serial0.println("========================================");
    
    return true;
}

/**
 * @brief 初始化网络配置参数
 * @details 从ConfigManager读取系统配置，填充NetworkConfig结构体，
 *          设置MQTT连接参数和自动重连选项
 */
void SystemManager::initNetworkConfig() {
    // 清空网络配置结构体
    memset(&_networkConfig, 0, sizeof(NetworkConfig));
    
    // 获取系统配置引用
    SystemConfig& config = _configManager->getConfig();
    
    // 填充服务器地址和端口
    strncpy(_networkConfig.server, config.server, sizeof(_networkConfig.server) - 1);
    _networkConfig.port = config.port;
    
    // 启用MQTT协议
    _networkConfig.useMqtt = true;
    
    // 填充MQTT参数
    strncpy(_networkConfig.mqttClientId, config.clientId, sizeof(_networkConfig.mqttClientId) - 1);
    strncpy(_networkConfig.mqttUsername, config.username, sizeof(_networkConfig.mqttUsername) - 1);
    strncpy(_networkConfig.mqttPassword, config.password, sizeof(_networkConfig.mqttPassword) - 1);
    strncpy(_networkConfig.mqttTopic, config.topic, sizeof(_networkConfig.mqttTopic) - 1);
    strncpy(_networkConfig.mqttSubTopic, config.subTopic, sizeof(_networkConfig.mqttSubTopic) - 1);
    
    // 启用自动重连，重连间隔10秒
    _networkConfig.autoReconnect = true;
    _networkConfig.reconnectInterval = 10000;
}

/**
 * @brief 网络状态变化回调处理函数
 * @param status 网络状态(已连接/已断开/连接中/错误)
 * @param type 网络类型(Wi-Fi/蓝牙/4G)
 * @param message 状态描述消息
 * @details 处理网络状态变化事件，更新networkReady标志，
 *          当网络连接成功时注册OTA进度回调
 */
void SystemManager::onNetworkStatusChanged(NetworkStatus status, NetworkType type, const char* message) {
    // 打印网络状态信息
    Serial0.print("[网络] ");
    switch (type) {
        case NETWORK_TYPE_4G: Serial0.print("4G"); break;
        case NETWORK_TYPE_BLUETOOTH: Serial0.print("蓝牙"); break;
        case NETWORK_TYPE_WIFI: Serial0.print("Wi-Fi"); break;
    }
    Serial0.print(" - ");
    switch (status) {
        case NETWORK_CONNECTED: Serial0.print("已连接"); break;
        case NETWORK_DISCONNECTED: Serial0.print("已断开"); break;
        case NETWORK_CONNECTING: Serial0.print("连接中"); break;
        case NETWORK_ERROR: Serial0.print("错误"); break;
    }
    Serial0.print(": "); 
    Serial0.println(message);
    
    // 更新全局网络就绪标志(蓝牙连接不计入网络就绪)
    if (type != NETWORK_TYPE_BLUETOOTH) {
        networkReady = (status == NETWORK_CONNECTED);
    }
    
    // 网络连接成功时，注册OTA进度回调
    if (networkReady && _otaManager != nullptr) {
        _otaManager->setProgressCallback([](int progress, void* userData) {
            // 打印OTA下载进度
            Serial0.printf("[OTA] 进度回调: %d%%\n", progress);
            
            // 通过userData获取SystemManager实例
            SystemManager* self = (SystemManager*)userData;
            if (self != nullptr && networkReady) {
                // 构建OTA状态上报JSON消息
                char response[128];
                snprintf(response, sizeof(response), 
                         "{\"type\":\"ota_status\",\"status\":\"downloading\",\"progress\":%d,\"message\":\"下载中...\"}", 
                         progress);
                // 发送到MQTT服务器
                self->_networkManager->send((uint8_t*)response, strlen(response));
            }
        }, this);
    }
}

/**
 * @brief 网络数据接收回调处理函数
 * @param data 接收到的数据指针
 * @param length 数据长度
 * @details 将接收到的MQTT消息转换为字符串，依次尝试由CommandProcessor
 *          和CommandHandler处理，打印处理结果
 */
void SystemManager::onNetworkDataReceived(const uint8_t* data, uint16_t length) {
    // 将接收到的数据复制到缓冲区并添加字符串结束符
    char jsonStr[512];
    strncpy(jsonStr, (const char*)data, min((uint16_t)511, length));
    jsonStr[min((uint16_t)511, length)] = '\0';
    
    // 先尝试由CommandProcessor处理(MQTT命令)
    if (_commandProcessor->processCommand(jsonStr)) {
        Serial0.println("[CMD] 命令执行成功");
    } 
    // 再尝试由CommandHandler处理(RFID命令)
    else if (_cmdHandler->parseMqttCommand(jsonStr)) {
        Serial0.println("[CMD] 命令执行成功");
    } 
    // 命令无法识别
    else {
        Serial0.println("[CMD] 命令执行失败");
    }
}

/**
 * @brief 静态网络状态回调转发函数
 * @param status 网络状态
 * @param type 网络类型
 * @param message 状态消息
 * @param userData 用户数据指针(指向SystemManager实例)
 * @details 将静态回调函数转换为成员函数调用，是C++回调机制的标准模式
 */
void SystemManager::staticNetworkStatusCallback(NetworkStatus status, NetworkType type, const char* message, void* userData) {
    // 将userData转换为SystemManager指针
    SystemManager* self = (SystemManager*)userData;
    if (self != nullptr) {
        // 调用成员函数处理
        self->onNetworkStatusChanged(status, type, message);
    }
}

/**
 * @brief 静态网络数据回调转发函数
 * @param data 数据指针
 * @param length 数据长度
 * @param userData 用户数据指针(指向SystemManager实例)
 * @details 将静态回调函数转换为成员函数调用
 */
void SystemManager::staticNetworkDataCallback(const uint8_t* data, uint16_t length, void* userData) {
    // 将userData转换为SystemManager指针
    SystemManager* self = (SystemManager*)userData;
    if (self != nullptr) {
        // 调用成员函数处理
        self->onNetworkDataReceived(data, length);
    }
}

/**
 * @brief 发送心跳包到MQTT服务器
 * @details 构建JSON格式心跳消息，包含当前运行时间(秒)，
 *          当网络就绪时发送到MQTT服务器
 */
void SystemManager::sendHeartbeat() {
    // 构建心跳JSON消息
    char json[128];
    snprintf(json, sizeof(json), "{\"type\":\"heartbeat\",\"time\":%lu}", millis() / 1000);
    
    // 打印心跳信息
    Serial0.printf("[MQTT] 发送心跳: %s\n", json);
    
    // 网络就绪时发送
    if (networkReady) {
        if (_networkManager->send((uint8_t*)json, strlen(json))) {
            Serial0.println("[MQTT] 心跳发送成功");
        } else {
            Serial0.println("[MQTT] 心跳发送失败");
        }
    }
}

/**
 * @brief 处理RFID数据接收和解析
 * @details 从RS485读取数据到缓冲区，检测帧头"RF"(0x52 0x46)，
 *          当数据接收超时后调用帧解析器处理完整帧
 */
void SystemManager::processRfidData() {
    // 检查RS485接收缓冲区中是否有可用数据
    size_t available = _rs485->available();
    if (available > 0) {
        // 计算可读取的字节数(不超过缓冲区剩余空间)
        uint16_t toRead = min((uint16_t)(RECEIVE_BUFFER_SIZE - _receiveLen), (uint16_t)available);
        uint8_t tempBuffer[256];
        
        // 从RS485读取数据(超时500ms, 最小字节数50)
        uint16_t read = _rs485->receive(tempBuffer, 500, 50);
        
        // 将读取的数据复制到接收缓冲区
        for (uint16_t i = 0; i < read && _receiveLen < RECEIVE_BUFFER_SIZE; i++) {
            _receiveBuffer[_receiveLen++] = tempBuffer[i];
        }
        // 更新最后接收时间戳
        _lastReceiveTime = millis();
        
        // 打印接收信息
        Serial0.printf("[RX] 收到 %d 字节, 总计: %d\n", read, _receiveLen);
    }
    
    // 检测帧接收是否超时(超过FRAME_HEADER_TIMEOUT未收到新数据)
    if (_receiveLen > 0 && millis() - _lastReceiveTime > FRAME_HEADER_TIMEOUT) {
        // 打印完整帧数据(十六进制)
        Serial0.printf("\n[RX] 帧接收完成! 总计 %d 字节: ", _receiveLen);
        for (int i = 0; i < _receiveLen; i++) {
            Serial0.printf("%02X ", _receiveBuffer[i]);
        }
        Serial0.println();
        
        // 查找帧头"RF"(0x52 0x46)
        int headerPos = -1;
        for (int i = 0; i < _receiveLen - 1; i++) {
            if (_receiveBuffer[i] == 0x52 && _receiveBuffer[i+1] == 0x46) {
                headerPos = i;
                Serial0.printf("[OK] 帧头 'RF' 位于偏移量 %d\n", i);
                // 从帧头位置开始解析帧
                _rfidFrameParser->parseFrame(_receiveBuffer + i, _receiveLen - i);
                break;
            }
        }
        
        // 未找到帧头
        if (headerPos < 0) {
            Serial0.println("[WARN] 未找到帧头 'RF' (0x52 0x46)");
        }
        
        // 清空接收缓冲区
        _receiveLen = 0;
    }
}

/**
 * @brief 主循环函数
 * @details 在Arduino loop()中周期性调用，执行以下任务：
 *          1. 更新网络管理器(处理MQTT消息、检查连接状态)
 *          2. 更新设备控制(处理报警超时和LED闪烁)
 *          3. 更新OTA管理器(检查升级完成状态)
 *          4. 发送心跳包(定时)
 *          5. 处理RFID数据(接收和解析)
 */
void SystemManager::loop() {
    // 1. 更新网络管理器
    _networkManager->update();
    
    // 2. 更新设备控制模块
    _deviceControl->update();
    
    // 3. 更新OTA管理器
    _otaManager->update();
    
    // 4. 定时发送心跳包
    if (millis() - _lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
        _lastHeartbeatTime = millis();
        sendHeartbeat();
    }
    
    // 5. 处理RFID数据
    processRfidData();
    
    // 短暂延时，降低CPU占用
    delay(1);
}