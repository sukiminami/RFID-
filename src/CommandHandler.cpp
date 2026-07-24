#include "CommandHandler.h"

#define RS485_DEBUG 1

CommandHandler::CommandHandler(Rs485Comm* rs485) : _rs485(rs485) {
}

uint8_t CommandHandler::calculateChecksum(const uint8_t* buffer, uint16_t len) {
    uint8_t checksum = 0;
    for (uint16_t i = 0; i < len; i++) {
        checksum += buffer[i];
    }
    return ~checksum + 1;
}

bool CommandHandler::buildFrame(uint8_t frameCode, const uint8_t* params, uint16_t paramLen, uint8_t* outFrame, uint16_t& outLen) {
    if (paramLen > MAX_FRAME_SIZE - 9) {
        #if RS485_DEBUG
        Serial0.println("[CMD] 错误: 帧长度过大");
        #endif
        return false;
    }
    
    uint16_t len = 0;
    outFrame[len++] = 0x52;
    outFrame[len++] = 0x46;
    outFrame[len++] = 0x00;
    outFrame[len++] = (DEVICE_ADDRESS >> 8) & 0xFF;
    outFrame[len++] = DEVICE_ADDRESS & 0xFF;
    outFrame[len++] = frameCode;
    outFrame[len++] = (paramLen >> 8) & 0xFF;
    outFrame[len++] = paramLen & 0xFF;
    
    if (params && paramLen > 0) {
        for (uint16_t i = 0; i < paramLen; i++) {
            outFrame[len++] = params[i];
        }
    }
    
    outFrame[len++] = calculateChecksum(outFrame, len);
    outLen = len;
    
    return true;
}

bool CommandHandler::sendFrame(uint8_t frameCode, const uint8_t* params, uint16_t paramLen) {
    uint8_t frame[MAX_FRAME_SIZE];
    uint16_t len = 0;
    
    if (!buildFrame(frameCode, params, paramLen, frame, len)) {
        return false;
    }
    
    #if RS485_DEBUG
    Serial0.printf("[CMD] 发送帧 0x%02X, 长度=%d: ", frameCode, len);
    for (uint16_t i = 0; i < len; i++) {
        Serial0.printf("%02X ", frame[i]);
    }
    Serial0.println();
    #endif
    
    return _rs485->send(frame, len);
}

bool CommandHandler::queryVersion() {
    return sendFrame(0x40);
}

bool CommandHandler::startInventory() {
    return sendFrame(0x21);
}

bool CommandHandler::stopInventory() {
    return sendFrame(0x23);
}

bool CommandHandler::singleInventory() {
    return sendFrame(0x22);
}

bool CommandHandler::setPower(uint8_t power) {
    uint8_t tlv[6];
    tlv[0] = 0x26;
    tlv[1] = 0x03;
    tlv[2] = 0x01;
    tlv[3] = power;
    return sendFrame(0x48, tlv, 4);
}

bool CommandHandler::setBeep(bool enable) {
    uint8_t tlv[5];
    tlv[0] = 0x26;
    tlv[1] = 0x03;
    tlv[2] = 0x02;
    tlv[3] = enable ? 0x01 : 0x00;
    return sendFrame(0x48, tlv, 4);
}

bool CommandHandler::setFilterTime(uint8_t seconds) {
    uint8_t tlv[5];
    tlv[0] = 0x26;
    tlv[1] = 0x03;
    tlv[2] = 0x03;
    tlv[3] = seconds;
    return sendFrame(0x48, tlv, 4);
}

bool CommandHandler::queryParam(uint8_t paramType) {
    uint8_t tlv[4];
    tlv[0] = 0x26;
    tlv[1] = 0x02;
    tlv[2] = paramType;
    return sendFrame(0x49, tlv, 3);
}

