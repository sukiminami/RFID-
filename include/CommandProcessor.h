/**
 * @file CommandProcessor.h
 * @brief MQTT命令处理器头文件
 * @details 负责解析和执行从MQTT服务器接收到的远程命令，
 *          支持白名单管理、配置管理、报警控制、OTA升级和标签操作。
 */

#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include <Arduino.h>

// 前向声明依赖的模块类
class ConfigManager;
class DeviceControl;
class OtaManager;
class NetworkManager;

/**
 * @class CommandProcessor
 * @brief MQTT命令处理器类
 * @details 解析JSON格式的MQTT命令，根据命令类型分发到对应的处理函数，
 *          执行白名单管理、配置管理、报警控制、OTA升级等操作。
 */
class CommandProcessor {
public:
    /**
     * @brief 构造函数
     * @param configManager 配置管理器指针
     * @param deviceControl 设备控制模块指针
     * @param otaManager OTA管理器指针
     * @param networkManager 网络管理器指针
     * @details 初始化各模块引用，用于执行命令时调用相应功能
     */
    CommandProcessor(ConfigManager* configManager, DeviceControl* deviceControl, 
                     OtaManager* otaManager, NetworkManager* networkManager);
    
    /**
     * @brief 处理接收到的MQTT命令
     * @param jsonStr JSON格式的命令字符串
     * @return true-命令已处理，false-命令无法识别
     * @details 解析JSON中的cmd字段，根据命令类型分发到相应的处理函数
     */
    bool processCommand(const char* jsonStr);
    
private:
    /**
     * @var _configManager
     * @brief 配置管理器指针
     * @details 用于白名单管理和配置读写操作
     */
    ConfigManager* _configManager;
    
    /**
     * @var _deviceControl
     * @brief 设备控制模块指针
     * @details 用于控制报警、喇叭、LED等设备
     */
    DeviceControl* _deviceControl;
    
    /**
     * @var _otaManager
     * @brief OTA管理器指针
     * @details 用于固件升级、版本检查等操作
     */
    OtaManager* _otaManager;
    
    /**
     * @var _networkManager
     * @brief 网络管理器指针
     * @details 用于发送MQTT响应消息
     */
    NetworkManager* _networkManager;
    
    /**
     * @brief 处理白名单相关命令
     * @param cmd 命令类型(add_whitelist/remove_whitelist/query_whitelist/clear_whitelist)
     * @param jsonStr 完整的JSON命令字符串
     * @return true-命令处理成功，false-处理失败
     */
    bool processWhitelistCommand(const char* cmd, const char* jsonStr);
    
    /**
     * @brief 处理配置相关命令
     * @param cmd 命令类型(save_config/reset_config)
     * @return true-命令处理成功，false-处理失败
     */
    bool processConfigCommand(const char* cmd);
    
    /**
     * @brief 处理报警相关命令
     * @param cmd 命令类型(stop_alarm)
     * @return true-命令处理成功，false-处理失败
     */
    bool processAlarmCommand(const char* cmd);
    
    /**
     * @brief 处理OTA升级相关命令
     * @param cmd 命令类型(ota_update/ota_status/ota_check/ota_github)
     * @param jsonStr 完整的JSON命令字符串
     * @return true-命令处理成功，false-处理失败
     */
    bool processOtaCommand(const char* cmd, const char* jsonStr);
    
    /**
     * @brief 处理标签操作相关命令
     * @param cmd 命令类型(write_tag/lock_tag/destroy_tag)
     * @param jsonStr 完整的JSON命令字符串
     * @return true-命令处理成功，false-处理失败
     */
    bool processTagCommand(const char* cmd, const char* jsonStr);
    
    /**
     * @brief 检查网络是否就绪
     * @return true-网络已连接，false-网络未连接
     * @details 获取全局networkReady标志的状态
     */
    bool isNetworkReady();
};

#endif