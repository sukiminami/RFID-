/**
 * @file CommandProcessor.cpp
 * @brief MQTT命令处理器实现文件
 * @details 实现CommandProcessor类的所有成员函数，处理MQTT命令的解析和执行。
 */

#include "CommandProcessor.h"
#include "ConfigManager.h"
#include "DeviceControl.h"
#include "OtaManager.h"
#include "NetworkManager.h"
#include "CommandHandler.h"

/**
 * @var networkReady
 * @brief 外部引用的全局网络就绪标志
 */
extern bool networkReady;

/**
 * @brief 构造函数
 * @param configManager 配置管理器指针
 * @param deviceControl 设备控制模块指针
 * @param otaManager OTA管理器指针
 * @param networkManager 网络管理器指针
 * @details 使用初始化列表保存各模块指针，便于后续命令执行时调用
 */
CommandProcessor::CommandProcessor(ConfigManager* configManager, DeviceControl* deviceControl, 
                                   OtaManager* otaManager, NetworkManager* networkManager)
    : _configManager(configManager), _deviceControl(deviceControl),
      _otaManager(otaManager), _networkManager(networkManager) {
}

/**
 * @brief 处理接收到的MQTT命令
 * @param jsonStr JSON格式的命令字符串
 * @return true-命令已处理，false-命令无法识别或解析失败
 * @details 通过字符串查找方式解析JSON中的cmd字段，根据命令类型分发到对应的处理函数：
 *          - 白名单命令: add_whitelist/remove_whitelist/query_whitelist/clear_whitelist
 *          - 配置命令: save_config/reset_config
 *          - 报警命令: stop_alarm
 *          - OTA命令: ota_update/ota_status/ota_check/ota_github
 *          - 标签命令: write_tag/lock_tag/destroy_tag
 */
bool CommandProcessor::processCommand(const char* jsonStr) {
    // 打印接收到的命令(最多128字符)
    Serial0.print("[网络] 收到命令: ");
    for (unsigned int i = 0; i < strlen(jsonStr) && i < 128; i++) {
        Serial0.print((char)jsonStr[i]);
    }
    Serial0.println();
    
    // 查找命令类型字段 "\"cmd\":\""
    const char* cmdStart = strstr(jsonStr, "\"cmd\":\"");
    if (cmdStart) {
        // 跳过 "\"cmd\":\"" 前缀(7个字符)
        cmdStart += 7;
        // 查找命令结束的引号
        const char* cmdEnd = strstr(cmdStart, "\"");
        if (cmdEnd) {
            // 提取命令字符串
            char cmd[32];
            strncpy(cmd, cmdStart, cmdEnd - cmdStart);
            cmd[cmdEnd - cmdStart] = '\0';
            
            // 根据命令类型分发处理
            if (strcmp(cmd, "add_whitelist") == 0 || 
                strcmp(cmd, "remove_whitelist") == 0 ||
                strcmp(cmd, "query_whitelist") == 0 ||
                strcmp(cmd, "clear_whitelist") == 0) {
                return processWhitelistCommand(cmd, jsonStr);
            } else if (strcmp(cmd, "save_config") == 0 ||
                       strcmp(cmd, "reset_config") == 0) {
                return processConfigCommand(cmd);
            } else if (strcmp(cmd, "stop_alarm") == 0) {
                return processAlarmCommand(cmd);
            } else if (strcmp(cmd, "ota_update") == 0 ||
                       strcmp(cmd, "ota_status") == 0 ||
                       strcmp(cmd, "ota_check") == 0 ||
                       strcmp(cmd, "ota_github") == 0) {
                return processOtaCommand(cmd, jsonStr);
            } else if (strcmp(cmd, "write_tag") == 0 ||
                       strcmp(cmd, "lock_tag") == 0 ||
                       strcmp(cmd, "destroy_tag") == 0) {
                return processTagCommand(cmd, jsonStr);
            }
        }
    }
    
    // 命令无法识别
    return false;
}

/**
 * @brief 处理白名单相关命令
 * @param cmd 命令类型
 * @param jsonStr 完整的JSON命令字符串
 * @return true-命令处理成功，false-处理失败或参数错误
 * @details 支持四种白名单命令：
 *          - add_whitelist: 添加EPC到白名单
 *          - remove_whitelist: 从白名单移除EPC
 *          - query_whitelist: 查询白名单列表
 *          - clear_whitelist: 清空白名单
 */
