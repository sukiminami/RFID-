#define RS485_DEBUG 1
#include <Arduino.h>
#include <ArduinoJson.h>
#include "rs485.h"
#include "CommandHandler.h"
#include "NetworkManager.h"
#include "DeviceControl.h"
#include "ConfigManager.h"
#include "OtaManager.h"

#define UART_TX_PIN 17
#define UART_RX_PIN 16
#define UART_DE_PIN 21
#define BAUD_RATE 115200

#define SERVER_IP "broker.emqx.io"
#define SERVER_PORT 1883

#define MQTT_CLIENT_ID "rfid_gateway_0001"
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
#define MQTT_TOPIC "SMtest"
#define MQTT_SUB_TOPIC "SMtest/cmd"

#define HEARTBEAT_INTERVAL 300000

#define FIRMWARE_VERSION "1.0.0"

Rs485Comm rs485(&Serial2, UART_TX_PIN, UART_RX_PIN, UART_DE_PIN);
CommandHandler cmdHandler(&rs485);
NetworkManager networkManager;
NetworkConfig networkConfig;
DeviceControl deviceControl;
ConfigManager configManager;
OtaManager otaManager;

uint8_t receiveBuffer[2048];
uint16_t receiveLen = 0;
unsigned long lastReceiveTime = 0;
unsigned long lastSendTime = 0;
unsigned long lastHeartbeatTime = 0;
bool networkReady = false;

void onNetworkStatusChanged(NetworkStatus status, NetworkType type, const char* message, void* userData) {
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
}