bool CommandHandler::setWorkParams(uint8_t power, uint8_t interval, uint8_t mode, uint8_t membank, 
                                   uint8_t startAddr, uint8_t length, uint8_t filterTime, 
                                   uint16_t deviceAddr, bool beepEnable, uint16_t antennaFlag) {
    uint8_t tlv[20];
    uint16_t len = 0;
    
    tlv[len++] = 0x23;
    tlv[len++] = 0x0F;
    tlv[len++] = 0x05;
    tlv[len++] = power;
    tlv[len++] = interval;
    tlv[len++] = mode;
    tlv[len++] = membank;
    tlv[len++] = startAddr;
    tlv[len++] = length;
    tlv[len++] = filterTime;
    tlv[len++] = (deviceAddr >> 8) & 0xFF;
    tlv[len++] = deviceAddr & 0xFF;
    tlv[len++] = beepEnable ? 0x01 : 0x00;
    tlv[len++] = 0x00;
    tlv[len++] = 0x00;
    tlv[len++] = (antennaFlag >> 8) & 0xFF;
    tlv[len++] = antennaFlag & 0xFF;
    
    return sendFrame(0x41, tlv, len);
}

bool CommandHandler::reboot() {
    return sendFrame(0x10);
}

bool CommandHandler::readTag(uint32_t password, uint8_t membank, uint8_t address, uint8_t length) {
    uint8_t tlv[12];
    uint16_t len = 0;
    
    tlv[len++] = 0x08;
    tlv[len++] = 0x08;
    tlv[len++] = (password >> 24) & 0xFF;
    tlv[len++] = (password >> 16) & 0xFF;
    tlv[len++] = (password >> 8) & 0xFF;
    tlv[len++] = password & 0xFF;
    tlv[len++] = 0x00;
    tlv[len++] = membank;
    tlv[len++] = address;
    tlv[len++] = length;
    
    return sendFrame(0x31, tlv, len);
}

bool CommandHandler::writeTag(uint32_t password, uint8_t membank, uint8_t address, uint8_t length, const uint8_t* data) {
    uint16_t dataLen = length * 2;
    if (dataLen > MAX_FRAME_SIZE - 20) {
        return false;
    }
    
    uint8_t tlv[MAX_FRAME_SIZE];
    uint16_t len = 0;
    
    tlv[len++] = 0x08;
    tlv[len++] = 0x08 + dataLen;
    tlv[len++] = (password >> 24) & 0xFF;
    tlv[len++] = (password >> 16) & 0xFF;
    tlv[len++] = (password >> 8) & 0xFF;
    tlv[len++] = password & 0xFF;
    tlv[len++] = 0x01;
    tlv[len++] = membank;
    tlv[len++] = address;
    tlv[len++] = length;
    
    for (uint16_t i = 0; i < dataLen; i++) {
        tlv[len++] = data[i];
    }
    
    return sendFrame(0x30, tlv, len);
}

bool CommandHandler::lockTag(uint32_t password, uint8_t membank, uint8_t address, uint8_t length) {
    uint8_t tlv[14];
    uint16_t len = 0;
    
    tlv[len++] = 0x08;
    tlv[len++] = 0x08;
    tlv[len++] = (password >> 24) & 0xFF;
    tlv[len++] = (password >> 16) & 0xFF;
    tlv[len++] = (password >> 8) & 0xFF;
    tlv[len++] = password & 0xFF;
    tlv[len++] = 0x02;
    tlv[len++] = membank;
    tlv[len++] = address;
    tlv[len++] = length;
    
    return sendFrame(0x30, tlv, len);
}

bool CommandHandler::destroyTag(uint32_t password) {
    uint8_t tlv[14];
    uint16_t len = 0;
    
    tlv[len++] = 0x08;
    tlv[len++] = 0x08;
    tlv[len++] = (password >> 24) & 0xFF;
    tlv[len++] = (password >> 16) & 0xFF;
    tlv[len++] = (password >> 8) & 0xFF;
    tlv[len++] = password & 0xFF;
    tlv[len++] = 0x03;
    tlv[len++] = 0x00;
    tlv[len++] = 0x00;
    tlv[len++] = 0x00;
    
    return sendFrame(0x30, tlv, len);
}

bool CommandHandler::controlRelay(uint8_t relayNo, bool open, uint8_t duration) {
    uint8_t tlv[5];
    tlv[0] = 0x27;
    tlv[1] = 0x03;
    tlv[2] = relayNo;
    tlv[3] = open ? 0x01 : 0x00;
    tlv[4] = duration;
    return sendFrame(0x4C, tlv, 5);
}

