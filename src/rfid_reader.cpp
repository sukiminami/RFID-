#include "rfid_reader.h"

/**
 * @brief 构造函数
 * @param serial 硬件串口对象指针
 * @param txPin TX引脚
 * @param rxPin RX引脚
 * @param dePin DE/RE引脚（RS485收发控制）
 */
RfidReader::RfidReader(HardwareSerial *serial, int txPin, int rxPin, int dePin)
    : _rs485(serial, txPin, rxPin, dePin)
{
}

/**
 * @brief 初始化串口通信
 * @param baudRate 波特率，默认115200
 * @return 是否初始化成功
 */
bool RfidReader::begin(uint32_t baudRate)
{
    Serial0.printf("[RFID] Initializing reader - Baud=%lu\n", baudRate);
    return _rs485.begin(baudRate);
}

/**
 * @brief 执行单次盘点操作
 * @param deviceAddr 设备地址（2字节）
 * @param response 盘点响应数据
 * @return 是否操作成功
 */
bool RfidReader::singleInventory(uint16_t deviceAddr, RfidResponse *response)
{
    uint16_t frameLen = rfidBuildSingleInventoryFrame(_sendFrame, deviceAddr);
    
    Serial0.printf("[RFID] Sending single inventory to device 0x%04X: ", deviceAddr);
    rfidPrintHex(_sendFrame, frameLen);
    
    if (!sendFrame(_sendFrame, frameLen)) {
        Serial0.println("[RFID] Send frame failed");
        return false;
    }
    
    uint16_t responseLen = receiveFrame(_readBuffer, RFID_READ_TIMEOUT_MS);
    if (responseLen == 0) {
        Serial0.println("[RFID] No response received");
        return false;
    }
    
    Serial0.printf("[RFID] Received response (%d bytes): ", responseLen);
    for (uint16_t i = 0; i < responseLen; i++) {
        Serial0.printf("%02X ", _readBuffer[i]);
    }
    Serial0.println();
    
    if (!rfidParseFrame(_readBuffer, responseLen, &_parsedFrame)) {
        Serial0.println("[RFID] Failed to parse frame");
        return false;
    }
    
    rfidExtractResponse(&_parsedFrame, response);
    return true;
}

/**
 * @brief 开始持续盘点操作
 * @param deviceAddr 设备地址（2字节）
 * @param response 响应数据
 * @return 是否操作成功
 */
bool RfidReader::startContinuousInventory(uint16_t deviceAddr, RfidResponse *response)
{
    uint16_t frameLen = rfidBuildContinuousInventoryFrame(_sendFrame, deviceAddr);
    
    Serial0.printf("[RFID] Sending continuous inventory to device 0x%04X: ", deviceAddr);
    rfidPrintHex(_sendFrame, frameLen);
    
    if (!sendFrame(_sendFrame, frameLen)) {
        Serial0.println("[RFID] Send frame failed");
        return false;
    }
    
    uint16_t responseLen = receiveFrame(_readBuffer, RFID_READ_TIMEOUT_MS);
    if (responseLen == 0) {
        Serial0.println("[RFID] No response received");
        return false;
    }
    
    Serial0.printf("[RFID] Received response (%d bytes): ", responseLen);
    for (uint16_t i = 0; i < responseLen; i++) {
        Serial0.printf("%02X ", _readBuffer[i]);
    }
    Serial0.println();
    
    if (!rfidParseFrame(_readBuffer, responseLen, &_parsedFrame)) {
        Serial0.println("[RFID] Failed to parse frame");
        return false;
    }
    
    rfidExtractResponse(&_parsedFrame, response);
    return true;
}

/**
 * @brief 停止盘点操作
 * @param deviceAddr 设备地址（2字节）
 * @param response 响应数据
 * @return 是否操作成功
 */