void onNetworkDataReceived(const uint8_t* data, uint16_t length, void* userData) {
    Serial0.print("[网络] 收到命令: ");
    for (unsigned int i = 0; i < length && i < 128; i++) {
        Serial0.print((char)data[i]);
    }
    Serial0.println();
    
    char jsonStr[512];
    strncpy(jsonStr, (const char*)data, min((uint16_t)511, length));
    jsonStr[min((uint16_t)511, length)] = '\0';
    
    const char* cmdStart = strstr(jsonStr, "\"cmd\":\"");
    if (cmdStart) {
        cmdStart += 7;
        const char* cmdEnd = strstr(cmdStart, "\"");
        if (cmdEnd) {
            char cmd[32];
            strncpy(cmd, cmdStart, cmdEnd - cmdStart);
            cmd[cmdEnd - cmdStart] = '\0';
            
            if (strcmp(cmd, "add_whitelist") == 0) {
                const char* epcStart = strstr(jsonStr, "\"epc\":\"");
                if (epcStart) {
                    epcStart += 7;
                    const char* epcEnd = strstr(epcStart, "\"");
                    if (epcEnd) {
                        char epc[64];
                        strncpy(epc, epcStart, epcEnd - epcStart);
                        epc[epcEnd - epcStart] = '\0';
                        
                        configManager.addToWhitelist(epc);
                        configManager.saveWhitelist();
                        
                        Serial0.printf("[CMD] 添加白名单成功: %s\n", epc);
                    }
                }
                return;
            } else if (strcmp(cmd, "remove_whitelist") == 0) {
                const char* epcStart = strstr(jsonStr, "\"epc\":\"");
                if (epcStart) {
                    epcStart += 7;
                    const char* epcEnd = strstr(epcStart, "\"");
                    if (epcEnd) {
                        char epc[64];
                        strncpy(epc, epcStart, epcEnd - epcStart);
                        epc[epcEnd - epcStart] = '\0';
                        
                        configManager.removeFromWhitelist(epc);
                        configManager.saveWhitelist();
                        
                        Serial0.printf("[CMD] 移除白名单成功: %s\n", epc);
                    }
                }
                return;
            } else if (strcmp(cmd, "clear_whitelist") == 0) {
                configManager.clearWhitelist();
                configManager.saveWhitelist();
                Serial0.println("[CMD] 清空白名单成功");
                return;
            } else if (strcmp(cmd, "query_whitelist") == 0) {
                char response[512];
                snprintf(response, sizeof(response), "{\"type\":\"whitelist_query\",\"count\":%d,\"items\":[", configManager.getWhitelistCount());
                
                for (int i = 0; i < configManager.getWhitelistCount(); i++) {
                    char epc[64];
                    configManager.getWhitelistItem(i, epc, sizeof(epc));
                    if (i > 0) strncat(response, ",", sizeof(response) - 1);
                    strncat(response, "\"", sizeof(response) - 1);
                    strncat(response, epc, sizeof(response) - 1);
                    strncat(response, "\"", sizeof(response) - 1);
                }
                strncat(response, "]}", sizeof(response) - 1);
                
                Serial0.printf("[CMD] 查询白名单: %s\n", response);
                
                if (networkReady) {
                    networkManager.send((uint8_t*)response, strlen(response));
                }
                return;
            } else if (strcmp(cmd, "save_config") == 0) {
                configManager.saveConfig();
                configManager.saveWhitelist();
                Serial0.println("[CMD] 保存配置成功");
                return;
            } else if (strcmp(cmd, "reset_config") == 0) {
                configManager.resetConfig();
                Serial0.println("[CMD] 重置配置成功");
                return;
            } else if (strcmp(cmd, "stop_alarm") == 0) {
                deviceControl.stopAlarm();
                Serial0.println("[CMD] 停止报警");
                return;
            } else if (strcmp(cmd, "ota_update") == 0) {
                const char* urlStart = strstr(jsonStr, "\"url\":\"");
                if (urlStart) {
                    urlStart += 7;
                    const char* urlEnd = strstr(urlStart, "\"");
                    if (urlEnd) {
                        char url[256];
                        strncpy(url, urlStart, urlEnd - urlStart);
                        url[urlEnd - urlStart] = '\0';
                        
                        Serial0.printf("[CMD] 开始OTA升级: %s\n", url);
                        
                        if (networkReady) {
                            char response[256];
                            snprintf(response, sizeof(response), 
                                     "{\"type\":\"ota_status\",\"status\":\"downloading\",\"progress\":0,\"version\":\"%s\"}", 
                                     otaManager.getCurrentVersion());
                            networkManager.send((uint8_t*)response, strlen(response));
                        }
                        
                        otaManager.startUpdate(url);
                        
                        if (networkReady) {
                            char response[256];
                            snprintf(response, sizeof(response), 
                                     "{\"type\":\"ota_status\",\"status\":\"%s\",\"progress\":%d,\"message\":\"%s\"}", 
                                     otaManager.getStatus() == OTA_COMPLETED ? "completed" : 
                                     otaManager.getStatus() == OTA_FAILED ? "failed" : "error",
                                     otaManager.getProgress(),
                                     otaManager.getStatusMessage());
                            networkManager.send((uint8_t*)response, strlen(response));
                        }
                    }
                }
                return;
            } else if (strcmp(cmd, "ota_status") == 0) {
                char response[256];
                snprintf(response, sizeof(response), 
                         "{\"type\":\"ota_status\",\"status\":\"%s\",\"progress\":%d,\"current_version\":\"%s\",\"message\":\"%s\"}", 
                         otaManager.getStatus() == OTA_IDLE ? "idle" :
                         otaManager.getStatus() == OTA_CHECKING ? "checking" :
                         otaManager.getStatus() == OTA_DOWNLOADING ? "downloading" :
                         otaManager.getStatus() == OTA_UPDATING ? "updating" :
                         otaManager.getStatus() == OTA_COMPLETED ? "completed" : "failed",
                         otaManager.getProgress(),
                         otaManager.getCurrentVersion(),
                         otaManager.getStatusMessage());
                Serial0.printf("[CMD] OTA状态: %s\n", response);
                if (networkReady) {
                    networkManager.send((uint8_t*)response, strlen(response));
                }
                return;
            } else if (strcmp(cmd, "ota_check") == 0) {
                StaticJsonDocument<512> doc;
                DeserializationError error = deserializeJson(doc, jsonStr);
                if (!error) {
                    const char* repo = doc["repo"];
                    if (repo != nullptr && strlen(repo) > 0) {
                        Serial0.printf("[CMD] 检查GitHub更新: %s\n", repo);
                        
                        char latestVersion[32];
                        bool hasUpdate = otaManager.checkUpdate(repo, latestVersion, sizeof(latestVersion));
                        
                        char response[256];
                        snprintf(response, sizeof(response), 
                                 "{\"type\":\"ota_check\",\"current_version\":\"%s\",\"latest_version\":\"%s\",\"has_update\":%s}", 
                                 otaManager.getCurrentVersion(),
                                 latestVersion,
                                 hasUpdate ? "true" : "false");
                        
                        Serial0.printf("[CMD] 检查结果: %s\n", response);
                        if (networkReady) {
                            networkManager.send((uint8_t*)response, strlen(response));
                        }
                    } else {
                        Serial0.println("[CMD] 缺少repo参数");
                    }
                } else {
                    Serial0.printf("[CMD] JSON解析失败: %s\n", error.c_str());
                }
                return;
            } else if (strcmp(cmd, "ota_github") == 0) {
                StaticJsonDocument<512> doc;
                DeserializationError error = deserializeJson(doc, jsonStr);
                if (!error) {
                    const char* repo = doc["repo"];
                    if (repo != nullptr && strlen(repo) > 0) {
                        const char* assetName = doc["asset"];
                        
                        Serial0.printf("[CMD] 从GitHub升级: %s, asset: %s\n", repo, assetName ? assetName : "auto");
                        
                        if (networkReady) {
                            char response[256];
                            snprintf(response, sizeof(response), 
                                     "{\"type\":\"ota_status\",\"status\":\"checking\",\"progress\":0,\"current_version\":\"%s\"}", 
                                     otaManager.getCurrentVersion());
                            networkManager.send((uint8_t*)response, strlen(response));
                        }
                        
                        bool result = otaManager.updateFromGithub(repo, assetName);
                        Serial0.printf("[OTA] 升级结果: %s, 状态: %d, 消息: %s\n", 
                                      result ? "成功" : "失败", 
                                      otaManager.getStatus(), 
                                      otaManager.getStatusMessage());
                        
                        if (networkReady) {
                            char response[256];
                            snprintf(response, sizeof(response), 
                                     "{\"type\":\"ota_status\",\"status\":\"%s\",\"progress\":%d,\"message\":\"%s\"}", 
                                     otaManager.getStatus() == OTA_COMPLETED ? "completed" : 
                                     otaManager.getStatus() == OTA_FAILED ? "failed" :
                                     otaManager.getStatus() == OTA_DOWNLOADING ? "downloading" :
                                     otaManager.getStatus() == OTA_CHECKING ? "checking" : "error",
                                     otaManager.getProgress(),
                                     otaManager.getStatusMessage());
                            networkManager.send((uint8_t*)response, strlen(response));
                        }
                    } else {
                        Serial0.println("[CMD] 缺少repo参数");
                    }
                } else {
                    Serial0.printf("[CMD] JSON解析失败: %s\n", error.c_str());
                }
                return;
            }
        }
    }
    
    if (cmdHandler.parseMqttCommand(jsonStr)) {
        Serial0.println("[CMD] 命令执行成功");
    } else {
        Serial0.println("[CMD] 命令执行失败");
    }
}

