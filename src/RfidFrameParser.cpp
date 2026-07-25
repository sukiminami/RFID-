#include "RfidFrameParser.h"
#include "ConfigManager.h"
#include "DeviceControl.h"
#include "NetworkManager.h"
#include "CommandHandler.h"

extern bool networkReady;

RfidFrameParser::RfidFrameParser(ConfigManager* configManager, DeviceControl* deviceControl, NetworkManager* networkManager)
    : _configManager(configManager), _deviceControl(deviceControl), _networkManager(networkManager) {
}

void RfidFrameParser::parseFrame(uint8_t* buffer, uint16_t len) {
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
    
    switch (frameType) {
        case 0x02:
            Serial0.println("[INFO] 收到通知帧");
            parseNotificationFrame(buffer, len);
            break;
        case 0x01:
            Serial0.println("[INFO] 收到响应帧");
            parseResponseFrame(buffer, len, frameCode, paramLen);
            break;
        case 0x00:
            Serial0.println("[INFO] 收到命令帧");
            parseCommandFrame(frameCode);
            break;
        default:
            Serial0.printf("[WARN] 未知帧类型: 0x%02X\n", frameType);
            break;
    }
}

void RfidFrameParser::parseNotificationFrame(uint8_t* buffer, uint16_t len) {
    uint16_t paramLen = (buffer[6] << 8) | buffer[7];
    
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
                
                bool isWhitelistTag = _configManager->isInWhitelist(epcStr);
                
                Serial0.printf("[TAG] EPC: %s, 白名单状态: %s, 白名单数量: %d\n", 
                              epcStr, isWhitelistTag ? "是" : "否", _configManager->getWhitelistCount());
                
                if (_configManager->getWhitelistCount() == 0) {
                    Serial0.println("[WARN] 白名单为空，所有标签都不会触发报警！请通过MQTT添加白名单");
                }
                
                if (isWhitelistTag && !_deviceControl->isAlarmActive()) {
                    _deviceControl->triggerAlarm(epcStr);
                    Serial0.println("[ALARM] 触发报警 - 喇叭播放 + 灯光闪烁");
                } else if (!isWhitelistTag) {
                    Serial0.println("[TAG] EPC不在白名单中，不触发报警");
                } else if (_deviceControl->isAlarmActive()) {
                    Serial0.println("[TAG] 报警已激活，跳过重复触发");
                }
                
                int rssi = 0;
                uint16_t rssiOffset = 12 + innerLen;
                if (rssiOffset + 2 < len && buffer[rssiOffset] == 0x05) {
                    rssi = (int8_t)buffer[rssiOffset + 2];
                    Serial0.printf("[TAG] RSSI: %d dBm\n", rssi);
                }
                
                bool alarmTriggered = isWhitelistTag && !_deviceControl->isAlarmActive();
                char responseJson[512];
                snprintf(responseJson, sizeof(responseJson), 
                         "{\"type\":\"tag_report\",\"epc\":\"%s\",\"rssi\":%d,\"alarm\":%s,\"in_whitelist\":%s}", 
                         epcStr, rssi, alarmTriggered ? "true" : "false",
                         isWhitelistTag ? "true" : "false");
                
                publishResponse(responseJson);
            }
        }
    }
}

void RfidFrameParser::parseResponseFrame(uint8_t* buffer, uint16_t len, uint8_t frameCode, uint16_t paramLen) {
    uint8_t statusCode = 0xFF;
    if (paramLen >= 3 && buffer[8] == 0x07) {
        statusCode = buffer[10];
        Serial0.printf("[INFO] 状态: 0x%02X (%s)\n", statusCode, "");
    }
    
    char responseJson[512];
    snprintf(responseJson, sizeof(responseJson), 
             "{\"type\":\"response\",\"frame_code\":0x%02X,\"status\":%d,\"status_msg\":\"%s\"}", 
             frameCode, statusCode, "");
    
    publishResponse(responseJson);
}

void RfidFrameParser::parseCommandFrame(uint8_t frameCode) {
    char responseJson[512];
    snprintf(responseJson, sizeof(responseJson), "{\"type\":\"command\",\"frame_code\":0x%02X}", frameCode);
    
    publishResponse(responseJson);
}

void RfidFrameParser::publishResponse(const char* json) {
    if (json[0] != '\0' && networkReady) {
        Serial0.printf("[MQTT] 发布消息: %s\n", json);
        if (_networkManager->send((uint8_t*)json, strlen(json))) {
            Serial0.println("[MQTT] 发布成功");
        } else {
            Serial0.println("[MQTT] 发布失败");
        }
    }
}