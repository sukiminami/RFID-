/**
 * @file ConfigManager.h
 * @brief 配置管理器头文件
 * @details 负责管理系统配置和标签白名单的持久化存储，使用ESP32 Preferences库将配置保存到Flash，
 *          支持系统配置的加载/保存/重置，以及白名单的增删查改操作。
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

// ==================== 常量定义 ====================

/**
 * @def MAX_WHITELIST_SIZE
 * @brief 白名单最大容量
 * @note 最多支持32个标签EPC
 */
#define MAX_WHITELIST_SIZE 32

/**
 * @def MAX_EPC_LENGTH
 * @brief 单个EPC最大长度(含NULL终止符)
 * @note EPC通常为12-24字节的十六进制字符串，转换后为24-48字符
 */
#define MAX_EPC_LENGTH 32

/**
 * @def CONFIG_NAMESPACE
 * @brief Preferences命名空间名称
 * @note 用于区分不同应用的配置存储
 */
#define CONFIG_NAMESPACE "rfid_gateway"

// ==================== 结构体定义 ====================

/**
 * @struct SystemConfig
 * @brief 系统配置结构体
 * @details 存储所有系统配置参数，包括MQTT连接参数、报警设置、心跳间隔等
 */
typedef struct {
    char server[64];              /**< MQTT服务器地址(IP或域名) */
    uint16_t port;                /**< MQTT服务器端口号 */
    char clientId[32];            /**< MQTT客户端ID */
    char username[32];            /**< MQTT用户名 */
    char password[32];            /**< MQTT密码 */
    char topic[64];               /**< MQTT发布主题(上行数据) */
    char subTopic[64];            /**< MQTT订阅主题(下行命令) */
    
    uint16_t alarmDuration;       /**< 报警持续时间(毫秒) */
    uint32_t heartbeatInterval;   /**< 心跳包发送间隔(毫秒) */
    uint8_t bleEnabled;           /**< 蓝牙是否启用(0-禁用, 1-启用) */
} SystemConfig;

// ==================== 类定义 ====================

/**
 * @class ConfigManager
 * @brief 配置管理器类
 * @details 封装系统配置和白名单管理功能：
 *          1. 系统配置的加载、保存、重置
 *          2. 白名单的增删查改操作
 *          3. 使用ESP32 Preferences库实现Flash持久化
 */
class ConfigManager {
public:
    /**
     * @brief 构造函数
     * @details 初始化默认配置，清空白名单数组
     */
    ConfigManager();
    
    /**
     * @brief 初始化配置管理器
     * @return true-初始化成功，false-初始化失败
     * @details 加载系统配置和白名单
     */
    bool begin();
    
    /**
     * @brief 从Flash加载系统配置
     * @return true-加载成功，false-加载失败(使用默认配置)
     * @details 从Preferences读取之前保存的配置参数
     */
    bool loadConfig();
    
    /**
     * @brief 保存系统配置到Flash
     * @return true-保存成功，false-保存失败
     * @details 将当前配置写入Preferences持久化存储
     */
    bool saveConfig();
    
    /**
     * @brief 重置配置为默认值
     * @details 初始化默认配置，清空白名单，保存到Flash
     */
    void resetConfig();
    
    /**
     * @brief 获取系统配置引用
     * @return SystemConfig引用
     * @details 返回当前配置的引用，可直接修改配置参数
     */
    SystemConfig& getConfig();
    
    /**
     * @brief 设置系统配置
     * @param config 新的系统配置
     * @return true-设置成功，false-设置失败
     * @details 复制配置并保存到Flash
     */
    bool setConfig(const SystemConfig& config);
    
    /**
     * @brief 添加EPC到白名单
     * @param epc 标签EPC字符串
     * @return true-添加成功，false-参数无效或白名单已满
     * @details 检查白名单是否已满和是否已存在，然后添加
     */
    bool addToWhitelist(const char* epc);
    
    /**
     * @brief 从白名单移除EPC
     * @param epc 标签EPC字符串
     * @return true-移除成功，false-参数无效或EPC不在白名单中
     * @details 查找EPC并移除，后面的元素前移
     */
    bool removeFromWhitelist(const char* epc);
    
    /**
     * @brief 检查EPC是否在白名单中
     * @param epc 标签EPC字符串
     * @return true-在白名单中，false-不在白名单中
     * @details 遍历白名单进行字符串比较
     */
    bool isInWhitelist(const char* epc);
    
    /**
     * @brief 获取白名单数量
     * @return 白名单中EPC的数量
     */
    int getWhitelistCount();
    
    /**
     * @brief 获取白名单指定位置的EPC
     * @param index 索引位置(0-based)
     * @param epc 输出参数，存储EPC字符串
     * @param maxLen epc缓冲区最大长度
     * @return true-获取成功，false-索引无效或参数为空
     */
    bool getWhitelistItem(int index, char* epc, int maxLen);
    
    /**
     * @brief 清空白名单
     * @return true-清空成功
     * @details 重置计数为0，清空白名单数组
     */
    bool clearWhitelist();
    
    /**
     * @brief 保存白名单到Flash
     * @return true-保存成功，false-保存失败
     * @details 将白名单数量和每个EPC写入Preferences
     */
    bool saveWhitelist();
    
    /**
     * @brief 从Flash加载白名单
     * @return true-加载成功，false-加载失败
     * @details 从Preferences读取白名单数量和每个EPC
     */
    bool loadWhitelist();
    
private:
    /**
     * @var _config
     * @brief 系统配置结构体实例
     */
    SystemConfig _config;
    
    /**
     * @var _whitelist
     * @brief 白名单数组，存储标签EPC
     */
    char _whitelist[MAX_WHITELIST_SIZE][MAX_EPC_LENGTH];
    
    /**
     * @var _whitelistCount
     * @brief 当前白名单数量
     */
    int _whitelistCount;
    
    /**
     * @var _prefs
     * @brief Preferences实例，用于Flash持久化存储
     */
    Preferences _prefs;
    
    /**
     * @brief 初始化默认配置
     * @details 设置所有配置参数为默认值，包括MQTT服务器、端口、主题等
     */
    void initDefaultConfig();
};

#endif