void initNetworkConfig() {
    memset(&networkConfig, 0, sizeof(NetworkConfig));
    
    SystemConfig& config = configManager.getConfig();
    
    strncpy(networkConfig.server, config.server, sizeof(networkConfig.server) - 1);
    networkConfig.port = config.port;
    
    networkConfig.useMqtt = true;
    strncpy(networkConfig.mqttClientId, config.clientId, sizeof(networkConfig.mqttClientId) - 1);
    strncpy(networkConfig.mqttUsername, config.username, sizeof(networkConfig.mqttUsername) - 1);
    strncpy(networkConfig.mqttPassword, config.password, sizeof(networkConfig.mqttPassword) - 1);
    strncpy(networkConfig.mqttTopic, config.topic, sizeof(networkConfig.mqttTopic) - 1);
    strncpy(networkConfig.mqttSubTopic, config.subTopic, sizeof(networkConfig.mqttSubTopic) - 1);
    
    networkConfig.autoReconnect = true;
    networkConfig.reconnectInterval = 10000;
}

void parseRfidFrame(uint8_t* buffer, uint16_t len) {
    if (len < 9) {
        Serial0.println("[WARN] 帧长度不足，无法解析");
        return;
    }
    
    uint8_t frameType = buffer[2];
    uint16_t address = (buffer[3] << 8) | buffer[4];
    uint8_t frameCode = buffer[5];
    uint16_t paramLen = (buffer[6] << 8) | buffer[7];
    uint8_t checksum = buffer[len - 1];
    
    Serial0.printf("[INFO] 帧类型: 0x%02X, 地址: 0x%04X, 帧码: 0x%02X, 参数长度: %d\n", 
                  frameType, address, frameCode, paramLen);
    
    char responseJson[512] = {0};
    
    if (frameType == 0x02) {
        Serial0.println("[INFO] 收到通知帧");
        
        if (paramLen >= 6 && buffer[8] == 0x50) {
            uint8_t tagTlvLen = buffer[9];
            
            if (10 + tagTlvLen <= len && tagTlvLen >= 4) {
                uint8_t innerType = buffer[10];
                uint8_t innerLen = buffer[11];
                
                if (innerType == 0x01 && 12 + innerLen <= len) {
                    char epcStr[64] = {0};
                    for (int i = 0; i < innerLen && i < 32; i++) {
                        sprintf(epcStr + i * 2, "%02X", buffer[12 + i]);
                    }
                    
                    bool isWhitelistTag = configManager.isInWhitelist(epcStr);
                    
                    Serial0.printf("[TAG] EPC: %s, 白名单状态: %s, 白名单数量: %d\n", 
                                  epcStr, isWhitelistTag ? "是" : "否", configManager.getWhitelistCount());
                    
                    if (configManager.getWhitelistCount() == 0) {
                        Serial0.println("[WARN] 白名单为空，所有标签都不会触发报警！请通过MQTT添加白名单");
                    }
                    
                    if (isWhitelistTag && !deviceControl.isAlarmActive()) {
                        deviceControl.triggerAlarm(epcStr);
                        Serial0.println("[ALARM] 触发报警 - 喇叭播放 + 灯光闪烁");
                    } else if (!isWhitelistTag) {
                        Serial0.println("[TAG] EPC不在白名单中，不触发报警");
                    } else if (deviceControl.isAlarmActive()) {
                        Serial0.println("[TAG] 报警已激活，跳过重复触发");
                    }
                    
                    int rssi = 0;
                    uint16_t rssiOffset = 12 + innerLen;
                    if (rssiOffset + 2 < len && buffer[rssiOffset] == 0x05) {
                        rssi = (int8_t)buffer[rssiOffset + 2];
                        Serial0.printf("[TAG] RSSI: %d dBm\n", rssi);
                    }
                    
                    bool alarmTriggered = isWhitelistTag && !deviceControl.isAlarmActive();
                    snprintf(responseJson, sizeof(responseJson), 
                             "{\"type\":\"tag_report\",\"epc\":\"%s\",\"rssi\":%d,\"alarm\":%s,\"in_whitelist\":%s}", 
                             epcStr, rssi, alarmTriggered ? "true" : "false",
                             isWhitelistTag ? "true" : "false");
                }
            }
        }
    } else if (frameType == 0x01) {
        Serial0.println("[INFO] 收到响应帧");
        
        uint8_t statusCode = 0xFF;
        if (paramLen >= 3 && buffer[8] == 0x07) {
            statusCode = buffer[10];
            Serial0.printf("[INFO] 状态: 0x%02X (%s)\n", statusCode, cmdHandler.getStatusMessage(statusCode));
        }
        
        snprintf(responseJson, sizeof(responseJson), 
                 "{\"type\":\"response\",\"frame_code\":0x%02X,\"status\":%d,\"status_msg\":\"%s\"}", 
                 frameCode, statusCode, cmdHandler.getStatusMessage(statusCode));
    } else if (frameType == 0x00) {
        Serial0.println("[INFO] 收到命令帧");
        snprintf(responseJson, sizeof(responseJson), "{\"type\":\"command\",\"frame_code\":0x%02X}", frameCode);
    }
    
    if (responseJson[0] != '\0' && networkReady) {
        Serial0.printf("[MQTT] 发布消息: %s\n", responseJson);
        if (networkManager.send((uint8_t*)responseJson, strlen(responseJson))) {
            Serial0.println("[MQTT] 发布成功");
        } else {
            Serial0.println("[MQTT] 发布失败");
        }
    }
}
    