bool CommandProcessor::processWhitelistCommand(const char* cmd, const char* jsonStr) {
    // 添加白名单命令
    if (strcmp(cmd, "add_whitelist") == 0) {
        // 提取epc参数
        const char* epcStart = strstr(jsonStr, "\"epc\":\"");
        if (epcStart) {
            epcStart += 7;
            const char* epcEnd = strstr(epcStart, "\"");
            if (epcEnd) {
                char epc[64];
                strncpy(epc, epcStart, epcEnd - epcStart);
                epc[epcEnd - epcStart] = '\0';
                
                // 添加到白名单并保存
                _configManager->addToWhitelist(epc);
                _configManager->saveWhitelist();
                
                Serial0.printf("[CMD] 添加白名单成功: %s\n", epc);
                return true;
            }
        }
    } 
    // 移除白名单命令
    else if (strcmp(cmd, "remove_whitelist") == 0) {
        // 提取epc参数
        const char* epcStart = strstr(jsonStr, "\"epc\":\"");
        if (epcStart) {
            epcStart += 7;
            const char* epcEnd = strstr(epcStart, "\"");
            if (epcEnd) {
                char epc[64];
                strncpy(epc, epcStart, epcEnd - epcStart);
                epc[epcEnd - epcStart] = '\0';
                
                // 从白名单移除并保存
                _configManager->removeFromWhitelist(epc);
                _configManager->saveWhitelist();
                
                Serial0.printf("[CMD] 移除白名单成功: %s\n", epc);
                return true;
            }
        }
    } 
    // 清空白名单命令
    else if (strcmp(cmd, "clear_whitelist") == 0) {
        _configManager->clearWhitelist();
        _configManager->saveWhitelist();
        Serial0.println("[CMD] 清空白名单成功");
        return true;
    } 
    // 查询白名单命令
    else if (strcmp(cmd, "query_whitelist") == 0) {
        // 构建白名单查询响应JSON
        char response[512];
        snprintf(response, sizeof(response), "{\"type\":\"whitelist_query\",\"count\":%d,\"items\":[", _configManager->getWhitelistCount());
        
        // 遍历白名单列表
        for (int i = 0; i < _configManager->getWhitelistCount(); i++) {
            char epc[64];
            _configManager->getWhitelistItem(i, epc, sizeof(epc));
            if (i > 0) strncat(response, ",", sizeof(response) - 1);
            strncat(response, "\"", sizeof(response) - 1);
            strncat(response, epc, sizeof(response) - 1);
            strncat(response, "\"", sizeof(response) - 1);
        }
        strncat(response, "]}", sizeof(response) - 1);
        
        Serial0.printf("[CMD] 查询白名单: %s\n", response);
        
        // 网络就绪时发送响应
        if (isNetworkReady()) {
            _networkManager->send((uint8_t*)response, strlen(response));
        }
        return true;
    }
    
    return false;
}

/**
 * @brief 处理配置相关命令
 * @param cmd 命令类型
 * @return true-命令处理成功，false-处理失败
 * @details 支持两种配置命令：
 *          - save_config: 保存当前配置到Flash
 *          - reset_config: 重置配置为默认值
 */
bool CommandProcessor::processConfigCommand(const char* cmd) {
    // 保存配置命令
    if (strcmp(cmd, "save_config") == 0) {
        _configManager->saveConfig();
        _configManager->saveWhitelist();
        Serial0.println("[CMD] 保存配置成功");
        return true;
    } 
    // 重置配置命令
    else if (strcmp(cmd, "reset_config") == 0) {
        _configManager->resetConfig();
        Serial0.println("[CMD] 重置配置成功");
        return true;
    }
    
    return false;
}

/**
 * @brief 处理报警相关命令
 * @param cmd 命令类型
 * @return true-命令处理成功，false-处理失败
 * @details 支持停止报警命令：
 *          - stop_alarm: 停止当前报警(喇叭和LED)
 */
