/**
 * @file NetworkInterface.h
 * @brief 统一网络连接接口定义
 * @details 定义所有网络模块必须实现的抽象基类，包含纯虚函数
 *          begin/disconnect/getStatus/getType/send/update，
 *          以及回调注册和状态触发机制。
 */

#ifndef NETWORK_INTERFACE_H
#define NETWORK_INTERFACE_H

#include <Arduino.h>

/**
 * @brief 网络状态枚举
 */
typedef enum {
    NETWORK_DISCONNECTED = 0,  /**< 已断开 */
    NETWORK_CONNECTING = 1,    /**< 连接中 */
    NETWORK_CONNECTED = 2,     /**< 已连接 */
    NETWORK_ERROR = 3          /**< 错误 */
} NetworkStatus;

/**
 * @brief 网络类型枚举
 */
typedef enum {
    NETWORK_TYPE_NONE = 0,     /**< 无网络 */
    NETWORK_TYPE_4G = 1,       /**< 4G LTE */
    NETWORK_TYPE_BLUETOOTH = 2,/**< 蓝牙 */
    NETWORK_TYPE_WIFI = 3      /**< Wi-Fi */
} NetworkType;

/**
 * @brief 网络数据接收回调函数指针类型
 * @param data 数据指针
 * @param length 数据长度
 * @param userData 用户数据指针
 */
typedef void (*NetworkDataCallback)(const uint8_t* data, uint16_t length, void* userData);

/**
 * @brief 网络状态变化回调函数指针类型
 * @param status 网络状态
 * @param type 网络类型
 * @param message 状态信息
 * @param userData 用户数据指针
 */
typedef void (*NetworkStatusCallback)(NetworkStatus status, NetworkType type, const char* message, void* userData);

/**
 * @brief 网络连接接口抽象基类
 * @details 所有网络模块（A7670C、WiFiNetwork、BluetoothNetwork）
 *          必须继承此类并实现所有纯虚函数，实现策略模式。
 */
class NetworkInterface {
public:
    /**
     * @brief 构造函数
     * @details 初始化状态为断开，回调为空，重试计数为 0
     */
    NetworkInterface();
    
    /**
     * @brief 虚析构函数
     */
    virtual ~NetworkInterface();
    
    /**
     * @brief 初始化网络模块（纯虚函数）
     * @return true-成功，false-失败
     */
    virtual bool begin() = 0;
    
    /**
     * @brief 断开网络连接（纯虚函数）
     */
    virtual void disconnect() = 0;
    
    /**
     * @brief 获取网络状态（纯虚函数）
     * @return NetworkStatus 枚举值
     */
    virtual NetworkStatus getStatus() = 0;
    
    /**
     * @brief 获取网络类型（纯虚函数）
     * @return NetworkType 枚举值
     */
    virtual NetworkType getType() = 0;
    
    /**
     * @brief 发送数据（纯虚函数）
     * @param data 数据指针
     * @param length 数据长度
     * @return true-发送成功，false-失败
     */
    virtual bool send(const uint8_t* data, uint16_t length) = 0;
    
    /**
     * @brief 周期性更新（纯虚函数）
     * @details 检查连接状态、接收数据、触发回调
     */
    virtual void update() = 0;
    
    /**
     * @brief 注册数据接收回调
     * @param callback 回调函数指针
     * @param userData 用户数据指针（传递给回调函数）
     */
    void setDataCallback(NetworkDataCallback callback, void* userData = nullptr);
    
    /**
     * @brief 注册状态变化回调
     * @param callback 回调函数指针
     * @param userData 用户数据指针（传递给回调函数）
     */
    void setStatusCallback(NetworkStatusCallback callback, void* userData = nullptr);

protected:
    NetworkStatus _status;                  /**< 当前网络状态 */
    NetworkDataCallback _dataCallback;      /**< 数据接收回调 */
    NetworkStatusCallback _statusCallback;  /**< 状态变化回调 */
    void* _userData;                        /**< 用户数据指针 */
    unsigned long _lastUpdateTime;          /**< 上次更新时间戳 */
    uint8_t _retryCount;                    /**< 当前重试次数 */
    static const uint8_t MAX_RETRY_COUNT = 3; /**< 最大重试次数 */
    
    /**
     * @brief 触发状态变化回调
     * @param status 网络状态
     * @param message 状态信息
     * @details 更新 _status 并调用 _statusCallback
     */
    void triggerStatusCallback(NetworkStatus status, const char* message);
};

#endif
