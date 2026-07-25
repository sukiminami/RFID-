/**
 * @file RfidFrameParser.cpp
 * @brief RFID帧解析器实现文件
 * @details 实现RfidFrameParser类的所有成员函数，解析RFID读写器返回的帧数据。
 */

#include "RfidFrameParser.h"
#include "ConfigManager.h"
#include "DeviceControl.h"
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
 * @param networkManager 网络管理器指针
 * @details 使用初始化列表保存各模块指针，便于后续解析时调用
 */
RfidFrameParser::RfidFrameParser(ConfigManager* configManager, DeviceControl* deviceControl, NetworkManager* networkManager)
    : _configManager(configManager), _deviceControl(deviceControl), _networkManager(networkManager) {
}

/**
 * @brief 解析RFID帧数据
 * @param buffer 帧数据指针
 * @param len 帧数据长度
 * @details 帧格式说明：
 *          [0-1] 帧头 "RF" (0x52 0x46)
 *          [2]   帧类型: 0x00-命令帧, 0x01-响应帧, 0x02-通知帧
 *          [3-4] 设备地址(大端序)
 *          [5]   帧码
 *          [6-7] 参数长度(大端序)
 *          [8...] 参数数据
 *          [最后] 校验和
 * @note 根据帧类型分发到对应的解析函数
 */