bool RfidReader::stopInventory(uint16_t deviceAddr, RfidResponse *response)
{
    uint16_t frameLen = rfidBuildStopInventoryFrame(_sendFrame, deviceAddr);
    
    Serial0.printf("[RFID] Sending stop inventory to device 0x%04X: ", deviceAddr);
    rfidPrintHex(_sendFrame, frameLen);
    
    if (!sendFrame(_sendFrame, frameLen)) {
        Serial0.println("[RFID] Send frame failed");
        return false;
    }
    
    uint16_t responseLen = receiveFrame(_readBuffer, RFID_READ_TIMEOUT_MS);
    if (responseLen == 0) {
        Serial0.println("[RFID] No response received");
        return false;
    }
    
    Serial0.printf("[RFID] Received response (%d bytes): ", responseLen);
    for (uint16_t i = 0; i < responseLen; i++) {
        Serial0.printf("%02X ", _readBuffer[i]);
    }
    Serial0.println();
    
    if (!rfidParseFrame(_readBuffer, responseLen, &_parsedFrame)) {
        Serial0.println("[RFID] Failed to parse frame");
        return false;
    }
    
    rfidExtractResponse(&_parsedFrame, response);
    return true;
}

/**
 * @brief 读取标签数据
 * @param deviceAddr 设备地址（2字节）
 * @param bank 内存分区
 * @param startAddr 起始地址（Word）
 * @param wordCount 读取字数
 * @param response 读取响应数据
 * @param accessPassword 访问密码（4字节，NULL表示无密码）
 * @return 是否操作成功
 */
bool RfidReader::readTag(uint16_t deviceAddr, RfidMemoryBank bank, uint16_t startAddr, uint16_t wordCount, RfidResponse *response, const uint8_t *accessPassword)
{
    uint16_t frameLen = rfidBuildReadFrame(_sendFrame, deviceAddr, bank, startAddr, wordCount, accessPassword);
    
    Serial0.printf("[RFID] Sending read frame to device 0x%04X (bank=%d, addr=%d, count=%d): ", 
                  deviceAddr, bank, startAddr, wordCount);
    rfidPrintHex(_sendFrame, frameLen);
    
    if (!sendFrame(_sendFrame, frameLen)) {
        Serial0.println("[RFID] Send frame failed");
        return false;
    }
    
    uint16_t responseLen = receiveFrame(_readBuffer, RFID_READ_TIMEOUT_MS);
    if (responseLen == 0) {
        Serial0.println("[RFID] No response received");
        return false;
    }
    
    Serial0.printf("[RFID] Received response (%d bytes): ", responseLen);
    for (uint16_t i = 0; i < responseLen; i++) {
        Serial0.printf("%02X ", _readBuffer[i]);
    }
    Serial0.println();
    
    if (!rfidParseFrame(_readBuffer, responseLen, &_parsedFrame)) {
        Serial0.println("[RFID] Failed to parse frame");
        return false;
    }
    
    rfidExtractResponse(&_parsedFrame, response);
    return true;
}

/**
 * @brief 写入标签数据
 * @param deviceAddr 设备地址（2字节）
 * @param bank 内存分区
 * @param startAddr 起始地址（Word）
 * @param data 待写入数据
 * @param wordCount 写入字数
 * @param response 写入响应数据
 * @param accessPassword 访问密码（4字节，NULL表示无密码）
 * @return 是否操作成功
 */
