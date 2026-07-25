/**
 * @file CommandHandler.h
 * @brief RS485命令处理器头文件
 * @details 负责构建和发送RFID读写器的命令帧，支持查询版本、盘点控制、参数设置、标签操作等功能。
 */

#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>
#include "rs485.h"

// ==================== 常量定义 ====================

/**
 * @def MAX_FRAME_SIZE
 * @brief 最大帧长度(字节)
 * @note 帧格式：帧头2 + 类型1 + 地址2 + 帧码1 + 参数长度2 + 参数N + 校验和1 = 9 + N
 */
#define MAX_FRAME_SIZE 256

/**
 * @def DEVICE_ADDRESS
 * @brief 设备地址
 * @note 默认0x0000，表示广播地址或单播地址
 */
#define DEVICE_ADDRESS 0x0000

/**
 * @class CommandHandler
 * @brief RS485命令处理器类
 * @details 封装RFID读写器的所有命令操作，负责帧构建、发送和响应处理。
 */
class CommandHandler {
public:
    /**
     * @brief 构造函数
     * @param rs485 RS485通信模块指针
     * @details 保存RS485通信模块指针，用于发送帧数据
     */
    CommandHandler(Rs485Comm* rs485);
    
    /**
     * @brief 构建命令帧
     * @param frameCode 帧码
     * @param params 参数数据指针(可为nullptr)
     * @param paramLen 参数长度
     * @param outFrame 输出帧缓冲区
     * @param outLen 输出帧长度
     * @return true-构建成功，false-参数过长
     * @details 按照RFID协议帧格式构建完整帧，包括帧头、类型、地址、帧码、参数长度、参数和校验和
     */
    bool buildFrame(uint8_t frameCode, const uint8_t* params, uint16_t paramLen, uint8_t* outFrame, uint16_t& outLen);
    
    /**
     * @brief 发送命令帧
     * @param frameCode 帧码
     * @param params 参数数据指针(默认nullptr)
     * @param paramLen 参数长度(默认0)
     * @return true-发送成功，false-构建或发送失败
     * @details 先构建帧再通过RS485发送
     */
    bool sendFrame(uint8_t frameCode, const uint8_t* params = nullptr, uint16_t paramLen = 0);
    
    /**
     * @brief 查询读写器版本
     * @return true-发送成功，false-发送失败
     * @details 帧码0x40，无参数
     */
    bool queryVersion();
    
    /**
     * @brief 启动持续盘点
     * @return true-发送成功，false-发送失败
     * @details 帧码0x21，无参数，启动后持续上报标签
     */
    bool startInventory();
    
    /**
     * @brief 停止盘点
     * @return true-发送成功，false-发送失败
     * @details 帧码0x23，无参数
     */
    bool stopInventory();
    
    /**
     * @brief 单次盘点
     * @return true-发送成功，false-发送失败
     * @details 帧码0x22，无参数，执行一次盘点后停止
     */
    bool singleInventory();
    
    /**
     * @brief 设置发射功率
     * @param power 功率值(0-30dBm)
     * @return true-发送成功，false-发送失败
     * @details 帧码0x48，参数为TLV格式：类型0x26，长度0x03，子类型0x01，值为功率
     */
    bool setPower(uint8_t power);
    
    /**
     * @brief 设置蜂鸣器开关
     * @param enable true-开启，false-关闭
     * @return true-发送成功，false-发送失败
     * @details 帧码0x48，参数为TLV格式：类型0x26，长度0x03，子类型0x02，值为开关状态
     */
    bool setBeep(bool enable);
    
    /**
     * @brief 设置过滤时间
     * @param seconds 过滤时间(秒)
     * @return true-发送成功，false-发送失败
     * @details 帧码0x48，参数为TLV格式：类型0x26，长度0x03，子类型0x03，值为秒数
     */
    bool setFilterTime(uint8_t seconds);
    
    /**
     * @brief 查询参数
     * @param paramType 参数类型
     * @return true-发送成功，false-发送失败
     * @details 帧码0x49，参数为TLV格式：类型0x26，长度0x02，子类型为参数类型
     */
    bool queryParam(uint8_t paramType);
    
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
     * @details 帧码0x41，参数为TLV格式：类型0x23，长度0x0F，包含所有工作参数
     */
    bool setWorkParams(uint8_t power, uint8_t interval, uint8_t mode, uint8_t membank, 
                       uint8_t startAddr, uint8_t length, uint8_t filterTime, 
                       uint16_t deviceAddr, bool beepEnable, uint16_t antennaFlag);
    
    /**
     * @brief 重启读写器
     * @return true-发送成功，false-发送失败
     * @details 帧码0x10，无参数
     */
    bool reboot();
    
    /**
     * @brief 读取标签数据
     * @param password 标签密码(4字节)
     * @param membank 存储区(0-保留,1-EPC,2-TID,3-USER)
     * @param address 起始地址
     * @param length 读取长度(字)
     * @return true-发送成功，false-发送失败
     * @details 帧码0x31，参数为TLV格式：类型0x08，操作类型0x00
     */
    bool readTag(uint32_t password, uint8_t membank, uint8_t address, uint8_t length);
    
