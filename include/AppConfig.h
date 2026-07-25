#ifndef APP_CONFIG_H
#define APP_CONFIG_H

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

#define RECEIVE_BUFFER_SIZE 2048
#define FRAME_HEADER_TIMEOUT 100

#endif