void RfidFrameParser::parseFrame(uint8_t* buffer, uint16_t len) {
    // 帧长度校验：至少需要9字节(帧头2 + 类型1 + 地址2 + 帧码1 + 参数长度2 + 校验和1)
    if (len < 9) {
        Serial0.println("[WARN] 帧长度不足，无法解析");
        return;
    }
    
    // 提取帧结构字段
    uint8_t frameType = buffer[2];      // 帧类型
    uint16_t address = (buffer[3] << 8) | buffer[4];  // 设备地址(大端序)
    uint8_t frameCode = buffer[5];      // 帧码
    uint16_t paramLen = (buffer[6] << 8) | buffer[7]; // 参数长度(大端序)
    uint8_t checksum = buffer[len - 1]; // 校验和
    
    // 打印帧信息
    Serial0.printf("[INFO] 帧类型: 0x%02X, 地址: 0x%04X, 帧码: 0x%02X, 参数长度: %d\n", 
                  frameType, address, frameCode, paramLen);
    
    // 根据帧类型分发解析
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

/**
 * @brief 解析通知帧
 * @param buffer 帧数据指针
 * @param len 帧数据长度
 * @details 通知帧主要用于上报标签检测信息，帧结构：
 *          参数区包含TLV格式数据：
 *          - 类型0x50: 标签信息
 *            - 内部类型0x01: EPC数据
 *            - 内部类型0x05: RSSI值
 * @note 检测到白名单中的标签时触发报警
 */
void RfidFrameParser::parseNotificationFrame(uint8_t* buffer, uint16_t len) {
    // 提取参数长度
    uint16_t paramLen = (buffer[6] << 8) | buffer[7];
    
    // 检查是否包含标签信息(TLV类型0x50)
    if (paramLen >= 6 && buffer[8] == 0x50) {
        // 标签TLV长度
        uint8_t tagTlvLen = buffer[9];
        
        // 验证TLV数据完整性
        if (10 + tagTlvLen <= len && tagTlvLen >= 4) {
            // 内部TLV类型和长度
            uint8_t innerType = buffer[10];
            uint8_t innerLen = buffer[11];
            
            // 检查是否为EPC数据(内部类型0x01)
            if (innerType == 0x01 && 12 + innerLen <= len) {
                // 将EPC字节转换为十六进制字符串
                char epcStr[64] = {0};
                for (int i = 0; i < innerLen && i < 32; i++) {
                    sprintf(epcStr + i * 2, "%02X", buffer[12 + i]);
                }
                
                // 检查EPC是否在白名单中
                bool isWhitelistTag = _configManager->isInWhitelist(epcStr);
                
                // 打印标签信息
                Serial0.printf("[TAG] EPC: %s, 白名单状态: %s, 白名单数量: %d\n", 
                              epcStr, isWhitelistTag ? "是" : "否", _configManager->getWhitelistCount());
                
                // 白名单为空警告
                if (_configManager->getWhitelistCount() == 0) {
                    Serial0.println("[WARN] 白名单为空，所有标签都不会触发报警！请通过MQTT添加白名单");
                }
                
                // 白名单标签且未报警时触发报警
                if (isWhitelistTag && !_deviceControl->isAlarmActive()) {
                    _deviceControl->triggerAlarm(epcStr);
                    Serial0.println("[ALARM] 触发报警 - 喇叭播放 + 灯光闪烁");
                } 
                // 非白名单标签
                else if (!isWhitelistTag) {
                    Serial0.println("[TAG] EPC不在白名单中，不触发报警");
                } 
                // 报警已激活，跳过重复触发
                else if (_deviceControl->isAlarmActive()) {
                    Serial0.println("[TAG] 报警已激活，跳过重复触发");
                }
                
                // 提取RSSI值(如果存在)
                int rssi = 0;
                uint16_t rssiOffset = 12 + innerLen;
                if (rssiOffset + 2 < len && buffer[rssiOffset] == 0x05) {
                    rssi = (int8_t)buffer[rssiOffset + 2];
                    Serial0.printf("[TAG] RSSI: %d dBm\n", rssi);
                }
                
                // 判断是否触发报警
                bool alarmTriggered = isWhitelistTag && !_deviceControl->isAlarmActive();
                
                // 构建标签上报JSON
                char responseJson[512];
                snprintf(responseJson, sizeof(responseJson), 
                         "{\"type\":\"tag_report\",\"epc\":\"%s\",\"rssi\":%d,\"alarm\":%s,\"in_whitelist\":%s}", 
                         epcStr, rssi, alarmTriggered ? "true" : "false",
                         isWhitelistTag ? "true" : "false");
                
                // 发布上报消息
                publishResponse(responseJson);
            }
        }
    }
}

/**
 * @brief 解析响应帧
 * @param buffer 帧数据指针
 * @param len 帧数据长度
 * @param frameCode 帧码
 * @param paramLen 参数长度
 * @details 响应帧包含命令执行结果，提取状态码并构建响应JSON上报
 */
void RfidFrameParser::parseResponseFrame(uint8_t* buffer, uint16_t len, uint8_t frameCode, uint16_t paramLen) {
    // 默认状态码为0xFF(未知)
    uint8_t statusCode = 0xFF;
    
    // 提取状态码(TLV类型0x07)
    if (paramLen >= 3 && buffer[8] == 0x07) {
        statusCode = buffer[10];
        Serial0.printf("[INFO] 状态: 0x%02X\n", statusCode);
    }
    
    // 构建响应JSON
    char responseJson[512];
    snprintf(responseJson, sizeof(responseJson), 
             "{\"type\":\"response\",\"frame_code\":0x%02X,\"status\":%d}", 
             frameCode, statusCode);
    
    // 发布响应消息
    publishResponse(responseJson);
}

/**
 * @brief 解析命令帧
 * @param frameCode 帧码
 * @details 命令帧是读写器发送的请求，构建响应JSON上报(预留功能)
 */
void RfidFrameParser::parseCommandFrame(uint8_t frameCode) {
    // 构建命令响应JSON
    char responseJson[512];
    snprintf(responseJson, sizeof(responseJson), "{\"type\":\"command\",\"frame_code\":0x%02X}", frameCode);
    
    // 发布响应消息
    publishResponse(responseJson);
}

/**
 * @brief 发布响应消息到MQTT
 * @param json JSON格式的响应消息
 * @details 网络就绪时将解析结果发送到MQTT服务器，否则仅打印日志
 */
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