bool RfidReader::writeTag(uint16_t deviceAddr, RfidMemoryBank bank, uint16_t startAddr, const uint8_t *data, uint16_t wordCount, RfidResponse *response, const uint8_t *accessPassword)
{
    uint16_t frameLen = rfidBuildWriteFrame(_sendFrame, deviceAddr, bank, startAddr, data, wordCount, accessPassword);
    
    Serial0.printf("[RFID] Sending write frame to device 0x%04X (bank=%d, addr=%d, count=%d): ", 
                  deviceAddr, bank, startAddr, wordCount);
    rfidPrintHex(_sendFrame, frameLen);
    
    if (!sendFrame(_sendFrame, frameLen)) {
        Serial0.println("[RFID] Send frame failed");
        return false;
    }
    
    uint16_t responseLen = receiveFrame(_readBuffer, RFID_READ_TIMEOUT_MS);
    if (responseLen == 0) {
        Serial0.println("[RFID] No response received");
        return false;
    }
    
    Serial0.printf("[RFID] Received response (%d bytes): ", responseLen);
    for (uint16_t i = 0; i < responseLen; i++) {
        Serial0.printf("%02X ", _readBuffer[i]);
    }
    Serial0.println();
    
    if (!rfidParseFrame(_readBuffer, responseLen, &_parsedFrame)) {
        Serial0.println("[RFID] Failed to parse frame");
        return false;
    }
    
    rfidExtractResponse(&_parsedFrame, response);
    return true;
}

/**
 * @brief 查询固件版本
 * @param deviceAddr 设备地址（2字节）
 * @param response 响应数据
 * @return 是否操作成功
 */
bool RfidReader::queryVersion(uint16_t deviceAddr, RfidResponse *response)
{
    uint16_t frameLen = rfidBuildQueryVersionFrame(_sendFrame, deviceAddr);
    
    Serial0.printf("[RFID] Sending query version to device 0x%04X: ", deviceAddr);
    rfidPrintHex(_sendFrame, frameLen);
    
    if (!sendFrame(_sendFrame, frameLen)) {
        Serial0.println("[RFID] Send frame failed");
        return false;
    }
    
    uint16_t responseLen = receiveFrame(_readBuffer, RFID_READ_TIMEOUT_MS);
    if (responseLen == 0) {
        Serial0.println("[RFID] No response received");
        return false;
    }
    
    Serial0.printf("[RFID] Received response (%d bytes): ", responseLen);
    for (uint16_t i = 0; i < responseLen; i++) {
        Serial0.printf("%02X ", _readBuffer[i]);
    }
    Serial0.println();
    
    if (!rfidParseFrame(_readBuffer, responseLen, &_parsedFrame)) {
        Serial0.println("[RFID] Failed to parse frame");
        return false;
    }
    
    rfidExtractResponse(&_parsedFrame, response);
    return true;
}

/**
 * @brief 设置单个参数
 * @param deviceAddr 设备地址（2字节）
 * @param paramType 参数类型
 * @param value 参数值
 * @param valueLen 值长度
 * @param response 响应数据
 * @return 是否操作成功
 */
bool RfidReader::setParam(uint16_t deviceAddr, RfidSingleParamType paramType, const uint8_t *value, uint8_t valueLen, RfidResponse *response)
{
    uint16_t frameLen = rfidBuildSetParamFrame(_sendFrame, deviceAddr, paramType, value, valueLen);
    
    Serial0.printf("[RFID] Sending set param to device 0x%04X (param=%d): ", deviceAddr, paramType);
    rfidPrintHex(_sendFrame, frameLen);
    
    if (!sendFrame(_sendFrame, frameLen)) {
        Serial0.println("[RFID] Send frame failed");
        return false;
    }
    
    uint16_t responseLen = receiveFrame(_readBuffer, RFID_READ_TIMEOUT_MS);
    if (responseLen == 0) {
        Serial0.println("[RFID] No response received");
        return false;
    }
    
    Serial0.printf("[RFID] Received response (%d bytes): ", responseLen);
    for (uint16_t i = 0; i < responseLen; i++) {
        Serial0.printf("%02X ", _readBuffer[i]);
    }
    Serial0.println();
    
    if (!rfidParseFrame(_readBuffer, responseLen, &_parsedFrame)) {
        Serial0.println("[RFID] Failed to parse frame");
        return false;
    }
    
    rfidExtractResponse(&_parsedFrame, response);
    return true;
}

/**
 * @brief 处理通知帧（持续盘点时的标签上报）
 * @param response 响应数据
 * @return 是否收到有效通知
 */
