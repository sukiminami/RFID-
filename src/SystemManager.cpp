#include "SystemManager.h"
#include <ArduinoJson.h>
#include "rs485.h"
#include "CommandHandler.h"
#include "DeviceControl.h"
#include "ConfigManager.h"
#include "OtaManager.h"
#include "CommandProcessor.h"
#include "RfidFrameParser.h"

bool networkReady = false;

SystemManager::SystemManager()
    : _rs485(nullptr), _cmdHandler(nullptr), _networkManager(nullptr),
      _deviceControl(nullptr), _configManager(nullptr),
      _otaManager(nullptr), _commandProcessor(nullptr), _rfidFrameParser(nullptr),
      _receiveLen(0), _lastReceiveTime(0), _lastHeartbeatTime(0) {
}

bool SystemManager::begin() {
    Serial0.println("========================================");
    Serial0.println("  RFID网关系统");
    Serial0.println("========================================");
    
    Serial0.println("[系统] 初始化RS485模块...");
    _rs485 = new Rs485Comm(&Serial2, UART_TX_PIN, UART_RX_PIN, UART_DE_PIN);
    if (!_rs485->begin(BAUD_RATE)) {
        Serial0.println("[系统] RS485初始化失败!");
        return false;
    }
    Serial0.println("[系统] RS485初始化完成");
    
    Serial0.println("[系统] 初始化设备控制模块...");
    _deviceControl = new DeviceControl();
    _deviceControl->begin();
    Serial0.println("[系统] 设备控制模块初始化完成");
    
    Serial0.println("[系统] 初始化配置管理器...");
    _configManager = new ConfigManager();
    _configManager->begin();
    Serial0.println("[系统] 配置管理器初始化完成");
    
    Serial0.println("[系统] 初始化OTA管理器...");
    _otaManager = new OtaManager();
    _otaManager->begin();
    _otaManager->setCurrentVersion(FIRMWARE_VERSION);
    Serial0.printf("[系统] 当前固件版本: %s\n", FIRMWARE_VERSION);
    Serial0.println("[系统] OTA管理器初始化完成");
    
    Serial0.println("[系统] 初始化命令处理器...");
    _cmdHandler = new CommandHandler(_rs485);
    
    Serial0.println("[系统] 初始化网络模块...");
    _networkManager = new NetworkManager();
    initNetworkConfig();
    
    _otaManager->setNetworkManager(_networkManager);
    
    if (!_networkManager->begin(&_networkConfig)) {
        Serial0.println("[系统] 网络管理器初始化失败");
    } else {
        Serial0.println("[系统] 网络管理器初始化成功");
        _networkManager->setStatusCallback(staticNetworkStatusCallback, this);
        _networkManager->setDataCallback(staticNetworkDataCallback, this);
        Serial0.println("[系统] 开始连接网络...");
        _networkManager->connect();
    }
    
    Serial0.println("[系统] 初始化命令处理器...");
    _commandProcessor = new CommandProcessor(_configManager, _deviceControl, _otaManager, _networkManager);
    
    Serial0.println("[系统] 初始化RFID帧解析器...");
    _rfidFrameParser = new RfidFrameParser(_configManager, _deviceControl, _networkManager);
    
    Serial0.println("系统启动完成!");
    Serial0.println("========================================");
    
    return true;
}

void SystemManager::initNetworkConfig() {
    memset(&_networkConfig, 0, sizeof(NetworkConfig));
    
    SystemConfig& config = _configManager->getConfig();
    
    strncpy(_networkConfig.server, config.server, sizeof(_networkConfig.server) - 1);
    _networkConfig.port = config.port;
    
    _networkConfig.useMqtt = true;
    strncpy(_networkConfig.mqttClientId, config.clientId, sizeof(_networkConfig.mqttClientId) - 1);
    strncpy(_networkConfig.mqttUsername, config.username, sizeof(_networkConfig.mqttUsername) - 1);
    strncpy(_networkConfig.mqttPassword, config.password, sizeof(_networkConfig.mqttPassword) - 1);
    strncpy(_networkConfig.mqttTopic, config.topic, sizeof(_networkConfig.mqttTopic) - 1);
    strncpy(_networkConfig.mqttSubTopic, config.subTopic, sizeof(_networkConfig.mqttSubTopic) - 1);
    
    _networkConfig.autoReconnect = true;
    _networkConfig.reconnectInterval = 10000;
}

