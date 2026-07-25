/**
 * @file CommandHandler.cpp
 * @brief RS485命令处理器实现文件
 * @details 实现CommandHandler类的所有成员函数，负责构建和发送RFID读写器命令帧。
 */

#include "CommandHandler.h"

/**
 * @def RS485_DEBUG
 * @brief 调试开关
 * @note 1-启用调试输出，0-禁用调试输出
 */
#define RS485_DEBUG 1

/**
 * @brief 构造函数
 * @param rs485 RS485通信模块指针
 * @details 使用初始化列表保存RS485通信模块指针
 */
CommandHandler::CommandHandler(Rs485Comm* rs485) : _rs485(rs485) {
}

/**
 * @brief 计算校验和
 * @param buffer 数据缓冲区
 * @param len 数据长度
 * @return 校验和值
 * @details 采用累加取反加1的算法：checksum = ~(sum) + 1
 *          即对所有字节进行累加，然后取反加1(补码)
 */
uint8_t CommandHandler::calculateChecksum(const uint8_t* buffer, uint16_t len) {
    uint8_t checksum = 0;
    // 累加所有字节
    for (uint16_t i = 0; i < len; i++) {
        checksum += buffer[i];
    }
    // 取反加1得到校验和
    return ~checksum + 1;
}

/**
 * @brief 构建命令帧
 * @param frameCode 帧码
 * @param params 参数数据指针(可为nullptr)
 * @param paramLen 参数长度
 * @param outFrame 输出帧缓冲区
 * @param outLen 输出帧长度
 * @return true-构建成功，false-参数过长
 * @details 帧格式：
 *          [0]    0x52 ('R') - 帧头1
 *          [1]    0x46 ('F') - 帧头2
 *          [2]    0x00       - 帧类型(命令帧)
 *          [3-4]  设备地址(大端序)
 *          [5]    帧码
 *          [6-7]  参数长度(大端序)
 *          [8...] 参数数据
 *          [最后] 校验和
 */
bool CommandHandler::buildFrame(uint8_t frameCode, const uint8_t* params, uint16_t paramLen, uint8_t* outFrame, uint16_t& outLen) {
    // 检查参数长度是否超过最大帧长度限制
    // 帧头2 + 类型1 + 地址2 + 帧码1 + 参数长度2 + 校验和1 = 9字节固定开销
    if (paramLen > MAX_FRAME_SIZE - 9) {
        #if RS485_DEBUG
        Serial0.println("[CMD] 错误: 帧长度过大");
        #endif
        return false;
    }
    
    uint16_t len = 0;
    
    // 帧头 "RF"
    outFrame[len++] = 0x52;  // 'R'
    outFrame[len++] = 0x46;  // 'F'
    
    // 帧类型：0x00表示命令帧
    outFrame[len++] = 0x00;
    
    // 设备地址(大端序)
    outFrame[len++] = (DEVICE_ADDRESS >> 8) & 0xFF;
    outFrame[len++] = DEVICE_ADDRESS & 0xFF;
    
    // 帧码
    outFrame[len++] = frameCode;
    
    // 参数长度(大端序)
    outFrame[len++] = (paramLen >> 8) & 0xFF;
    outFrame[len++] = paramLen & 0xFF;
    
    // 参数数据
    if (params && paramLen > 0) {
        for (uint16_t i = 0; i < paramLen; i++) {
            outFrame[len++] = params[i];
        }
    }
    
    // 校验和(对前面所有字节计算)
    outFrame[len++] = calculateChecksum(outFrame, len);
    
    // 输出帧长度
    outLen = len;
    
    return true;
}

/**
 * @brief 发送命令帧
 * @param frameCode 帧码
 * @param params 参数数据指针(默认nullptr)
 * @param paramLen 参数长度(默认0)
 * @return true-发送成功，false-构建或发送失败
 * @details 先调用buildFrame构建帧，然后通过RS485发送，调试模式下打印帧内容
 */
