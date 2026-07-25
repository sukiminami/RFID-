/**
 * @file NetworkManager.h
 * @brief 网络连接管理器头文件
 * @details 统一管理 4G、Wi-Fi、蓝牙三种网络模块，采用主备网络模式：
 *          - 主网络：Wi-Fi（日常数据发送）
 *          - 备用网络：4G（Wi-Fi 断开时自动切换）
 *          - 蓝牙：用于配置 Wi-Fi 名称和密码
 *          支持自动网络切换和状态监控。
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "NetworkInterface.h"
#include "BluetoothNetwork.h"
#include "WiFiNetwork.h"
#include <Preferences.h>  // 【新增】ESP32持久化存储库

/**
 * @brief 网络配置结构体
 * @details 存储连接所需的全部参数，包括 Wi-Fi 凭证、服务器地址、
 *          4G APN、MQTT 配置、自动重连设置等。
 */
typedef struct {
    char ssid[64];              /**< Wi-Fi SSID */
    char password[64];          /**< Wi-Fi 密码 */
    char server[128];           /**< 服务器 IP 或域名 */
    uint16_t port;              /**< 服务器端口号 */
    bool autoReconnect;         /**< 是否启用自动重连 */
    unsigned long reconnectInterval; /**< 重连间隔（毫秒） */
    
    // MQTT 配置
    bool useMqtt;               /**< 是否使用 MQTT 协议 */
    char mqttClientId[64];      /**< MQTT 客户端 ID */
    char mqttUsername[64];      /**< MQTT 用户名 */
    char mqttPassword[64];      /**< MQTT 密码 */
    char mqttTopic[128];        /**< MQTT 发布主题（上行） */
    char mqttSubTopic[128];     /**< MQTT 订阅主题（下行命令） */
} NetworkConfig;

/**
 * @brief 网络配置阶段枚举
 */
typedef enum {
    CONFIG_PHASE_WAITING,     /**< 等待蓝牙配置 Wi-Fi */
    CONFIG_PHASE_RECEIVED,    /**< 已收到 Wi-Fi 配置 */
    CONFIG_PHASE_CONNECTING   /**< 正在连接 Wi-Fi */
} ConfigPhase;

/**
 * @brief 网络连接管理器类
 * @details 管理 Wi-Fi 和蓝牙网络模块。蓝牙用于配置 Wi-Fi 参数，
 *          Wi-Fi 连接后通过 MQTT 与服务器通信。支持自动重连。
 */
class NetworkManager {
public:
    /**
     * @brief 构造函数
     * @details 初始化所有成员变量为空/默认值
     */
    NetworkManager();
    
    /**
     * @brief 析构函数
     * @details 断开所有网络连接，释放所有网络模块内存
     */
    ~NetworkManager();
    
    /**
     * @brief 初始化网络管理器
     * @param config 网络配置指针（可为 nullptr，使用默认配置）
     * @return true-初始化成功，false-失败
     * @details 创建蓝牙和 Wi-Fi 模块实例，并为每个模块注册回调
     */
    bool begin(NetworkConfig* config = nullptr);
    
    /**
     * @brief 连接网络
     * @return true-连接成功，false-等待配置或连接失败
     * @details 若已收到 Wi-Fi 配置则连接 Wi-Fi，否则等待蓝牙配置
     */
    bool connect();
    
    /**
     * @brief 处理蓝牙接收到的 Wi-Fi 配置数据
     * @param data 数据指针
     * @param length 数据长度
     * @details 解析格式：SET_WIFI:SSID:PASSWORD
     */
    void handleWifiConfig(const uint8_t* data, uint16_t length);
    
    /**
     * @brief 检查是否已收到 Wi-Fi 配置
     * @return true-已收到配置，false-未收到
     */
    bool hasWifiConfig();
    
    /**
     * @brief 周期性更新函数（需在 loop 中调用）
     * @details 更新所有网络模块状态，监控网络健康度
     */
    void update();
    
    /**
     * @brief 断开所有网络连接
     * @details 依次断开 Wi-Fi、蓝牙连接
     */
    void disconnect();
    
    /**
     * @brief 发送数据到服务器
     * @param data 数据指针
     * @param length 数据长度
     * @return true-发送成功，false-失败
     * @details 使用 Wi-Fi 发送数据
     */
    bool send(const uint8_t* data, uint16_t length);
    
    /**
     * @brief 获取当前使用的网络类型
     * @return NetworkType 枚举值
     */
    NetworkType getCurrentNetworkType();
    
    /**
     * @brief 获取当前网络状态
     * @return NetworkStatus 枚举值
     */
    NetworkStatus getNetworkStatus();
    
    /**
     * @brief 检查是否有网络连接
     * @return true-网络已连接，false-未连接
     */
    bool isConnected();
    