bool CommandHandler::playAudio(const char* text) {
    uint16_t textLen = strlen(text);
    if (textLen > MAX_FRAME_SIZE - 8) {
        return false;
    }
    
    uint8_t tlv[MAX_FRAME_SIZE];
    uint16_t len = 0;
    
    tlv[len++] = 0x28;
    tlv[len++] = 0x01 + textLen;
    tlv[len++] = 0x01;
    
    for (uint16_t i = 0; i < textLen; i++) {
        tlv[len++] = text[i];
    }
    
    return sendFrame(0x4D, tlv, len);
}

bool CommandHandler::parseMqttCommand(const char* jsonStr) {
    const char* cmdStart = strstr(jsonStr, "\"cmd\":\"");
    if (!cmdStart) {
        #if RS485_DEBUG
        Serial0.println("[CMD] 错误: 未找到命令");
        #endif
        return false;
    }
    
    cmdStart += 7;
    const char* cmdEnd = strstr(cmdStart, "\"");
    if (!cmdEnd) {
        return false;
    }
    
    char cmd[32];
    strncpy(cmd, cmdStart, cmdEnd - cmdStart);
    cmd[cmdEnd - cmdStart] = '\0';
    
    #if RS485_DEBUG
    Serial0.printf("[CMD] 收到MQTT命令: %s\n", cmd);
    #endif
    
    if (strcmp(cmd, "query_version") == 0) {
        return queryVersion();
    }
    else if (strcmp(cmd, "start_inventory") == 0) {
        return startInventory();
    }
    else if (strcmp(cmd, "stop_inventory") == 0) {
        return stopInventory();
    }
    else if (strcmp(cmd, "single_inventory") == 0) {
        return singleInventory();
    }
    else if (strcmp(cmd, "reboot") == 0) {
        return reboot();
    }
    else if (strcmp(cmd, "set_power") == 0) {
        const char* valStart = strstr(jsonStr, "\"power\":");
        if (valStart) {
            uint8_t power = atoi(valStart + 8);
            return setPower(power);
        }
    }
    else if (strcmp(cmd, "set_beep") == 0) {
        const char* valStart = strstr(jsonStr, "\"enable\":");
        if (valStart) {
            bool enable = atoi(valStart + 9) != 0;
            return setBeep(enable);
        }
    }
    else if (strcmp(cmd, "set_filter_time") == 0) {
        const char* valStart = strstr(jsonStr, "\"seconds\":");
        if (valStart) {
            uint8_t seconds = atoi(valStart + 10);
            return setFilterTime(seconds);
        }
    }
    else if (strcmp(cmd, "query_param") == 0) {
        const char* valStart = strstr(jsonStr, "\"param_type\":");
        if (valStart) {
            uint8_t paramType = atoi(valStart + 12);
            return queryParam(paramType);
        }
    }
    else if (strcmp(cmd, "set_work_params") == 0) {
        uint8_t power = 30, interval = 10, mode = 0, membank = 1;
        uint8_t startAddr = 0, length = 0, filterTime = 0;
        uint16_t deviceAddr = DEVICE_ADDRESS;
        bool beepEnable = false;
        uint16_t antennaFlag = 1;
        
        const char* p;
        if ((p = strstr(jsonStr, "\"power\":"))) power = atoi(p + 8);
        if ((p = strstr(jsonStr, "\"interval\":"))) interval = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"mode\":"))) mode = atoi(p + 7);
        if ((p = strstr(jsonStr, "\"membank\":"))) membank = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"start_addr\":"))) startAddr = atoi(p + 13);
        if ((p = strstr(jsonStr, "\"length\":"))) length = atoi(p + 10);
        if ((p = strstr(jsonStr, "\"filter_time\":"))) filterTime = atoi(p + 13);
        if ((p = strstr(jsonStr, "\"device_addr\":"))) deviceAddr = atoi(p + 13);
        if ((p = strstr(jsonStr, "\"beep\":"))) beepEnable = atoi(p + 7) != 0;
        if ((p = strstr(jsonStr, "\"antenna\":"))) antennaFlag = atoi(p + 10);
        
        return setWorkParams(power, interval, mode, membank, startAddr, length, 
                            filterTime, deviceAddr, beepEnable, antennaFlag);
    }
    else if (strcmp(cmd, "read_tag") == 0) {
        uint32_t password = 0;
        uint8_t membank = 1, address = 0, length = 4;
        
        const char* p;
        if ((p = strstr(jsonStr, "\"password\":"))) password = strtoul(p + 11, NULL, 16);
        if ((p = strstr(jsonStr, "\"membank\":"))) membank = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"address\":"))) address = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"length\":"))) length = atoi(p + 10);
        
        return readTag(password, membank, address, length);
    }
    else if (strcmp(cmd, "write_tag") == 0) {
        uint32_t password = 0;
        uint8_t membank = 1, address = 0, length = 4;
        uint8_t writeData[64] = {0};
        int dataLen = 0;
        
        const char* p;
        if ((p = strstr(jsonStr, "\"password\":"))) password = strtoul(p + 11, NULL, 16);
        if ((p = strstr(jsonStr, "\"membank\":"))) membank = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"address\":"))) address = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"length\":"))) length = atoi(p + 10);
        
        const char* dataStart = strstr(jsonStr, "\"data\":\"");
        if (dataStart) {
            dataStart += 8;
            const char* dataEnd = strstr(dataStart, "\"");
            if (dataEnd) {
                const char* hexPtr = dataStart;
                while (hexPtr < dataEnd && dataLen < sizeof(writeData)) {
                    char hexByte[3] = {0};
                    hexByte[0] = *hexPtr++;
                    if (hexPtr < dataEnd) hexByte[1] = *hexPtr++;
                    writeData[dataLen++] = strtoul(hexByte, NULL, 16);
                }
            }
        }
        
        return writeTag(password, membank, address, length, writeData);
    }
    else if (strcmp(cmd, "lock_tag") == 0) {
        uint32_t password = 0;
        uint8_t membank = 1, address = 0, length = 4;
        
        const char* p;
        if ((p = strstr(jsonStr, "\"password\":"))) password = strtoul(p + 11, NULL, 16);
        if ((p = strstr(jsonStr, "\"membank\":"))) membank = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"address\":"))) address = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"length\":"))) length = atoi(p + 10);
        
        return lockTag(password, membank, address, length);
    }
    else if (strcmp(cmd, "destroy_tag") == 0) {
        uint32_t password = 0;
        
        const char* p;
        if ((p = strstr(jsonStr, "\"password\":"))) password = strtoul(p + 11, NULL, 16);
        
        return destroyTag(password);
    }
    else if (strcmp(cmd, "control_relay") == 0) {
        uint8_t relayNo = 1;
        bool open = true;
        uint8_t duration = 0;
        
        const char* p;
        if ((p = strstr(jsonStr, "\"relay_no\":"))) relayNo = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"open\":"))) open = atoi(p + 7) != 0;
        if ((p = strstr(jsonStr, "\"duration\":"))) duration = atoi(p + 11);
        
        return controlRelay(relayNo, open, duration);
    }
    else if (strcmp(cmd, "play_audio") == 0) {
        const char* textStart = strstr(jsonStr, "\"text\":\"");
        if (textStart) {
            textStart += 8;
            const char* textEnd = strstr(textStart, "\"");
            if (textEnd) {
                char text[128];
                strncpy(text, textStart, textEnd - textStart);
                text[textEnd - textStart] = '\0';
                return playAudio(text);
            }
        }
    }
    
    #if RS485_DEBUG
    Serial0.printf("[CMD] 未知命令: %s\n", cmd);
    #endif
    return false;
}

const char* CommandHandler::getStatusMessage(uint8_t statusCode) {
    switch (statusCode) {
        case 0x00: return "成功";
        case 0x14: return "不支持的参数";
        case 0x15: return "参数长度错误";
        case 0x16: return "参数内容错误";
        case 0x17: return "不支持的命令";
        case 0x18: return "设备地址错误";
        case 0x20: return "校验和错误";
        case 0x21: return "不支持的TLV类型";
        case 0x22: return "Flash错误";
        case 0xFF: return "内部错误";
        default: return "未知错误";
    }
}