bool CommandHandler::sendFrame(uint8_t frameCode, const uint8_t* params, uint16_t paramLen) {
    uint8_t frame[MAX_FRAME_SIZE];
    uint16_t len = 0;
    
    // 构建帧
    if (!buildFrame(frameCode, params, paramLen, frame, len)) {
        return false;
    }
    
    // 调试输出：打印帧内容
    #if RS485_DEBUG
    Serial0.printf("[CMD] 发送帧 0x%02X, 长度=%d: ", frameCode, len);
    for (uint16_t i = 0; i < len; i++) {
        Serial0.printf("%02X ", frame[i]);
    }
    Serial0.println();
    #endif
    
    // 通过RS485发送帧
    return _rs485->send(frame, len);
}

/**
 * @brief 查询读写器版本
 * @return true-发送成功，false-发送失败
 * @details 帧码0x40，无参数，用于获取读写器固件版本信息
 */
bool CommandHandler::queryVersion() {
    return sendFrame(0x40);
}

/**
 * @brief 启动持续盘点
 * @return true-发送成功，false-发送失败
 * @details 帧码0x21，无参数，启动后读写器持续扫描标签并上报通知帧
 */
bool CommandHandler::startInventory() {
    return sendFrame(0x21);
}

/**
 * @brief 停止盘点
 * @return true-发送成功，false-发送失败
 * @details 帧码0x23，无参数，停止读写器的标签扫描
 */
bool CommandHandler::stopInventory() {
    return sendFrame(0x23);
}

/**
 * @brief 单次盘点
 * @return true-发送成功，false-发送失败
 * @details 帧码0x22，无参数，执行一次盘点后自动停止
 */
bool CommandHandler::singleInventory() {
    return sendFrame(0x22);
}

/**
 * @brief 设置发射功率
 * @param power 功率值(0-30dBm)
 * @return true-发送成功，false-发送失败
 * @details 帧码0x48，参数为TLV格式：
 *          类型0x26，长度0x03，子类型0x01(功率)，值为功率值
 */
bool CommandHandler::setPower(uint8_t power) {
    uint8_t tlv[6];
    tlv[0] = 0x26;  // TLV类型：参数设置
    tlv[1] = 0x03;  // TLV长度
    tlv[2] = 0x01;  // 子类型：发射功率
    tlv[3] = power; // 功率值
    return sendFrame(0x48, tlv, 4);
}

/**
 * @brief 设置蜂鸣器开关
 * @param enable true-开启，false-关闭
 * @return true-发送成功，false-发送失败
 * @details 帧码0x48，参数为TLV格式：
 *          类型0x26，长度0x03，子类型0x02(蜂鸣器)，值为开关状态
 */
bool CommandHandler::setBeep(bool enable) {
    uint8_t tlv[5];
    tlv[0] = 0x26;           // TLV类型：参数设置
    tlv[1] = 0x03;           // TLV长度
    tlv[2] = 0x02;           // 子类型：蜂鸣器
    tlv[3] = enable ? 0x01 : 0x00;  // 开关状态
    return sendFrame(0x48, tlv, 4);
}

/**
 * @brief 设置过滤时间
 * @param seconds 过滤时间(秒)
 * @return true-发送成功，false-发送失败
 * @details 帧码0x48，参数为TLV格式：
 *          类型0x26，长度0x03，子类型0x03(过滤时间)，值为秒数
 *          过滤时间用于同一标签重复上报的间隔
 */
bool CommandHandler::setFilterTime(uint8_t seconds) {
    uint8_t tlv[5];
    tlv[0] = 0x26;     // TLV类型：参数设置
    tlv[1] = 0x03;     // TLV长度
    tlv[2] = 0x03;     // 子类型：过滤时间
    tlv[3] = seconds;  // 过滤时间(秒)
    return sendFrame(0x48, tlv, 4);
}

/**
 * @brief 查询参数
 * @param paramType 参数类型
 * @return true-发送成功，false-发送失败
 * @details 帧码0x49，参数为TLV格式：
 *          类型0x26，长度0x02，子类型为参数类型
 */