void sendHeartbeat() {
    char json[128];
    snprintf(json, sizeof(json), "{\"type\":\"heartbeat\",\"time\":%lu}", millis() / 1000);
    
    Serial0.printf("[MQTT] 发送心跳: %s\n", json);
    
    if (networkReady) {
        if (networkManager.send((uint8_t*)json, strlen(json))) {
            Serial0.println("[MQTT] 心跳发送成功");
        } else {
            Serial0.println("[MQTT] 心跳发送失败");
        }
    }
}

void setup() {
    Serial0.begin(115200);
    delay(2000);
    
    Serial0.println("========================================");
    Serial0.println("  RFID网关系统22222");
    Serial0.println("========================================");
    
    Serial0.println("[系统] 初始化RS485模块...");
    if (!rs485.begin(BAUD_RATE)) {
        Serial0.println("[系统] RS485初始化失败!");
        while (1);
    }
    Serial0.println("[系统] RS485初始化完成");
    
    Serial0.println("[系统] 初始化设备控制模块...");
    deviceControl.begin();
    Serial0.println("[系统] 设备控制模块初始化完成");
    
    Serial0.println("[系统] 初始化配置管理器...");
    configManager.begin();
    Serial0.println("[系统] 配置管理器初始化完成");
    
    Serial0.println("[系统] 初始化OTA管理器...");
    otaManager.begin();
    otaManager.setCurrentVersion(FIRMWARE_VERSION);
    Serial0.printf("[系统] 当前固件版本: %s\n", FIRMWARE_VERSION);
    Serial0.println("[系统] OTA管理器初始化完成");
    
    Serial0.println("[系统] 初始化网络模块...");
    initNetworkConfig();
    
    if (!networkManager.begin(&networkConfig)) {
        Serial0.println("[系统] 网络管理器初始化失败");
    } else {
        Serial0.println("[系统] 网络管理器初始化成功");
        networkManager.setStatusCallback(onNetworkStatusChanged);
        networkManager.setDataCallback(onNetworkDataReceived);
        Serial0.println("[系统] 开始连接网络...");
        networkManager.connect();
    }
    
    Serial0.println("系统启动完成!");
    Serial0.println("========================================");
}

