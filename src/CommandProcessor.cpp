#include "CommandProcessor.h"
#include "ConfigManager.h"
#include "DeviceControl.h"
#include "OtaManager.h"
#include "NetworkManager.h"
#include "CommandHandler.h"

extern bool networkReady;

CommandProcessor::CommandProcessor(ConfigManager* configManager, DeviceControl* deviceControl, 
                                   OtaManager* otaManager, NetworkManager* networkManager)
    : _configManager(configManager), _deviceControl(deviceControl),
      _otaManager(otaManager), _networkManager(networkManager) {
}

bool CommandProcessor::processCommand(const char* jsonStr) {
    Serial0.print("[网络] 收到命令: ");
    for (unsigned int i = 0; i < strlen(jsonStr) && i < 128; i++) {
        Serial0.print((char)jsonStr[i]);
    }
    Serial0.println();
    
    const char* cmdStart = strstr(jsonStr, "\"cmd\":\"");
    if (cmdStart) {
        cmdStart += 7;
        const char* cmdEnd = strstr(cmdStart, "\"");
        if (cmdEnd) {
            char cmd[32];
            strncpy(cmd, cmdStart, cmdEnd - cmdStart);
            cmd[cmdEnd - cmdStart] = '\0';
            
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
    
    return false;
}

bool CommandProcessor::processWhitelistCommand(const char* cmd, const char* jsonStr) {
    if (strcmp(cmd, "add_whitelist") == 0) {
        const char* epcStart = strstr(jsonStr, "\"epc\":\"");
        if (epcStart) {
            epcStart += 7;
            const char* epcEnd = strstr(epcStart, "\"");
            if (epcEnd) {
                char epc[64];
                strncpy(epc, epcStart, epcEnd - epcStart);
                epc[epcEnd - epcStart] = '\0';
                
                _configManager->addToWhitelist(epc);
                _configManager->saveWhitelist();
                
                Serial0.printf("[CMD] 添加白名单成功: %s\n", epc);
                return true;
            }
        }
    } else if (strcmp(cmd, "remove_whitelist") == 0) {
        const char* epcStart = strstr(jsonStr, "\"epc\":\"");
        if (epcStart) {
            epcStart += 7;
            const char* epcEnd = strstr(epcStart, "\"");
            if (epcEnd) {
                char epc[64];
                strncpy(epc, epcStart, epcEnd - epcStart);
                epc[epcEnd - epcStart] = '\0';
                
                _configManager->removeFromWhitelist(epc);
                _configManager->saveWhitelist();
                
                Serial0.printf("[CMD] 移除白名单成功: %s\n", epc);
                return true;
            }
        }
    } else if (strcmp(cmd, "clear_whitelist") == 0) {
        _configManager->clearWhitelist();
        _configManager->saveWhitelist();
        Serial0.println("[CMD] 清空白名单成功");
        return true;
    } else if (strcmp(cmd, "query_whitelist") == 0) {
        char response[512];
        snprintf(response, sizeof(response), "{\"type\":\"whitelist_query\",\"count\":%d,\"items\":[", _configManager->getWhitelistCount());
        
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
        
        if (isNetworkReady()) {
            _networkManager->send((uint8_t*)response, strlen(response));
        }
        return true;
    }
    
    return false;
}

bool CommandProcessor::processConfigCommand(const char* cmd) {
    if (strcmp(cmd, "save_config") == 0) {
        _configManager->saveConfig();
        _configManager->saveWhitelist();
        Serial0.println("[CMD] 保存配置成功");
        return true;
    } else if (strcmp(cmd, "reset_config") == 0) {
        _configManager->resetConfig();
        Serial0.println("[CMD] 重置配置成功");
        return true;
    }
    
    return false;
}

bool CommandProcessor::processAlarmCommand(const char* cmd) {
    if (strcmp(cmd, "stop_alarm") == 0) {
        _deviceControl->stopAlarm();
        Serial0.println("[CMD] 停止报警");
        return true;
    }
    
    return false;
}

bool CommandProcessor::processOtaCommand(const char* cmd, const char* jsonStr) {
    if (strcmp(cmd, "ota_update") == 0) {
        const char* urlStart = strstr(jsonStr, "\"url\":\"");
        if (urlStart) {
            urlStart += 7;
            const char* urlEnd = strstr(urlStart, "\"");
            if (urlEnd) {
                char url[256];
                strncpy(url, urlStart, urlEnd - urlStart);
                url[urlEnd - urlStart] = '\0';
                
                Serial0.printf("[CMD] 开始OTA升级: %s\n", url);
                
                if (isNetworkReady()) {
                    char response[256];
                    snprintf(response, sizeof(response), 
                             "{\"type\":\"ota_status\",\"status\":\"downloading\",\"progress\":0,\"version\":\"%s\"}", 
                             _otaManager->getCurrentVersion());
                    _networkManager->send((uint8_t*)response, strlen(response));
                }
                
                _otaManager->startUpdate(url);
                
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
    } else if (strcmp(cmd, "ota_status") == 0) {
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
        if (isNetworkReady()) {
            _networkManager->send((uint8_t*)response, strlen(response));
        }
        return true;
    } else if (strcmp(cmd, "ota_check") == 0) {
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, jsonStr);
        if (!error) {
            const char* repo = doc["repo"];
            if (repo != nullptr && strlen(repo) > 0) {
                Serial0.printf("[CMD] 检查GitHub更新: %s\n", repo);
                
                char latestVersion[32];
                bool hasUpdate = _otaManager->checkUpdate(repo, latestVersion, sizeof(latestVersion));
                
                char response[256];
                snprintf(response, sizeof(response), 
                         "{\"type\":\"ota_check\",\"current_version\":\"%s\",\"latest_version\":\"%s\",\"has_update\":%s}", 
                         _otaManager->getCurrentVersion(),
                         latestVersion,
                         hasUpdate ? "true" : "false");
                
                Serial0.printf("[CMD] 检查结果: %s\n", response);
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
    } else if (strcmp(cmd, "ota_github") == 0) {
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, jsonStr);
        if (!error) {
            const char* repo = doc["repo"];
            if (repo != nullptr && strlen(repo) > 0) {
                const char* assetName = doc["asset"];
                
                Serial0.printf("[CMD] 从GitHub升级: %s, asset: %s\n", repo, assetName ? assetName : "auto");
                
                if (isNetworkReady()) {
                    char response[256];
                    snprintf(response, sizeof(response), 
                             "{\"type\":\"ota_status\",\"status\":\"checking\",\"progress\":0,\"current_version\":\"%s\"}", 
                             _otaManager->getCurrentVersion());
                    _networkManager->send((uint8_t*)response, strlen(response));
                }
                
                bool result = _otaManager->updateFromGithub(repo, assetName);
                Serial0.printf("[OTA] 升级结果: %s, 状态: %d, 消息: %s\n", 
                              result ? "成功" : "失败", 
                              _otaManager->getStatus(), 
                              _otaManager->getStatusMessage());
                
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

bool CommandProcessor::processTagCommand(const char* cmd, const char* jsonStr) {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (!error) {
        const char* password = doc["password"];
        int membank = doc["membank"];
        int address = doc["address"];
        int length = doc["length"];
        
        Serial0.printf("[CMD] 标签操作: %s, password: %s, membank: %d, address: %d, length: %d\n", 
                      cmd, password ? password : "none", membank, address, length);
        return true;
    } else {
        Serial0.printf("[CMD] JSON解析失败: %s\n", error.c_str());
    }
    
    return false;
}

bool CommandProcessor::isNetworkReady() {
    return networkReady;
}