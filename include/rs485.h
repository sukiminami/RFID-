#ifndef RS485_H
#define RS485_H

#include <Arduino.h>

#define RS485_DEFAULT_BAUD 115200
#define RS485_DEFAULT_TIMEOUT 2000
#define RS485_DEFAULT_IDLE_TIMEOUT 100
#define RS485_BUFFER_SIZE 512

#ifndef RS485_DEBUG
#define RS485_DEBUG 0
#endif

class Rs485Comm {
public:
    Rs485Comm(HardwareSerial *serial, int txPin, int rxPin, int dePin);
    bool begin(uint32_t baudRate = RS485_DEFAULT_BAUD);
    bool send(const uint8_t *data, uint16_t len);
    uint16_t receive(uint8_t *buffer, uint16_t timeoutMs = RS485_DEFAULT_TIMEOUT, 
                     uint16_t idleTimeoutMs = RS485_DEFAULT_IDLE_TIMEOUT);
    void flushInput();
    int available();
    
private:
    HardwareSerial *_serial;
    int _txPin;
    int _rxPin;
    int _dePin;
    uint32_t _baudRate;
    void enableTransmit(bool enable);
};

#endif