void loop() {
    networkManager.update();
    deviceControl.update();
    otaManager.update();
    
    if (millis() - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
        lastHeartbeatTime = millis();
        sendHeartbeat();
    }
    
    size_t available = rs485.available();
    if (available > 0) {
        uint16_t toRead = min((uint16_t)(2048 - receiveLen), (uint16_t)available);
        uint8_t tempBuffer[256];
        uint16_t read = rs485.receive(tempBuffer, 500, 50);
        
        for (uint16_t i = 0; i < read && receiveLen < 2048; i++) {
            receiveBuffer[receiveLen++] = tempBuffer[i];
        }
        lastReceiveTime = millis();
        
        Serial0.printf("[RX] 收到 %d 字节, 总计: %d\n", read, receiveLen);
    }
    
    if (receiveLen > 0 && millis() - lastReceiveTime > 100) {
        Serial0.printf("\n[RX] 帧接收完成! 总计 %d 字节: ", receiveLen);
        for (int i = 0; i < receiveLen; i++) {
            Serial0.printf("%02X ", receiveBuffer[i]);
        }
        Serial0.println();
        
        int headerPos = -1;
        for (int i = 0; i < receiveLen - 1; i++) {
            if (receiveBuffer[i] == 0x52 && receiveBuffer[i+1] == 0x46) {
                headerPos = i;
                Serial0.printf("[OK] 帧头 'RF' 位于偏移量 %d\n", i);
                parseRfidFrame(receiveBuffer + i, receiveLen - i);
                break;
            }
        }
        
        if (headerPos < 0) {
            Serial0.println("[WARN] 未找到帧头 'RF' (0x52 0x46)");
        }
        
        receiveLen = 0;
    }
    
    delay(1);
}