bool CommandHandler::queryParam(uint8_t paramType) {
    uint8_t tlv[4];
    tlv[0] = 0x26;        // TLV类型：参数查询
    tlv[1] = 0x02;        // TLV长度
    tlv[2] = paramType;   // 参数类型
    return sendFrame(0x49, tlv, 3);
}

/**
 * @brief 设置工作参数(完整配置)
 * @param power 发射功率(0-30)
 * @param interval 盘点间隔(毫秒)
 * @param mode 工作模式(0-连续,1-单次,2-定时)
 * @param membank 存储区(0-保留,1-EPC,2-TID,3-USER)
 * @param startAddr 起始地址
 * @param length 数据长度
 * @param filterTime 过滤时间(秒)
 * @param deviceAddr 设备地址
 * @param beepEnable 蜂鸣器开关
 * @param antennaFlag 天线选择标志
 * @return true-发送成功，false-发送失败
 * @details 帧码0x41，参数为TLV格式：
 *          类型0x23，长度0x0F(15字节)，包含所有工作参数
 */
bool CommandHandler::setWorkParams(uint8_t power, uint8_t interval, uint8_t mode, uint8_t membank, 
                                   uint8_t startAddr, uint8_t length, uint8_t filterTime, 
                                   uint16_t deviceAddr, bool beepEnable, uint16_t antennaFlag) {
    uint8_t tlv[20];
    uint16_t len = 0;
    
    tlv[len++] = 0x23;     // TLV类型：工作参数
    tlv[len++] = 0x0F;     // TLV长度(15字节)
    tlv[len++] = 0x05;     // 子类型：完整配置
    
    // 参数值依次填入
    tlv[len++] = power;           // 发射功率
    tlv[len++] = interval;        // 盘点间隔
    tlv[len++] = mode;            // 工作模式
    tlv[len++] = membank;         // 存储区
    tlv[len++] = startAddr;       // 起始地址
    tlv[len++] = length;          // 数据长度
    tlv[len++] = filterTime;      // 过滤时间
    
    // 设备地址(大端序)
    tlv[len++] = (deviceAddr >> 8) & 0xFF;
    tlv[len++] = deviceAddr & 0xFF;
    
    tlv[len++] = beepEnable ? 0x01 : 0x00;  // 蜂鸣器开关
    tlv[len++] = 0x00;                       // 保留字节
    tlv[len++] = 0x00;                       // 保留字节
    
    // 天线选择标志(大端序)
    tlv[len++] = (antennaFlag >> 8) & 0xFF;
    tlv[len++] = antennaFlag & 0xFF;
    
    return sendFrame(0x41, tlv, len);
}

/**
 * @brief 重启读写器
 * @return true-发送成功，false-发送失败
 * @details 帧码0x10，无参数，触发读写器重启
 */
bool CommandHandler::reboot() {
    return sendFrame(0x10);
}

/**
 * @brief 读取标签数据
 * @param password 标签密码(4字节)
 * @param membank 存储区(0-保留,1-EPC,2-TID,3-USER)
 * @param address 起始地址
 * @param length 读取长度(字)
 * @return true-发送成功，false-发送失败
 * @details 帧码0x31，参数为TLV格式：
 *          类型0x08，操作类型0x00(读)，密码4字节，存储区，地址，长度
 */
bool CommandHandler::readTag(uint32_t password, uint8_t membank, uint8_t address, uint8_t length) {
    uint8_t tlv[12];
    uint16_t len = 0;
    
    tlv[len++] = 0x08;                 // TLV类型：标签操作
    tlv[len++] = 0x08;                 // TLV长度(8字节)
    
    // 密码(大端序)
    tlv[len++] = (password >> 24) & 0xFF;
    tlv[len++] = (password >> 16) & 0xFF;
    tlv[len++] = (password >> 8) & 0xFF;
    tlv[len++] = password & 0xFF;
    
    tlv[len++] = 0x00;                 // 操作类型：0x00=读
    tlv[len++] = membank;              // 存储区
    tlv[len++] = address;              // 起始地址
    tlv[len++] = length;               // 读取长度
    
    return sendFrame(0x31, tlv, len);
}

