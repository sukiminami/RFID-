/**
 * @file AppConfig.h
 * @brief 应用程序全局配置头文件
 * @details 集中管理所有硬件引脚、网络参数、缓冲区大小等常量配置，
 *          便于统一修改和维护。
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// ==================== UART串口配置 ====================

/**
 * @def UART_TX_PIN
 * @brief RS485通信的串口发送引脚
 * @note ESP32-S3 GPIO17对应Serial2 TX
 */
#define UART_TX_PIN 17

/**
 * @def UART_RX_PIN
 * @brief RS485通信的串口接收引脚
 * @note ESP32-S3 GPIO16对应Serial2 RX
 */
#define UART_RX_PIN 16

/**
 * @def UART_DE_PIN
 * @brief RS485收发控制引脚(DE/RE)
 * @note GPIO21，高电平发送，低电平接收
 */
#define UART_DE_PIN 21

/**
 * @def BAUD_RATE
 * @brief RS485通信波特率
 * @note RFID读卡器默认波特率为115200
 */
#define BAUD_RATE 115200

// ==================== 网络服务器配置 ====================

/**
 * @def SERVER_IP
 * @brief MQTT服务器地址(默认EMQX公共服务器)
 * @note 可根据实际部署环境修改为私网IP或其他MQTT服务器
 */
#define SERVER_IP "broker.emqx.io"

/**
 * @def SERVER_PORT
 * @brief MQTT服务器端口
 * @note 1883为MQTT非加密端口，8883为MQTT SSL加密端口
 */
#define SERVER_PORT 1883

// ==================== MQTT客户端配置 ====================

/**
 * @def MQTT_CLIENT_ID
 * @brief MQTT客户端唯一标识符
 * @note 每个设备必须使用唯一的客户端ID，建议包含设备序列号
 */
#define MQTT_CLIENT_ID "rfid_gateway_0001"

/**
 * @def MQTT_USERNAME
 * @brief MQTT连接用户名(可选)
 * @note 公共服务器通常无需认证，私网服务器需配置
 */
#define MQTT_USERNAME ""

/**
 * @def MQTT_PASSWORD
 * @brief MQTT连接密码(可选)
 * @note 与用户名配套使用
 */
#define MQTT_PASSWORD ""

/**
 * @def MQTT_TOPIC
 * @brief MQTT发布主题(上行数据)
 * @note 设备上报数据使用此主题
 */
#define MQTT_TOPIC "SMtest"

/**
 * @def MQTT_SUB_TOPIC
 * @brief MQTT订阅主题(下行命令)
 * @note 接收服务器下发命令使用此主题
 */
#define MQTT_SUB_TOPIC "SMtest/cmd"

// ==================== 系统定时配置 ====================

/**
 * @def HEARTBEAT_INTERVAL
 * @brief 心跳包发送间隔(毫秒)
 * @note 默认300000ms = 5分钟，用于告知服务器设备在线状态
 */
#define HEARTBEAT_INTERVAL 300000

// ==================== 固件版本配置 ====================

/**
 * @def FIRMWARE_VERSION
 * @brief 当前固件版本号
 * @note 格式为"主版本.次版本.修订号"，用于OTA版本检查
 */
#define FIRMWARE_VERSION "1.0.0"

// ==================== 缓冲区配置 ====================

/**
 * @def RECEIVE_BUFFER_SIZE
 * @brief RFID数据接收缓冲区大小(字节)
 * @note 2048字节足够容纳多个RFID帧数据
 */
#define RECEIVE_BUFFER_SIZE 2048

/**
 * @def FRAME_HEADER_TIMEOUT
 * @brief 帧头检测超时时间(毫秒)
 * @note 超过此时间未收到新数据则认为帧接收完成
 */
#define FRAME_HEADER_TIMEOUT 100

#endif