void SystemManager::onNetworkStatusChanged(NetworkStatus status, NetworkType type, const char* message) {
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
    
    if (type != NETWORK_TYPE_BLUETOOTH) {
        networkReady = (status == NETWORK_CONNECTED);
    }
    
    if (networkReady && _otaManager != nullptr) {
        _otaManager->setProgressCallback([](int progress, void* userData) {
            Serial0.printf("[OTA] 进度回调: %d%%\n", progress);
            SystemManager* self = (SystemManager*)userData;
            if (self != nullptr && networkReady) {
                char response[128];
                snprintf(response, sizeof(response), 
                         "{\"type\":\"ota_status\",\"status\":\"downloading\",\"progress\":%d,\"message\":\"下载中...\"}", 
                         progress);
                self->_networkManager->send((uint8_t*)response, strlen(response));
            }
        }, this);
    }
}

void SystemManager::onNetworkDataReceived(const uint8_t* data, uint16_t length) {
    char jsonStr[512];
    strncpy(jsonStr, (const char*)data, min((uint16_t)511, length));
    jsonStr[min((uint16_t)511, length)] = '\0';
    
    if (_commandProcessor->processCommand(jsonStr)) {
        Serial0.println("[CMD] 命令执行成功");
    } else if (_cmdHandler->parseMqttCommand(jsonStr)) {
        Serial0.println("[CMD] 命令执行成功");
    } else {
        Serial0.println("[CMD] 命令执行失败");
    }
}

void SystemManager::staticNetworkStatusCallback(NetworkStatus status, NetworkType type, const char* message, void* userData) {
    SystemManager* self = (SystemManager*)userData;
    if (self != nullptr) {
        self->onNetworkStatusChanged(status, type, message);
    }
}

void SystemManager::staticNetworkDataCallback(const uint8_t* data, uint16_t length, void* userData) {
    SystemManager* self = (SystemManager*)userData;
    if (self != nullptr) {
        self->onNetworkDataReceived(data, length);
    }
}

void SystemManager::sendHeartbeat() {
    char json[128];
    snprintf(json, sizeof(json), "{\"type\":\"heartbeat\",\"time\":%lu}", millis() / 1000);
    
    Serial0.printf("[MQTT] 发送心跳: %s\n", json);
    
    if (networkReady) {
        if (_networkManager->send((uint8_t*)json, strlen(json))) {
            Serial0.println("[MQTT] 心跳发送成功");
        } else {
            Serial0.println("[MQTT] 心跳发送失败");
        }
    }
}

void SystemManager::processRfidData() {
    size_t available = _rs485->available();
    if (available > 0) {
        uint16_t toRead = min((uint16_t)(RECEIVE_BUFFER_SIZE - _receiveLen), (uint16_t)available);
        uint8_t tempBuffer[256];
        uint16_t read = _rs485->receive(tempBuffer, 500, 50);
        
        for (uint16_t i = 0; i < read && _receiveLen < RECEIVE_BUFFER_SIZE; i++) {
            _receiveBuffer[_receiveLen++] = tempBuffer[i];
        }
        _lastReceiveTime = millis();
        
        Serial0.printf("[RX] 收到 %d 字节, 总计: %d\n", read, _receiveLen);
    }
    
    if (_receiveLen > 0 && millis() - _lastReceiveTime > FRAME_HEADER_TIMEOUT) {
        Serial0.printf("\n[RX] 帧接收完成! 总计 %d 字节: ", _receiveLen);
        for (int i = 0; i < _receiveLen; i++) {
            Serial0.printf("%02X ", _receiveBuffer[i]);
        }
        Serial0.println();
        
        int headerPos = -1;
        for (int i = 0; i < _receiveLen - 1; i++) {
            if (_receiveBuffer[i] == 0x52 && _receiveBuffer[i+1] == 0x46) {
                headerPos = i;
                Serial0.printf("[OK] 帧头 'RF' 位于偏移量 %d\n", i);
                _rfidFrameParser->parseFrame(_receiveBuffer + i, _receiveLen - i);
                break;
            }
        }
        
        if (headerPos < 0) {
            Serial0.println("[WARN] 未找到帧头 'RF' (0x52 0x46)");
        }
        
        _receiveLen = 0;
    }
}

void SystemManager::loop() {
    _networkManager->update();
    _deviceControl->update();
    _otaManager->update();
    
    if (millis() - _lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
        _lastHeartbeatTime = millis();
        sendHeartbeat();
    }
    
    processRfidData();
    
    delay(1);
}