bool RfidReader::processNotification(RfidResponse *response)
{
    uint16_t responseLen = receiveFrame(_readBuffer, 100);
    if (responseLen == 0) {
        return false;
    }
    
    Serial0.printf("[RFID] Received notification (%d bytes): ", responseLen);
    for (uint16_t i = 0; i < responseLen; i++) {
        Serial0.printf("%02X ", _readBuffer[i]);
    }
    Serial0.println();
    
    if (!rfidParseFrame(_readBuffer, responseLen, &_parsedFrame)) {
        Serial0.println("[RFID] Failed to parse notification frame");
        return false;
    }
    
    if (_parsedFrame.frameType != RFID_FRAME_TYPE_NOTIFICATION) {
        return false;
    }
    
    rfidExtractNotification(&_parsedFrame, response);
    return true;
}

/**
 * @brief 发送数据帧
 * @param frame 帧数据
 * @param len 帧长度
 * @return 是否发送成功
 */
bool RfidReader::sendFrame(const uint8_t *frame, uint16_t len)
{
    return _rs485.send(frame, len);
}

/**
 * @brief 接收数据帧
 * @param buffer 接收缓冲区
 * @param timeoutMs 超时时间（毫秒）
 * @return 接收到的数据长度
 */
uint16_t RfidReader::receiveFrame(uint8_t *buffer, uint16_t timeoutMs)
{
    return _rs485.receive(buffer, timeoutMs);
}

/**
 * @brief 获取状态码对应的消息
 * @param status 状态码
 * @return 状态消息字符串
 */
const char* RfidReader::getStatusMessage(uint8_t status)
{
    switch (status) {
        case RFID_STATUS_SUCCESS: return "Success";
        case RFID_STATUS_UNSUPPORTED_PARAM: return "Unsupported parameter";
        case RFID_STATUS_PARAM_LEN_ERROR: return "Parameter length error";
        case RFID_STATUS_PARAM_CONTENT_ERROR: return "Parameter content error";
        case RFID_STATUS_UNSUPPORTED_CMD: return "Unsupported command";
        case RFID_STATUS_ADDR_MISMATCH: return "Address mismatch";
        case RFID_STATUS_CHECKSUM_ERROR: return "Checksum error";
        case RFID_STATUS_INVALID_TLV_TYPE: return "Invalid TLV type";
        case RFID_STATUS_FLASH_WRITE_FAILED: return "Flash write failed";
        case RFID_STATUS_INTERNAL_ERROR: return "Internal error";
        default: return "Unknown error";
    }
}

/**
 * @brief 打印标签信息
 * @param tag 标签信息
 */
void RfidReader::printTagInfo(const RfidTagInfo *tag)
{
    Serial0.print("[RFID]   EPC: ");
    for (uint8_t i = 0; i < tag->epcLen; i++) {
        Serial0.printf("%02X", tag->epc[i]);
    }
    Serial0.println();
    
    Serial0.printf("[RFID]   RSSI: %d dBm\n", tag->rssi);
    
    if (tag->timestamp[0] != 0) {
        Serial0.printf("[RFID]   Timestamp: %04d-%02d-%02d %02d:%02d:%02d\n",
                      (uint16_t)tag->timestamp[0] << 8 | tag->timestamp[1],
                      tag->timestamp[2], tag->timestamp[3],
                      tag->timestamp[4], tag->timestamp[5], tag->timestamp[6]);
    }
}

/**
 * @brief 打印响应数据
 * @param response 响应数据
 */
void RfidReader::printResponse(const RfidResponse *response)
{
    Serial0.printf("[RFID] Status: 0x%02X (%s)\n", response->status, getStatusMessage(response->status));
    Serial0.printf("[RFID] Tag Count: %d\n", response->tagCount);
    
    for (uint8_t i = 0; i < response->tagCount; i++) {
        Serial0.printf("[RFID] --- Tag %d ---\n", i + 1);
        printTagInfo(&response->tags[i]);
    }
}
