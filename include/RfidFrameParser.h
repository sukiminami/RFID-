/**
 * @file RfidFrameParser.h
 * @brief RFID帧解析器头文件
 * @details 负责解析RFID读写器通过RS485发送的帧数据，
 *          支持通知帧、响应帧和命令帧三种帧类型的解析。
 */

#ifndef RFID_FRAME_PARSER_H
#define RFID_FRAME_PARSER_H

#include <Arduino.h>

// 前向声明依赖的模块类
class ConfigManager;
class DeviceControl;
class NetworkManager;

/**
 * @class RfidFrameParser
 * @brief RFID帧解析器类
 * @details 解析RFID读写器返回的二进制帧数据，提取标签信息(EPC、RSSI)，
 *          检查白名单状态，触发报警，并将结果上报到MQTT服务器。
 */
class RfidFrameParser {
public:
    /**
     * @brief 构造函数
     * @param configManager 配置管理器指针
     * @param deviceControl 设备控制模块指针
     * @param networkManager 网络管理器指针
     * @details 初始化各模块引用，用于白名单查询、报警控制和MQTT上报
     */
    RfidFrameParser(ConfigManager* configManager, DeviceControl* deviceControl, NetworkManager* networkManager);
    
    /**
     * @brief 解析RFID帧数据
     * @param buffer 帧数据指针
     * @param len 帧数据长度
     * @details 根据帧类型分发到对应的解析函数
     */
    void parseFrame(uint8_t* buffer, uint16_t len);
    
private:
    /**
     * @var _configManager
     * @brief 配置管理器指针
     * @details 用于白名单查询操作
     */
    ConfigManager* _configManager;
    
    /**
     * @var _deviceControl
     * @brief 设备控制模块指针
     * @details 用于触发和停止报警
     */
    DeviceControl* _deviceControl;
    
    /**
     * @var _networkManager
     * @brief 网络管理器指针
     * @details 用于发送MQTT上报消息
     */
    NetworkManager* _networkManager;
    
    /**
     * @brief 解析通知帧
     * @param buffer 帧数据指针
     * @param len 帧数据长度
     * @details 通知帧包含标签信息，提取EPC和RSSI，检查白名单，触发报警
     */
    void parseNotificationFrame(uint8_t* buffer, uint16_t len);
    
    /**
     * @brief 解析响应帧
     * @param buffer 帧数据指针
     * @param len 帧数据长度
     * @param frameCode 帧码
     * @param paramLen 参数长度
     * @details 响应帧包含命令执行结果，提取状态码并上报
     */
    void parseResponseFrame(uint8_t* buffer, uint16_t len, uint8_t frameCode, uint16_t paramLen);
    
    /**
     * @brief 解析命令帧
     * @param frameCode 帧码
     * @details 命令帧是读写器发送的请求，构建响应并上报
     */
    void parseCommandFrame(uint8_t frameCode);
    
    /**
     * @brief 发布响应消息到MQTT
     * @param json JSON格式的响应消息
     * @details 网络就绪时将解析结果发送到MQTT服务器
     */
    void publishResponse(const char* json);
};

#endif