/**
 * @brief 写入标签数据
 * @param password 标签密码(4字节)
 * @param membank 存储区(0-保留,1-EPC,2-TID,3-USER)
 * @param address 起始地址
 * @param length 写入长度(字)
 * @param data 写入数据
 * @return true-发送成功，false-发送失败
 * @details 帧码0x30，参数为TLV格式：
 *          类型0x08，操作类型0x01(写)，密码4字节，存储区，地址，长度，数据
 */
bool CommandHandler::writeTag(uint32_t password, uint8_t membank, uint8_t address, uint8_t length, const uint8_t* data) {
    // 计算数据字节长度(每个字2字节)
    uint16_t dataLen = length * 2;
    
    // 检查数据长度是否超过帧大小限制
    if (dataLen > MAX_FRAME_SIZE - 20) {
        return false;
    }
    
    uint8_t tlv[MAX_FRAME_SIZE];
    uint16_t len = 0;
    
    tlv[len++] = 0x08;                 // TLV类型：标签操作
    tlv[len++] = 0x08 + dataLen;       // TLV长度(8字节固定 + 数据长度)
    
    // 密码(大端序)
    tlv[len++] = (password >> 24) & 0xFF;
    tlv[len++] = (password >> 16) & 0xFF;
    tlv[len++] = (password >> 8) & 0xFF;
    tlv[len++] = password & 0xFF;
    
    tlv[len++] = 0x01;                 // 操作类型：0x01=写
    tlv[len++] = membank;              // 存储区
    tlv[len++] = address;              // 起始地址
    tlv[len++] = length;               // 写入长度
    
    // 写入数据
    for (uint16_t i = 0; i < dataLen; i++) {
        tlv[len++] = data[i];
    }
    
    return sendFrame(0x30, tlv, len);
}

/**
 * @brief 锁定标签数据
 * @param password 标签密码(4字节)
 * @param membank 存储区(0-保留,1-EPC,2-TID,3-USER)
 * @param address 起始地址
 * @param length 锁定长度(字)
 * @return true-发送成功，false-发送失败
 * @details 帧码0x30，参数为TLV格式：
 *          类型0x08，操作类型0x02(锁定)，密码4字节，存储区，地址，长度
 */
bool CommandHandler::lockTag(uint32_t password, uint8_t membank, uint8_t address, uint8_t length) {
    uint8_t tlv[14];
    uint16_t len = 0;
    
    tlv[len++] = 0x08;                 // TLV类型：标签操作
    tlv[len++] = 0x08;                 // TLV长度(8字节)
    
    // 密码(大端序)
    tlv[len++] = (password >> 24) & 0xFF;
    tlv[len++] = (password >> 16) & 0xFF;
    tlv[len++] = (password >> 8) & 0xFF;
    tlv[len++] = password & 0xFF;
    
    tlv[len++] = 0x02;                 // 操作类型：0x02=锁定
    tlv[len++] = membank;              // 存储区
    tlv[len++] = address;              // 起始地址
    tlv[len++] = length;               // 锁定长度
    
    return sendFrame(0x30, tlv, len);
}

/**
 * @brief 销毁标签
 * @param password 标签密码(4字节)
 * @return true-发送成功，false-发送失败
 * @details 帧码0x30，参数为TLV格式：
 *          类型0x08，操作类型0x03(销毁)，密码4字节，其他参数填0
 */
bool CommandHandler::destroyTag(uint32_t password) {
    uint8_t tlv[14];
    uint16_t len = 0;
    
    tlv[len++] = 0x08;                 // TLV类型：标签操作
    tlv[len++] = 0x08;                 // TLV长度(8字节)
    
    // 密码(大端序)
    tlv[len++] = (password >> 24) & 0xFF;
    tlv[len++] = (password >> 16) & 0xFF;
    tlv[len++] = (password >> 8) & 0xFF;
    tlv[len++] = password & 0xFF;
    
    tlv[len++] = 0x03;                 // 操作类型：0x03=销毁
    tlv[len++] = 0x00;                 // 存储区(无效)
    tlv[len++] = 0x00;                 // 地址(无效)
    tlv[len++] = 0x00;                 // 长度(无效)
    
    return sendFrame(0x30, tlv, len);
}