    /**
     * @brief 写入标签数据
     * @param password 标签密码(4字节)
     * @param membank 存储区(0-保留,1-EPC,2-TID,3-USER)
     * @param address 起始地址
     * @param length 写入长度(字)
     * @param data 写入数据
     * @return true-发送成功，false-发送失败
     * @details 帧码0x30，参数为TLV格式：类型0x08，操作类型0x01
     */
    bool writeTag(uint32_t password, uint8_t membank, uint8_t address, uint8_t length, const uint8_t* data);
    
    /**
     * @brief 锁定标签数据
     * @param password 标签密码(4字节)
     * @param membank 存储区(0-保留,1-EPC,2-TID,3-USER)
     * @param address 起始地址
     * @param length 锁定长度(字)
     * @return true-发送成功，false-发送失败
     * @details 帧码0x30，参数为TLV格式：类型0x08，操作类型0x02
     */
    bool lockTag(uint32_t password, uint8_t membank, uint8_t address, uint8_t length);
    
    /**
     * @brief 销毁标签
     * @param password 标签密码(4字节)
     * @return true-发送成功，false-发送失败
     * @details 帧码0x30，参数为TLV格式：类型0x08，操作类型0x03
     */
    bool destroyTag(uint32_t password);
    
    /**
     * @brief 控制继电器
     * @param relayNo 继电器编号(1-N)
     * @param open true-吸合，false-释放
     * @param duration 持续时间(秒，0为永久)
     * @return true-发送成功，false-发送失败
     * @details 帧码0x4C，参数为TLV格式：类型0x27，长度0x03
     */
    bool controlRelay(uint8_t relayNo, bool open, uint8_t duration);
    
    /**
     * @brief 播放语音
     * @param text 语音文本
     * @return true-发送成功，false-文本过长
     * @details 帧码0x4D，参数为TLV格式：类型0x28，操作类型0x01，跟随文本内容
     */
    bool playAudio(const char* text);
    
    /**
     * @brief 解析MQTT命令
     * @param jsonStr JSON格式的命令字符串
     * @return true-命令解析并执行成功，false-解析失败或未知命令
     * @details 从JSON中提取命令类型和参数，调用对应的命令方法
     */
    bool parseMqttCommand(const char* jsonStr);
    
    /**
     * @brief 获取状态码对应的消息
     * @param statusCode 状态码
     * @return 状态消息字符串
     * @details 将RFID读写器返回的状态码转换为可读的中文消息
     */
    const char* getStatusMessage(uint8_t statusCode);
    
private:
    /**
     * @var _rs485
     * @brief RS485通信模块指针
     */
    Rs485Comm* _rs485;
    
    /**
     * @brief 计算校验和
     * @param buffer 数据缓冲区
     * @param len 数据长度
     * @return 校验和值
     * @details 采用累加取反加1的算法：checksum = ~(sum) + 1
     */
    uint8_t calculateChecksum(const uint8_t* buffer, uint16_t len);
    
    /**
     * @brief 构建状态TLV
     * @param statusCode 状态码
     * @param tlv 输出TLV缓冲区
     * @param tlvLen 输出TLV长度
     * @details TLV格式：类型0x07，长度0x01，值为状态码
     */
    void buildStatusTLV(uint8_t statusCode, uint8_t* tlv, uint16_t& tlvLen);
    
    /**
     * @brief 构建单参数TLV
     * @param paramType 参数类型
     * @param value 参数值指针
     * @param valueLen 参数值长度
     * @param tlv 输出TLV缓冲区
     * @param tlvLen 输出TLV长度
     * @details TLV格式：类型0x26，长度=2+valueLen，子类型+值
     */
    void buildSingleParamTLV(uint8_t paramType, const uint8_t* value, uint8_t valueLen, uint8_t* tlv, uint16_t& tlvLen);
    
    /**
     * @brief 构建标签操作TLV
     * @param password 标签密码
     * @param type 操作类型(0-读,1-写,2-锁定,3-销毁)
     * @param membank 存储区
     * @param address 起始地址
     * @param length 数据长度
     * @param data 数据(写入时有效)
     * @param tlv 输出TLV缓冲区
     * @param tlvLen 输出TLV长度
     * @details TLV格式：类型0x08，长度=8+数据长度
     */
    void buildOperationTLV(uint32_t password, uint8_t type, uint8_t membank, uint8_t address, 
                           uint8_t length, const uint8_t* data, uint8_t* tlv, uint16_t& tlvLen);
    
    /**
     * @brief 构建继电器控制TLV
     * @param relayNo 继电器编号
     * @param open 开关状态
     * @param duration 持续时间
     * @param tlv 输出TLV缓冲区
     * @param tlvLen 输出TLV长度
     * @details TLV格式：类型0x27，长度0x03
     */
    void buildRelayTLV(uint8_t relayNo, bool open, uint8_t duration, uint8_t* tlv, uint16_t& tlvLen);
    
    /**
     * @brief 构建语音播放TLV
     * @param operation 操作类型
     * @param text 语音文本
     * @param tlv 输出TLV缓冲区
     * @param tlvLen 输出TLV长度
     * @details TLV格式：类型0x28，长度=1+文本长度
     */
    void buildAudioTLV(uint8_t operation, const char* text, uint8_t* tlv, uint16_t& tlvLen);
};

#endif