#include "rfid_protocol.h"

/**
 * @brief 计算补码校验和（累加所有字节，按位取反+1）
 * @param data 数据指针
 * @param len 数据长度
 * @return 补码校验值
 */
uint8_t rfidCalcChecksum(const uint8_t *data, uint16_t len)
{
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint8_t)(~sum + 1);
}

/**
 * @brief 验证补码校验和
 * @param data 数据指针（包含校验和的完整帧）
 * @param len 数据长度
 * @return 校验是否通过
 */
bool rfidVerifyChecksum(const uint8_t *data, uint16_t len)
{
    if (len < 8) {
        return false;
    }
    
    uint8_t checksum = rfidCalcChecksum(data, len - 1);
    return checksum == data[len - 1];
}

/**
 * @brief 构建通用帧
 * @param buffer 输出缓冲区
 * @param frameType 帧类型（命令/响应/通知）
 * @param deviceAddr 设备地址（2字节，MSB在前）
 * @param frameCode 命令码
 * @param tlvs TLV参数数组
 * @param tlvCount TLV数量
 * @return 帧总长度
 */
uint16_t rfidBuildFrame(uint8_t *buffer, uint8_t frameType, uint16_t deviceAddr, uint8_t frameCode, const RfidTlv *tlvs, uint8_t tlvCount)
{
    uint16_t offset = 0;
    
    buffer[offset++] = RFID_FRAME_HEADER_1;
    buffer[offset++] = RFID_FRAME_HEADER_2;
    buffer[offset++] = frameType;
    buffer[offset++] = (uint8_t)((deviceAddr >> 8) & 0xFF);
    buffer[offset++] = (uint8_t)(deviceAddr & 0xFF);
    buffer[offset++] = frameCode;
    
    uint16_t paramLen = 0;
    for (uint8_t i = 0; i < tlvCount && tlvs; i++) {
        paramLen += 2 + tlvs[i].length;
    }
    
    buffer[offset++] = (uint8_t)((paramLen >> 8) & 0xFF);
    buffer[offset++] = (uint8_t)(paramLen & 0xFF);
    
    for (uint8_t i = 0; i < tlvCount && tlvs; i++) {
        buffer[offset++] = tlvs[i].type;
        buffer[offset++] = tlvs[i].length;
        if (tlvs[i].length > 0) {
            memcpy(buffer + offset, tlvs[i].value, tlvs[i].length);
            offset += tlvs[i].length;
        }
    }
    
    buffer[offset++] = rfidCalcChecksum(buffer, offset);
    
    return offset;
}

/**
 * @brief 在缓冲区中搜索帧头起始位置
 * @param buffer 输入数据缓冲区
 * @param len 数据长度
 * @return 帧头起始位置，未找到返回-1
 */
