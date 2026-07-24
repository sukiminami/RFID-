/**
 * @file WiFiNetwork.h
 * @brief Wi-Fi 网络连接模块头文件
 * @details 使用 ESP32 内置 Wi-Fi 功能连接无线热点并建立 TCP 连接。
 *          继承自 NetworkInterface。
 */

#ifndef WIFI_NETWORK_H
#define WIFI_NETWORK_H

#include "NetworkInterface.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

/**
 * @brief 网络协议类型枚举
 */
typedef enum {
    PROTOCOL_TCP = 0,      /**< 原始 TCP 协议 */
    PROTOCOL_MQTT = 1      /**< MQTT 协议 */
} NetworkProtocol;

/**
 * @brief Wi-Fi 网络连接类
 * @details 封装 ESP32 Wi-Fi 连接和 TCP 客户端功能，
 *          实现 NetworkInterface 接口。
 */
class WiFiNetwork : public NetworkInterface {
public:
    /**
     * @brief 构造函数
     * @details 初始化 TCP 连接状态为断开，清空 SSID/密码/服务器信息
     */
    WiFiNetwork();
    
    /**
     * @brief 析构函数
     * @details 断开 TCP 连接和 Wi-Fi
     */
    ~WiFiNetwork();
    
    /**
     * @brief 初始化 Wi-Fi 模块
     * @return true-成功
     * @details 设置状态为断开，TCP 连接标志为 false
     */
    bool begin() override;
    
    /**
     * @brief 断开 Wi-Fi 和 TCP 连接
     */
    void disconnect() override;
    
    /**
     * @brief 获取网络状态
     * @return NetworkStatus 枚举值
     */
    NetworkStatus getStatus() override;
    
    /**
     * @brief 获取网络类型
     * @return NETWORK_TYPE_WIFI
     */
    NetworkType getType() override { return NETWORK_TYPE_WIFI; }
    
    /**
     * @brief 发送数据到 TCP 服务器
     * @param data 数据指针
     * @param length 数据长度
     * @return true-发送成功，false-失败
     */
    bool send(const uint8_t* data, uint16_t length) override;
    
    /**
     * @brief 周期性更新
     * @details 检查 TCP 连接状态，读取接收数据并触发回调
     */
    void update() override;
    
    /**
     * @brief 连接到 Wi-Fi 热点（默认 15 秒超时）
     * @param ssid Wi-Fi 名称
     * @param password Wi-Fi 密码
     * @return true-连接成功，false-失败或超时
     */
    bool connectToAP(const char* ssid, const char* password);
    
    /**
     * @brief 连接到 Wi-Fi 热点（自定义超时）
     * @param ssid Wi-Fi 名称
     * @param password Wi-Fi 密码
     * @param timeout 超时时间（毫秒）
     * @return true-连接成功，false-失败或超时
     */
    bool connectToAP(const char* ssid, const char* password, unsigned long timeout);
    
    /**
     * @brief 建立 TCP 连接到服务器
     * @param server 服务器 IP 或域名
     * @param port 服务器端口
     * @return true-连接成功，false-失败
     */
    bool connectTCP(const char* server, uint16_t port);
    
    /**
     * @brief 连接到 MQTT Broker
     * @param server MQTT 服务器地址
     * @param port MQTT 服务器端口
     * @param clientId MQTT 客户端 ID
     * @param username MQTT 用户名（可选）
     * @param password MQTT 密码（可选）
     * @param topic MQTT 发布主题（关键！必须设置）
     * @param subTopic MQTT 订阅主题（下行命令接收）
     * @return true-连接成功，false-失败
     */
    bool connectMQTT(const char* server, uint16_t port, const char* clientId, 
                     const char* username = nullptr, const char* password = nullptr,
                     const char* topic = nullptr, const char* subTopic = nullptr);
    
    /**
     * @brief 发布 MQTT 消息到指定主题
     * @param topic MQTT 主题
     * @param payload 消息内容
     * @param length 消息长度
     * @param retained 是否保留消息（默认 false）
     * @return true-发布成功，false-失败
     */
    bool mqttPublish(const char* topic, const uint8_t* payload, uint16_t length, bool retained = false);
    
    /**
     * @brief 订阅 MQTT 主题
     * @param topic MQTT 主题
     * @param qos QoS 级别（0-2，默认 0）
     * @return true-订阅成功，false-失败
     */
    bool mqttSubscribe(const char* topic, uint8_t qos = 0);
    
    /**
     * @brief 检查 MQTT 是否已连接
     * @return true-已连接，false-未连接
     */
    bool isMqttConnected();
    
    /**
     * @brief 获取 Wi-Fi 信号强度
     * @return RSSI 值（dBm），未连接返回 -127
     */
    int getRSSI();
    
    /**
     * @brief 获取本地 IP 地址
     * @param buffer 输出缓冲区（至少 16 字节）
     * @param bufferSize 缓冲区大小
     * @return true-获取成功，false-失败
     */
    bool getLocalIP(char* buffer, size_t bufferSize);
    
private:
    WiFiClient _wifiClient;        /**< Wi-Fi TCP 客户端对象 */
    PubSubClient _mqttClient;      /**< MQTT 客户端对象 */
    NetworkProtocol _protocol;     /**< 当前使用的协议 */
    bool _tcpConnected;            /**< TCP 连接状态 */
    char _ssid[64];                /**< Wi-Fi SSID */
    char _password[64];            /**< Wi-Fi 密码 */
    char _server[128];             /**< 服务器地址 */
    uint16_t _port;                /**< 服务器端口 */
    char _clientId[64];            /**< MQTT 客户端 ID */
    char _username[64];            /**< MQTT 用户名 */
    char _mqttPassword[64];        /**< MQTT 密码 */
    char _mqttTopic[128];          /**< MQTT 发布主题 */

    // 【新增】非阻塞状态机成员
    unsigned long _lastReconnectAttempt;  /**< 上次重连尝试时间 */
    unsigned long _reconnectDelay;        /**< 当前重连延迟（毫秒） */
    int _reconnectRetryCount;             /**< 重连尝试次数 */
    bool _isReconnecting;                 /**< 是否正在重连中 */
    unsigned long _lastWifiCheckTime;     /**< 上次WiFi检查时间 */
    
    /**
     * @brief MQTT 回调函数
     * @param topic 主题
     * @param payload 消息内容
     * @param length 消息长度
     */
    void mqttCallback(char* topic, uint8_t* payload, unsigned int length);
};

#endif
