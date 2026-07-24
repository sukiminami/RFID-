/**
 * @file BluetoothNetwork.cpp
 * @brief 蓝牙网络连接模块实现（BLE 版本）
 * @details 本文件实现了基于 ESP32-S3 的 BLE（Bluetooth Low Energy）
 *          虚拟串口通信功能。使用标准 Nordic UART Service UUID，
 *          兼容 nRF Connect、LightBlue 等常见 BLE 调试 APP。
 */

#include "BluetoothNetwork.h"
#include <WiFi.h>

/**
 * @brief 构造函数
 * @param deviceName BLE 设备名称
 * @param type 蓝牙类型（BLE）
 */
BluetoothNetwork::BluetoothNetwork(const char* deviceName, BluetoothType type) {
    _pServer = nullptr;
    _pTxCharacteristic = nullptr;
    _pRxCharacteristic = nullptr;
    _connected = false;
    _type = type;
    _rxLength = 0;
    strncpy(_deviceName, deviceName, sizeof(_deviceName) - 1);
    _deviceName[sizeof(_deviceName) - 1] = '\0';
}

/**
 * @brief 析构函数
 * @details 释放 BLE 资源
 */
BluetoothNetwork::~BluetoothNetwork() {
    disconnect();
}

/**
 * @brief BLE 服务器回调实现
 * @param pServer BLE 服务器指针
 */
void BleServerCallbacks::onConnect(BLEServer* pServer) {
    _network->_connected = true;
    _network->triggerStatusCallback(NETWORK_CONNECTED, "BLE connected");
}

/**
 * @brief BLE 断开连接回调实现
 * @param pServer BLE 服务器指针
 */
void BleServerCallbacks::onDisconnect(BLEServer* pServer) {
    _network->_connected = false;
    _network->triggerStatusCallback(NETWORK_DISCONNECTED, "BLE disconnected");
    pServer->startAdvertising();
}

/**
 * @brief BLE 特征写入回调实现
 * @param pCharacteristic BLE 特征指针
 */
void BleCharacteristicCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
        _network->handleReceivedData((const uint8_t*)value.c_str(), value.length());
    }
}

/**
 * @brief 初始化 BLE 模块
 * @return true-成功，false-失败
 * @details 创建 BLE 设备、服务器、服务和特征，开始广播
 */
bool BluetoothNetwork::begin() {
    Serial0.printf("[BLE] Initializing BLE device: %s\n", _deviceName);
    
    WiFi.mode(WIFI_OFF);
    delay(200);
    
    Serial0.println("[BLE] WiFi disabled to save power");
    
    BLEDevice::init(_deviceName);
    delay(100);
    
    _pServer = BLEDevice::createServer();
    if (_pServer == nullptr) {
        Serial0.println("[BLE] ERROR: Failed to create BLE server");
        triggerStatusCallback(NETWORK_ERROR, "BLE server creation failed");
        BLEDevice::deinit();
        return false;
    }
    
    _pServer->setCallbacks(new BleServerCallbacks(this));
    
    BLEService* pService = _pServer->createService(BLE_SERVICE_UUID);
    if (pService == nullptr) {
        Serial0.println("[BLE] ERROR: Failed to create BLE service");
        triggerStatusCallback(NETWORK_ERROR, "BLE service creation failed");
        BLEDevice::deinit();
        return false;
    }
    
    _pTxCharacteristic = pService->createCharacteristic(
        BLE_TX_CHARACTERISTIC,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );
    if (_pTxCharacteristic == nullptr) {
        Serial0.println("[BLE] ERROR: Failed to create TX characteristic");
        triggerStatusCallback(NETWORK_ERROR, "TX characteristic creation failed");
        BLEDevice::deinit();
        return false;
    }
    _pTxCharacteristic->addDescriptor(new BLE2902());
    
    _pRxCharacteristic = pService->createCharacteristic(
        BLE_RX_CHARACTERISTIC,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    if (_pRxCharacteristic == nullptr) {
        Serial0.println("[BLE] ERROR: Failed to create RX characteristic");
        triggerStatusCallback(NETWORK_ERROR, "RX characteristic creation failed");
        BLEDevice::deinit();
        return false;
    }
    _pRxCharacteristic->setCallbacks(new BleCharacteristicCallbacks(this));
    
    pService->start();
    delay(50);
    
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    if (pAdvertising == nullptr) {
        Serial0.println("[BLE] ERROR: Failed to get advertising");
        triggerStatusCallback(NETWORK_ERROR, "BLE advertising failed");
        BLEDevice::deinit();
        return false;
    }
    
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    
    BLEDevice::startAdvertising();
    delay(50);
    
    Serial0.println("[BLE] BLE initialized successfully, waiting for connection");
    triggerStatusCallback(NETWORK_CONNECTING, "BLE waiting for connection");
    _status = NETWORK_CONNECTING;
    
    return true;
}

/**
 * @brief 断开 BLE 连接
 * @details 断开当前连接并重新开始广播
 */
void BluetoothNetwork::disconnect() {
    if (_pServer != nullptr) {
        BLEDevice::getAdvertising()->stop();
        _connected = false;
        _status = NETWORK_DISCONNECTED;
    }
}

/**
 * @brief 获取 BLE 连接状态
 * @return NetworkStatus 枚举值
 */
NetworkStatus BluetoothNetwork::getStatus() {
    if (_connected) {
        return NETWORK_CONNECTED;
    } else if (_status == NETWORK_CONNECTING) {
        return NETWORK_CONNECTING;
    }
    return NETWORK_DISCONNECTED;
}

/**
 * @brief 通过 BLE 发送数据
 * @param data 数据指针
 * @param length 数据长度
 * @return true-发送成功，false-失败或未连接
 */
bool BluetoothNetwork::send(const uint8_t* data, uint16_t length) {
    if (!_connected || _pTxCharacteristic == nullptr) {
        return false;
    }
    
    _pTxCharacteristic->setValue((uint8_t*)data, length);
    _pTxCharacteristic->notify();
    
    return true;
}

/**
 * @brief 周期性更新
 * @details 检查 BLE 连接状态，处理接收数据
 */
void BluetoothNetwork::update() {
    unsigned long now = millis();
    if (now - _lastUpdateTime < 50) return;
    _lastUpdateTime = now;
    
    if (_connected && _rxLength > 0) {
        if (_dataCallback != nullptr) {
            _dataCallback(_rxBuffer, _rxLength, _userData);
        }
        _rxLength = 0;
    }
}

/**
 * @brief 检查是否已连接
 * @return true-已连接，false-未连接
 */
bool BluetoothNetwork::isConnected() {
    return _connected;
}

/**
 * @brief 处理接收到的数据
 * @param data 数据指针
 * @param length 数据长度
 */
void BluetoothNetwork::handleReceivedData(const uint8_t* data, uint16_t length) {
    if (length > sizeof(_rxBuffer)) {
        length = sizeof(_rxBuffer);
    }
    memcpy(_rxBuffer, data, length);
    _rxLength = length;
}