/**
 * @brief 控制继电器
 * @param relayNo 继电器编号(1-N)
 * @param open true-吸合，false-释放
 * @param duration 持续时间(秒，0为永久)
 * @return true-发送成功，false-发送失败
 * @details 帧码0x4C，参数为TLV格式：
 *          类型0x27，长度0x03，继电器编号，开关状态，持续时间
 */
bool CommandHandler::controlRelay(uint8_t relayNo, bool open, uint8_t duration) {
    uint8_t tlv[5];
    tlv[0] = 0x27;                      // TLV类型：继电器控制
    tlv[1] = 0x03;                      // TLV长度
    tlv[2] = relayNo;                   // 继电器编号
    tlv[3] = open ? 0x01 : 0x00;        // 开关状态
    tlv[4] = duration;                  // 持续时间(秒)
    return sendFrame(0x4C, tlv, 5);
}

/**
 * @brief 播放语音
 * @param text 语音文本
 * @return true-发送成功，false-文本过长
 * @details 帧码0x4D，参数为TLV格式：
 *          类型0x28，长度=1+文本长度，操作类型0x01，跟随文本内容
 */
bool CommandHandler::playAudio(const char* text) {
    uint16_t textLen = strlen(text);
    
    // 检查文本长度是否超过帧大小限制
    if (textLen > MAX_FRAME_SIZE - 8) {
        return false;
    }
    
    uint8_t tlv[MAX_FRAME_SIZE];
    uint16_t len = 0;
    
    tlv[len++] = 0x28;                 // TLV类型：语音播放
    tlv[len++] = 0x01 + textLen;       // TLV长度(1字节操作类型 + 文本长度)
    tlv[len++] = 0x01;                 // 操作类型：0x01=播放文本
    
    // 语音文本
    for (uint16_t i = 0; i < textLen; i++) {
        tlv[len++] = text[i];
    }
    
    return sendFrame(0x4D, tlv, len);
}

/**
 * @brief 解析MQTT命令
 * @param jsonStr JSON格式的命令字符串
 * @return true-命令解析并执行成功，false-解析失败或未知命令
 * @details 从JSON中提取命令类型(cmd字段)，然后根据命令类型提取对应参数，
 *          调用相应的命令方法执行操作
 */