bool CommandProcessor::processAlarmCommand(const char* cmd) {
    // 停止报警命令
    if (strcmp(cmd, "stop_alarm") == 0) {
        _deviceControl->stopAlarm();
        Serial0.println("[CMD] 停止报警");
        return true;
    }
    
    return false;
}

/**
 * @brief 处理OTA升级相关命令
 * @param cmd 命令类型
 * @param jsonStr 完整的JSON命令字符串
 * @return true-命令处理成功，false-处理失败或参数错误
 * @details 支持四种OTA命令：
 *          - ota_update: 从指定URL下载并升级固件
 *          - ota_status: 查询当前OTA状态
 *          - ota_check: 检查GitHub仓库是否有新版本
 *          - ota_github: 从GitHub仓库下载并升级固件
 */
bool CommandProcessor::processOtaCommand(const char* cmd, const char* jsonStr) {
    // 从URL升级命令
    if (strcmp(cmd, "ota_update") == 0) {
        // 提取url参数
        const char* urlStart = strstr(jsonStr, "\"url\":\"");
        if (urlStart) {
            urlStart += 7;
            const char* urlEnd = strstr(urlStart, "\"");
            if (urlEnd) {
                char url[256];
                strncpy(url, urlStart, urlEnd - urlStart);
                url[urlEnd - urlStart] = '\0';
                
                Serial0.printf("[CMD] 开始OTA升级: %s\n", url);
                
                // 发送升级开始通知
                if (isNetworkReady()) {
                    char response[256];
                    snprintf(response, sizeof(response), 
                             "{\"type\":\"ota_status\",\"status\":\"downloading\",\"progress\":0,\"version\":\"%s\"}", 
                             _otaManager->getCurrentVersion());
                    _networkManager->send((uint8_t*)response, strlen(response));
                }
                
                // 执行OTA升级
                _otaManager->startUpdate(url);
                
                // 发送升级结果
                if (isNetworkReady()) {
                    char response[256];
                    snprintf(response, sizeof(response), 
                             "{\"type\":\"ota_status\",\"status\":\"%s\",\"progress\":%d,\"message\":\"%s\"}", 
                             _otaManager->getStatus() == OTA_COMPLETED ? "completed" : 
                             _otaManager->getStatus() == OTA_FAILED ? "failed" : "error",
                             _otaManager->getProgress(),
                             _otaManager->getStatusMessage());
                    _networkManager->send((uint8_t*)response, strlen(response));
                }
                return true;
            }
        }
    } 
    // 查询OTA状态命令
    else if (strcmp(cmd, "ota_status") == 0) {
        // 构建OTA状态响应
        char response[256];
        snprintf(response, sizeof(response), 
                 "{\"type\":\"ota_status\",\"status\":\"%s\",\"progress\":%d,\"current_version\":\"%s\",\"message\":\"%s\"}", 
                 _otaManager->getStatus() == OTA_IDLE ? "idle" :
                 _otaManager->getStatus() == OTA_CHECKING ? "checking" :
                 _otaManager->getStatus() == OTA_DOWNLOADING ? "downloading" :
                 _otaManager->getStatus() == OTA_UPDATING ? "updating" :
                 _otaManager->getStatus() == OTA_COMPLETED ? "completed" : "failed",
                 _otaManager->getProgress(),
                 _otaManager->getCurrentVersion(),
                 _otaManager->getStatusMessage());
        Serial0.printf("[CMD] OTA状态: %s\n", response);
        
        // 发送状态响应
        if (isNetworkReady()) {
            _networkManager->send((uint8_t*)response, strlen(response));
        }
        return true;
    } 
    // 检查GitHub更新命令
    else if (strcmp(cmd, "ota_check") == 0) {
        // 使用ArduinoJson解析JSON
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, jsonStr);
        if (!error) {
            const char* repo = doc["repo"];
            if (repo != nullptr && strlen(repo) > 0) {
                Serial0.printf("[CMD] 检查GitHub更新: %s\n", repo);
                
                // 检查更新
                char latestVersion[32];
                bool hasUpdate = _otaManager->checkUpdate(repo, latestVersion, sizeof(latestVersion));
                
                // 构建检查结果响应
                char response[256];
                snprintf(response, sizeof(response), 
                         "{\"type\":\"ota_check\",\"current_version\":\"%s\",\"latest_version\":\"%s\",\"has_update\":%s}", 
                         _otaManager->getCurrentVersion(),
                         latestVersion,
                         hasUpdate ? "true" : "false");
                
                Serial0.printf("[CMD] 检查结果: %s\n", response);
                
                // 发送检查结果
                if (isNetworkReady()) {
                    _networkManager->send((uint8_t*)response, strlen(response));
                }
                return true;
            } else {
                Serial0.println("[CMD] 缺少repo参数");
            }
        } else {
            Serial0.printf("[CMD] JSON解析失败: %s\n", error.c_str());
        }
    } 
    // 从GitHub升级命令
    else if (strcmp(cmd, "ota_github") == 0) {
        // 使用ArduinoJson解析JSON
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, jsonStr);
        if (!error) {
            const char* repo = doc["repo"];
            if (repo != nullptr && strlen(repo) > 0) {
                const char* assetName = doc["asset"];
                
                Serial0.printf("[CMD] 从GitHub升级: %s, asset: %s\n", repo, assetName ? assetName : "auto");
                
                // 发送检查开始通知
                if (isNetworkReady()) {
                    char response[256];
                    snprintf(response, sizeof(response), 
                             "{\"type\":\"ota_status\",\"status\":\"checking\",\"progress\":0,\"current_version\":\"%s\"}", 
                             _otaManager->getCurrentVersion());
                    _networkManager->send((uint8_t*)response, strlen(response));
                }
                
                // 执行GitHub升级
                bool result = _otaManager->updateFromGithub(repo, assetName);
                Serial0.printf("[OTA] 升级结果: %s, 状态: %d, 消息: %s\n", 
                              result ? "成功" : "失败", 
                              _otaManager->getStatus(), 
                              _otaManager->getStatusMessage());
                
                // 发送升级结果
                if (isNetworkReady()) {
                    char response[256];
                    snprintf(response, sizeof(response), 
                             "{\"type\":\"ota_status\",\"status\":\"%s\",\"progress\":%d,\"message\":\"%s\"}", 
                             _otaManager->getStatus() == OTA_COMPLETED ? "completed" : 
                             _otaManager->getStatus() == OTA_FAILED ? "failed" :
                             _otaManager->getStatus() == OTA_DOWNLOADING ? "downloading" :
                             _otaManager->getStatus() == OTA_CHECKING ? "checking" : "error",
                             _otaManager->getProgress(),
                             _otaManager->getStatusMessage());
                    _networkManager->send((uint8_t*)response, strlen(response));
                }
                return true;
            } else {
                Serial0.println("[CMD] 缺少repo参数");
            }
        } else {
            Serial0.printf("[CMD] JSON解析失败: %s\n", error.c_str());
        }
    }
    
    return false;
}

