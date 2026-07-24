/**
 * @file BluetoothNetwork.h
 * @brief 蓝牙网络连接模块头文件（BLE 版本）
 * @details 本模块封装了 ESP32-S3 的 BLE（Bluetooth Low Energy）功能，
 *          实现 NetworkInterface 接口。支持 BLE 串口模拟，通过 BLE
 *          与手机或其他 BLE 设备进行数据通信。
 */

#ifndef BLUETOOTH_NETWORK_H
#define BLUETOOTH_NETWORK_H

#include "NetworkInterface.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

/**
 * @brief 蓝牙类型枚举
 */
typedef enum {
    BLUETOOTH_BLE = 0  ///< 低功耗蓝牙（BLE）
} BluetoothType;

/**
 * @brief BLE 服务 UUID 定义
 * @note 使用标准串口服务 UUID 兼容现有 BLE 串口 APP
 */
#define BLE_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_RX_CHARACTERISTIC   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_TX_CHARACTERISTIC   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

/**
 * @brief 蓝牙网络连接类（BLE 实现）
 * @details 封装 ESP32-S3 BLE 功能，实现 NetworkInterface 接口。
 *          通过 BLE 虚拟串口与外部设备通信，适用于配置和数据传输。
 */
class BluetoothNetwork : public NetworkInterface {
public:
    /**
     * @brief 构造函数
     * @param deviceName 蓝牙设备名称（默认 "SleepMonitor_BLE"）
     * @param type 蓝牙类型（BLE）
     */
    BluetoothNetwork(const char* deviceName = "SleepMonitor_BLE", BluetoothType type = BLUETOOTH_BLE);
    
    /**
     * @brief 析构函数
     */
    ~BluetoothNetwork();
    
    /**
     * @brief 初始化 BLE 模块
     * @return true-成功，false-失败
     * @details 创建 BLE 设备、服务器、服务和特征，
     *          开始广播等待客户端连接
     */
    bool begin() override;
    
    /**
     * @brief 断开 BLE 连接
     * @details 断开当前 BLE 连接，重新开始广播
     */
    void disconnect() override;
    
    /**
     * @brief 获取 BLE 连接状态
     * @return NetworkStatus 枚举值
     */
    NetworkStatus getStatus() override;
    
    /**
     * @brief 获取网络类型
     * @return NETWORK_TYPE_BLUETOOTH
     */
    NetworkType getType() override { return NETWORK_TYPE_BLUETOOTH; }
    
    /**
     * @brief 通过 BLE 发送数据
     * @param data 数据指针
     * @param length 数据长度
     * @return true-发送成功，false-失败或未连接
     */
    bool send(const uint8_t* data, uint16_t length) override;
    
    /**
     * @brief 周期性更新
     * @details 检查 BLE 连接状态，处理接收数据并触发回调
     */
    void update() override;
    
    /**
     * @brief 检查是否已连接
     * @return true-已连接，false-未连接
     */
    bool isConnected();
    
    /**
     * @brief 获取 BLE 服务器指针
     * @return BLEServer 指针
     */
    BLEServer* getServer() { return _pServer; }
    
    /**
     * @brief 获取 TX 特征指针
     * @return BLECharacteristic 指针
     */
    BLECharacteristic* getTxCharacteristic() { return _pTxCharacteristic; }
    
private:
    BLEServer* _pServer;                   ///< BLE 服务器指针
    BLECharacteristic* _pTxCharacteristic; ///< TX 特征（发送数据）
    BLECharacteristic* _pRxCharacteristic; ///< RX 特征（接收数据）
    bool _connected;                       ///< 连接状态标志
    char _deviceName[32];                  ///< BLE 设备名称
    BluetoothType _type;                   ///< 蓝牙类型
    
    uint8_t _rxBuffer[256];                ///< 接收缓冲区
    uint16_t _rxLength;                    ///< 接收数据长度
    
    void handleReceivedData(const uint8_t* data, uint16_t length);
    
    friend class BleServerCallbacks;
    friend class BleCharacteristicCallbacks;
};

/**
 * @brief BLE 服务器回调类
 * @details 处理 BLE 连接和断开事件
 */
class BleServerCallbacks : public BLEServerCallbacks {
public:
    BleServerCallbacks(BluetoothNetwork* network) : _network(network) {}
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;
    
private:
    BluetoothNetwork* _network;
};

/**
 * @brief BLE 特征回调类
 * @details 处理 BLE 数据写入事件
 */
class BleCharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
    BleCharacteristicCallbacks(BluetoothNetwork* network) : _network(network) {}
    void onWrite(BLECharacteristic* pCharacteristic) override;
    
private:
    BluetoothNetwork* _network;
};

#endif