bool CommandHandler::parseMqttCommand(const char* jsonStr) {
    // 查找命令字段
    const char* cmdStart = strstr(jsonStr, "\"cmd\":\"");
    if (!cmdStart) {
        #if RS485_DEBUG
        Serial0.println("[CMD] 错误: 未找到命令");
        #endif
        return false;
    }
    
    // 定位命令值起始位置
    cmdStart += 7;  // 跳过 "\"cmd\":\""
    
    // 查找命令值结束位置
    const char* cmdEnd = strstr(cmdStart, "\"");
    if (!cmdEnd) {
        return false;
    }
    
    // 提取命令字符串
    char cmd[32];
    strncpy(cmd, cmdStart, cmdEnd - cmdStart);
    cmd[cmdEnd - cmdStart] = '\0';
    
    // 调试输出
    #if RS485_DEBUG
    Serial0.printf("[CMD] 收到MQTT命令: %s\n", cmd);
    #endif
    
    // 根据命令类型分发处理
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
        // 提取功率参数
        const char* valStart = strstr(jsonStr, "\"power\":");
        if (valStart) {
            uint8_t power = atoi(valStart + 8);
            return setPower(power);
        }
    }
    else if (strcmp(cmd, "set_beep") == 0) {
        // 提取蜂鸣器开关参数
        const char* valStart = strstr(jsonStr, "\"enable\":");
        if (valStart) {
            bool enable = atoi(valStart + 9) != 0;
            return setBeep(enable);
        }
    }
    else if (strcmp(cmd, "set_filter_time") == 0) {
        // 提取过滤时间参数
        const char* valStart = strstr(jsonStr, "\"seconds\":");
        if (valStart) {
            uint8_t seconds = atoi(valStart + 10);
            return setFilterTime(seconds);
        }
    }
    else if (strcmp(cmd, "query_param") == 0) {
        // 提取参数类型
        const char* valStart = strstr(jsonStr, "\"param_type\":");
        if (valStart) {
            uint8_t paramType = atoi(valStart + 12);
            return queryParam(paramType);
        }
    }
    else if (strcmp(cmd, "set_work_params") == 0) {
        // 设置默认参数值
        uint8_t power = 30, interval = 10, mode = 0, membank = 1;
        uint8_t startAddr = 0, length = 0, filterTime = 0;
        uint16_t deviceAddr = DEVICE_ADDRESS;
        bool beepEnable = false;
        uint16_t antennaFlag = 1;
        
        // 提取各参数(使用strstr查找)
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
        // 设置默认参数值
        uint32_t password = 0;
        uint8_t membank = 1, address = 0, length = 4;
        
        // 提取各参数
        const char* p;
        if ((p = strstr(jsonStr, "\"password\":"))) password = strtoul(p + 11, NULL, 16);
        if ((p = strstr(jsonStr, "\"membank\":"))) membank = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"address\":"))) address = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"length\":"))) length = atoi(p + 10);
        
        return readTag(password, membank, address, length);
    }
    else if (strcmp(cmd, "write_tag") == 0) {
        // 设置默认参数值
        uint32_t password = 0;
        uint8_t membank = 1, address = 0, length = 4;
        uint8_t writeData[64] = {0};
        int dataLen = 0;
        
        // 提取各参数
        const char* p;
        if ((p = strstr(jsonStr, "\"password\":"))) password = strtoul(p + 11, NULL, 16);
        if ((p = strstr(jsonStr, "\"membank\":"))) membank = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"address\":"))) address = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"length\":"))) length = atoi(p + 10);
        
        // 提取十六进制数据
        const char* dataStart = strstr(jsonStr, "\"data\":\"");
        if (dataStart) {
            dataStart += 8;
            const char* dataEnd = strstr(dataStart, "\"");
            if (dataEnd) {
                const char* hexPtr = dataStart;
                // 逐字节解析十六进制字符串
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
        // 设置默认参数值
        uint32_t password = 0;
        uint8_t membank = 1, address = 0, length = 4;
        
        // 提取各参数
        const char* p;
        if ((p = strstr(jsonStr, "\"password\":"))) password = strtoul(p + 11, NULL, 16);
        if ((p = strstr(jsonStr, "\"membank\":"))) membank = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"address\":"))) address = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"length\":"))) length = atoi(p + 10);
        
        return lockTag(password, membank, address, length);
    }
    else if (strcmp(cmd, "destroy_tag") == 0) {
        uint32_t password = 0;
        
        // 提取密码参数
        const char* p;
        if ((p = strstr(jsonStr, "\"password\":"))) password = strtoul(p + 11, NULL, 16);
        
        return destroyTag(password);
    }
    else if (strcmp(cmd, "control_relay") == 0) {
        // 设置默认参数值
        uint8_t relayNo = 1;
        bool open = true;
        uint8_t duration = 0;
        
        // 提取各参数
        const char* p;
        if ((p = strstr(jsonStr, "\"relay_no\":"))) relayNo = atoi(p + 11);
        if ((p = strstr(jsonStr, "\"open\":"))) open = atoi(p + 7) != 0;
        if ((p = strstr(jsonStr, "\"duration\":"))) duration = atoi(p + 11);
        
        return controlRelay(relayNo, open, duration);
    }
    else if (strcmp(cmd, "play_audio") == 0) {
        // 提取语音文本
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
    
    // 未知命令
    #if RS485_DEBUG
    Serial0.printf("[CMD] 未知命令: %s\n", cmd);
    #endif
    return false;
}

/**
 * @brief 获取状态码对应的消息
 * @param statusCode 状态码
 * @return 状态消息字符串
 * @details 将RFID读写器返回的状态码转换为可读的中文消息，便于调试和日志输出
 */
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