/**
 * @brief 处理标签操作相关命令
 * @param cmd 命令类型
 * @param jsonStr 完整的JSON命令字符串
 * @return true-命令处理成功，false-处理失败
 * @details 支持三种标签操作命令(预留功能，当前仅解析参数)：
 *          - write_tag: 写入标签数据
 *          - lock_tag: 锁定标签
 *          - destroy_tag: 销毁标签
 */
bool CommandProcessor::processTagCommand(const char* cmd, const char* jsonStr) {
    // 使用ArduinoJson解析JSON
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (!error) {
        // 提取标签操作参数
        const char* password = doc["password"];
        int membank = doc["membank"];
        int address = doc["address"];
        int length = doc["length"];
        
        // 打印标签操作信息(预留功能)
        Serial0.printf("[CMD] 标签操作: %s, password: %s, membank: %d, address: %d, length: %d\n", 
                      cmd, password ? password : "none", membank, address, length);
        return true;
    } else {
        Serial0.printf("[CMD] JSON解析失败: %s\n", error.c_str());
    }
    
    return false;
}

/**
 * @brief 检查网络是否就绪
 * @return true-网络已连接，false-网络未连接
 * @details 获取全局networkReady标志的状态，用于判断是否可以发送MQTT消息
 */
bool CommandProcessor::isNetworkReady() {
    return networkReady;
}