int32_t rfidFindFrameStart(const uint8_t *buffer, uint16_t len)
{
    for (uint16_t i = 0; i < len - 1; i++) {
        if (buffer[i] == RFID_FRAME_HEADER_1 && buffer[i + 1] == RFID_FRAME_HEADER_2) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 解析帧数据
 * @param buffer 输入数据缓冲区
 * @param len 数据长度
 * @param frame 解析结果
 * @return 解析是否成功
 */
bool rfidParseFrame(const uint8_t *buffer, uint16_t len, RfidFrame *frame)
{
    Serial0.printf("[RFID] Trying to parse %d bytes: ", len);
    for (uint16_t i = 0; i < len; i++) {
        Serial0.printf("%02X ", buffer[i]);
    }
    Serial0.println();
    
    if (len < 8) {
        Serial0.printf("[RFID] Frame too short: %d bytes\n", len);
        return false;
    }
    
    int32_t startPos = rfidFindFrameStart(buffer, len);
    if (startPos < 0) {
        Serial0.println("[RFID] Frame header not found in buffer");
        return false;
    }
    
    if (startPos > 0) {
        Serial0.printf("[RFID] Skipped %d bytes before frame header\n", startPos);
    }
    
    const uint8_t *data = buffer + startPos;
    uint16_t dataLen = len - startPos;
    
    if (dataLen < 8) {
        Serial0.printf("[RFID] Frame too short after header: %d bytes\n", dataLen);
        return false;
    }
    
    if (!rfidVerifyChecksum(data, dataLen)) {
        Serial0.println("[RFID] Checksum verification failed");
        return false;
    }
    
    uint16_t offset = 0;
    
    frame->header[0] = data[offset++];
    frame->header[1] = data[offset++];
    frame->frameType = data[offset++];
    frame->deviceAddr = (uint16_t)data[offset++] << 8 | data[offset++];
    frame->frameCode = data[offset++];
    frame->paramLen = (uint16_t)data[offset++] << 8 | data[offset++];
    
    frame->tlvCount = 0;
    uint16_t paramOffset = offset;
    
    while (paramOffset < offset + frame->paramLen && frame->tlvCount < RFID_MAX_TLV_COUNT) {
        if (paramOffset + 2 > dataLen - 1) {
            break;
        }
        
        uint8_t tlvType = data[paramOffset++];
        uint8_t tlvLen = data[paramOffset++];
        
        if (paramOffset + tlvLen > dataLen - 1) {
            break;
        }
        
        frame->tlvs[frame->tlvCount].type = tlvType;
        frame->tlvs[frame->tlvCount].length = tlvLen;
        
        if (tlvLen > 0) {
            memcpy(frame->tlvs[frame->tlvCount].value, data + paramOffset, tlvLen);
        }
        
        paramOffset += tlvLen;
        frame->tlvCount++;
    }
    
    frame->checksum = data[dataLen - 1];
    
    Serial0.printf("[RFID] Frame parsed: Type=0x%02X, Addr=0x%04X, Code=0x%02X, ParamLen=%d\n",
                  frame->frameType, frame->deviceAddr, frame->frameCode, frame->paramLen);
    
    return true;
}

/**
 * @brief 从响应帧中提取数据
 * @param frame 帧数据
 * @param response 响应数据
 * @return 提取是否成功
 */
bool rfidExtractResponse(const RfidFrame *frame, RfidResponse *response)
{
    response->tagCount = 0;
    response->version[0] = '\0';
    
    for (uint8_t i = 0; i < frame->tlvCount; i++) {
        switch (frame->tlvs[i].type) {
            case RFID_TLV_STATUS:
                if (frame->tlvs[i].length >= 1) {
                    response->status = frame->tlvs[i].value[0];
                }
                break;
                
            case RFID_TLV_TAG: {
                if (response->tagCount >= RFID_MAX_TAG_COUNT) {
                    break;
                }
                
                RfidTagInfo *tag = &response->tags[response->tagCount];
                tag->epcLen = 0;
                tag->rssi = 0;
                memset(tag->timestamp, 0, 6);
                
                uint16_t offset = 0;
                while (offset < frame->tlvs[i].length) {
                    if (offset + 2 > frame->tlvs[i].length) {
                        break;
                    }
                    
                    uint8_t subType = frame->tlvs[i].value[offset++];
                    uint8_t subLen = frame->tlvs[i].value[offset++];
                    
                    if (offset + subLen > frame->tlvs[i].length) {
                        break;
                    }
                    
                    switch (subType) {
                        case RFID_TLV_SUB_EPC:
                            tag->epcLen = subLen;
                            memcpy(tag->epc, frame->tlvs[i].value + offset, subLen);
                            break;
                            
                        case RFID_TLV_SUB_RSSI:
                            if (subLen >= 1) {
                                tag->rssi = (int8_t)frame->tlvs[i].value[offset];
                            }
                            break;
                            
                        case RFID_TLV_SUB_TIMESTAMP:
                            memcpy(tag->timestamp, frame->tlvs[i].value + offset, subLen > 6 ? 6 : subLen);
                            break;
                    }
                    
                    offset += subLen;
                }
                
                response->tagCount++;
                break;
            }
            
            default:
                break;
        }
    }
    
    return true;
}

/**
 * @brief 从通知帧中提取数据
 * @param frame 帧数据
 * @param response 响应数据
 * @return 提取是否成功
 */
bool rfidExtractNotification(const RfidFrame *frame, RfidResponse *response)
{
    return rfidExtractResponse(frame, response);
}

/**
 * @brief 构建单次盘点帧
 * @param buffer 输出缓冲区
 * @param deviceAddr 设备地址
 * @return 帧总长度
 */
uint16_t rfidBuildSingleInventoryFrame(uint8_t *buffer, uint16_t deviceAddr)
{
    return rfidBuildFrame(buffer, RFID_FRAME_TYPE_COMMAND, deviceAddr, RFID_CMD_SINGLE_INVENTORY, NULL, 0);
}

/**
 * @brief 构建持续盘点帧
 * @param buffer 输出缓冲区
 * @param deviceAddr 设备地址
 * @return 帧总长度
 */
uint16_t rfidBuildContinuousInventoryFrame(uint8_t *buffer, uint16_t deviceAddr)
{
    return rfidBuildFrame(buffer, RFID_FRAME_TYPE_COMMAND, deviceAddr, RFID_CMD_CONTINUOUS_INVENTORY, NULL, 0);
}

/**
 * @brief 构建停止盘点帧
 * @param buffer 输出缓冲区
 * @param deviceAddr 设备地址
 * @return 帧总长度
 */
uint16_t rfidBuildStopInventoryFrame(uint8_t *buffer, uint16_t deviceAddr)
{
    return rfidBuildFrame(buffer, RFID_FRAME_TYPE_COMMAND, deviceAddr, RFID_CMD_STOP_INVENTORY, NULL, 0);
}

/**
 * @brief 构建读标签帧
 * @param buffer 输出缓冲区
 * @param deviceAddr 设备地址
 * @param bank 内存分区
 * @param startAddr 起始地址（Word）
 * @param wordCount 读取字数
 * @param accessPassword 访问密码（4字节，NULL表示无密码）
 * @return 帧总长度
 */
uint16_t rfidBuildReadFrame(uint8_t *buffer, uint16_t deviceAddr, RfidMemoryBank bank, uint16_t startAddr, uint16_t wordCount, const uint8_t *accessPassword)
{
    RfidTlv tlv;
    tlv.type = RFID_TLV_OPERATION;
    tlv.length = 7 + (accessPassword ? 4 : 0);
    
    uint8_t offset = 0;
    tlv.value[offset++] = RFID_OP_READ;
    tlv.value[offset++] = bank;
    tlv.value[offset++] = (uint8_t)((startAddr >> 8) & 0xFF);
    tlv.value[offset++] = (uint8_t)(startAddr & 0xFF);
    tlv.value[offset++] = (uint8_t)((wordCount >> 8) & 0xFF);
    tlv.value[offset++] = (uint8_t)(wordCount & 0xFF);
    tlv.value[offset++] = accessPassword ? 0x01 : 0x00;
    
    if (accessPassword) {
        memcpy(tlv.value + offset, accessPassword, 4);
    }
    
    return rfidBuildFrame(buffer, RFID_FRAME_TYPE_COMMAND, deviceAddr, RFID_CMD_READ_TAG, &tlv, 1);
}

/**
 * @brief 构建写标签帧
 * @param buffer 输出缓冲区
 * @param deviceAddr 设备地址
 * @param bank 内存分区
 * @param startAddr 起始地址（Word）
 * @param data 待写入数据
 * @param wordCount 写入字数
 * @param accessPassword 访问密码（4字节，NULL表示无密码）
 * @return 帧总长度
 */
uint16_t rfidBuildWriteFrame(uint8_t *buffer, uint16_t deviceAddr, RfidMemoryBank bank, uint16_t startAddr, const uint8_t *data, uint16_t wordCount, const uint8_t *accessPassword)
{
    RfidTlv tlv;
    tlv.type = RFID_TLV_OPERATION;
    tlv.length = 7 + wordCount * 2 + (accessPassword ? 4 : 0);
    
    uint8_t offset = 0;
    tlv.value[offset++] = RFID_OP_WRITE;
    tlv.value[offset++] = bank;
    tlv.value[offset++] = (uint8_t)((startAddr >> 8) & 0xFF);
    tlv.value[offset++] = (uint8_t)(startAddr & 0xFF);
    tlv.value[offset++] = (uint8_t)((wordCount >> 8) & 0xFF);
    tlv.value[offset++] = (uint8_t)(wordCount & 0xFF);
    tlv.value[offset++] = accessPassword ? 0x01 : 0x00;
    
    if (data && wordCount > 0) {
        memcpy(tlv.value + offset, data, wordCount * 2);
        offset += wordCount * 2;
    }
    
    if (accessPassword) {
        memcpy(tlv.value + offset, accessPassword, 4);
    }
    
    return rfidBuildFrame(buffer, RFID_FRAME_TYPE_COMMAND, deviceAddr, RFID_CMD_WRITE_TAG, &tlv, 1);
}

/**
 * @brief 构建查询版本帧
 * @param buffer 输出缓冲区
 * @param deviceAddr 设备地址
 * @return 帧总长度
 */
uint16_t rfidBuildQueryVersionFrame(uint8_t *buffer, uint16_t deviceAddr)
{
    return rfidBuildFrame(buffer, RFID_FRAME_TYPE_COMMAND, deviceAddr, RFID_CMD_QUERY_VERSION, NULL, 0);
}

/**
 * @brief 构建设置单参数帧
 * @param buffer 输出缓冲区
 * @param deviceAddr 设备地址
 * @param paramType 参数类型
 * @param value 参数值
 * @param valueLen 值长度
 * @return 帧总长度
 */
uint16_t rfidBuildSetParamFrame(uint8_t *buffer, uint16_t deviceAddr, RfidSingleParamType paramType, const uint8_t *value, uint8_t valueLen)
{
    RfidTlv tlv;
    tlv.type = RFID_TLV_SINGLE_PARAM;
    tlv.length = 1 + valueLen;
    tlv.value[0] = paramType;
    if (value && valueLen > 0) {
        memcpy(tlv.value + 1, value, valueLen);
    }
    
    return rfidBuildFrame(buffer, RFID_FRAME_TYPE_COMMAND, deviceAddr, RFID_CMD_SET_SINGLE_PARAM, &tlv, 1);
}

/**
 * @brief 打印十六进制数据
 * @param data 数据指针
 * @param len 数据长度
 */
void rfidPrintHex(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        Serial0.printf("%02X ", data[i]);
    }
    Serial0.println();
}

void rfidPrintTlv(const RfidTlv *tlv)
{
    Serial0.printf("[RS485]   TLV: Type=0x%02X, Length=%d\n", tlv->type, tlv->length);
}

void rfidPrintFrame(const RfidFrame *frame)
{
    Serial0.printf("[RS485] Frame: Header=%02X%02X, Type=0x%02X, Addr=0x%04X, Code=0x%02X, ParamLen=%d, Checksum=0x%02X\n",
                  frame->header[0], frame->header[1], frame->frameType, frame->deviceAddr,
                  frame->frameCode, frame->paramLen, frame->checksum);
}