    /**
     * @brief 检查 Wi-Fi 是否已连接
     * @return true-已连接，false-未连接
     */
    bool isWifiConnected();
    
    /**
     * @brief 设置网络配置参数
     * @param config 配置结构体指针
     * @return true-设置成功，false-参数为空
     */
    bool setConfig(NetworkConfig* config);
    
    /**
     * @brief 保存Wi-Fi配置到Flash持久化存储
     * @return true-保存成功，false-失败
     * @details 使用ESP32 Preferences库将SSID和密码保存到Flash，
     *          重启后可自动加载，无需每次通过蓝牙重新配置
     */
    bool saveWifiConfig();
    
    /**
     * @brief 从Flash加载已保存的Wi-Fi配置
     * @return true-加载成功且有有效配置，false-无保存的配置
     * @details 从Preferences中读取之前保存的WiFi账号密码，
     *          如果存在则自动填充到_config并返回true
     */
    bool loadWifiConfig();
    
    /**
     * @brief 清除已保存的Wi-Fi配置
     * @return true-清除成功，false-失败
     */
    bool clearWifiConfig();
    
    /**
     * @brief 获取蓝牙模块指针
     * @return BluetoothNetwork 指针（可能为 nullptr）
     */
    BluetoothNetwork* getBluetoothModule();
    
    /**
     * @brief 获取 Wi-Fi 模块指针
     * @return WiFiNetwork 指针（可能为 nullptr）
     */
    WiFiNetwork* getWiFiModule();
    
    /**
     * @brief 设置数据接收回调函数
     * @param callback 回调函数指针
     * @param userData 用户数据指针
     */
    void setDataCallback(NetworkDataCallback callback, void* userData = nullptr);
    
    /**
     * @brief 设置网络状态变化回调函数
     * @param callback 回调函数指针
     * @param userData 用户数据指针
     */
    void setStatusCallback(NetworkStatusCallback callback, void* userData = nullptr);

private:
    BluetoothNetwork* _bluetooth;   /**< 蓝牙模块实例指针 */
    WiFiNetwork* _wifi;             /**< Wi-Fi 模块实例指针 */
    NetworkConfig _config;          /**< 网络配置参数 */
    NetworkType _activeNetwork;     /**< 当前活跃网络类型 */
    ConfigPhase _configPhase;       /**< 配置阶段状态 */
    bool _initialized;              /**< 是否已初始化 */
    bool _bluetoothInitialized;     /**< 蓝牙是否已初始化 */
    unsigned long _lastUpdateTime;  /**< 上次更新时间戳 */
    unsigned long _lastSwitchTime;  /**< 上次网络切换时间戳 */
    unsigned long _wifiFailTime;    /**< Wi-Fi 失败时间戳 */
    NetworkDataCallback _dataCallback;      /**< 数据接收回调 */
    NetworkStatusCallback _statusCallback;  /**< 状态变化回调 */
    void* _userData;                        /**< 用户数据指针 */
    
    static const unsigned long SWITCH_COOLDOWN = 10000; /**< 网络切换冷却时间（毫秒） */
    static const unsigned long WIFI_RETRY_INTERVAL = 60000; /**< Wi-Fi 重试间隔（60秒） */
    
    /**
     * @brief 初始化 Wi-Fi 模块并连接服务器
     * @return true-成功，false-失败
     */
    bool initWiFi();
    
    /**
     * @brief 初始化蓝牙模块
     * @return true-成功，false-失败
     */
    bool initBluetooth();
    
    /**
     * @brief 检查并自动重连 Wi-Fi
     * @details 若 Wi-Fi 断开且超过冷却时间，尝试重新连接
     */
    void checkAutoReconnect();
    
    /**
     * @brief 内部状态变化处理函数
     * @param status 网络状态
     * @param type 网络类型
     * @param message 状态信息
     */
    void onNetworkStatusChanged(NetworkStatus status, NetworkType type, const char* message);
    
    /**
     * @brief 内部数据接收处理函数
     * @param data 数据指针
     * @param length 数据长度
     */
    void onNetworkDataReceived(const uint8_t* data, uint16_t length);
    
    /**
     * @brief 静态回调转发函数（状态变化）
     * @param status 网络状态
     * @param type 网络类型
     * @param message 状态信息
     * @param userData 用户数据指针（指向 NetworkManager 实例）
     */
    static void staticStatusCallback(NetworkStatus status, NetworkType type, const char* message, void* userData);
    
    /**
     * @brief 静态回调转发函数（数据接收）
     * @param data 数据指针
     * @param length 数据长度
     * @param userData 用户数据指针（指向 NetworkManager 实例）
     */
    static void staticDataCallback(const uint8_t* data, uint16_t length, void* userData);
};

#endif
