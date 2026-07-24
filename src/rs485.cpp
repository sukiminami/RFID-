#include "rs485.h"

Rs485Comm::Rs485Comm(HardwareSerial *serial, int txPin, int rxPin, int dePin)
    : _serial(serial), _txPin(txPin), _rxPin(rxPin), _dePin(dePin)
{
}

bool Rs485Comm::begin(uint32_t baudRate)
{
    if (_serial == nullptr) {
        #if RS485_DEBUG
        Serial0.println("[RS485] 错误: 串口指针为空");
        #endif
        return false;
    }
    
    _baudRate = baudRate;
    #if RS485_DEBUG
    Serial0.printf("[RS485] 初始化: TX=%d, RX=%d, DE=%d, 波特率=%lu\n", 
                  _txPin, _rxPin, _dePin, baudRate);
    #endif
    
    if (_dePin >= 0) {
        pinMode(_dePin, OUTPUT);
        digitalWrite(_dePin, LOW);
        #if RS485_DEBUG
        Serial0.println("[RS485] DE引脚设置为LOW (接收模式)");
        #endif
    }
    
    _serial->begin(baudRate, SERIAL_8N1, _rxPin, _txPin);
    delay(100);
    flushInput();
    
    #if RS485_DEBUG
    Serial0.println("[RS485] 初始化成功");
    #endif
    return true;
}

bool Rs485Comm::send(const uint8_t *data, uint16_t len)
{
    if (_serial == nullptr) {
        #if RS485_DEBUG
        Serial0.println("[RS485] 错误: 串口指针为空");
        #endif
        return false;
    }
    
    if (data == nullptr || len == 0) {
        #if RS485_DEBUG
        Serial0.println("[RS485] 错误: 无效数据或长度");
        #endif
        return false;
    }
    
    #if RS485_DEBUG
    Serial0.println("[RS485] DE=HIGH (发送模式)");
    #endif
    enableTransmit(true);
    delayMicroseconds(10);
    
    #if RS485_DEBUG
    Serial0.printf("[RS485] 写入 %d 字节...\n", len);
    #endif
    size_t sent = _serial->write(data, len);
    _serial->flush();
    
    enableTransmit(false);
    delayMicroseconds(1);
    
    #if RS485_DEBUG
    Serial0.println("[RS485] DE=LOW (接收模式)");
    Serial0.printf("[RS485] 已发送 %d 字节: ", sent);
    for (int i = 0; i < len && i < 32; i++) {
        Serial0.printf("%02X ", data[i]);
    }
    if (len > 32) {
        Serial0.print("...");
    }
    Serial0.println();
    #endif
    
    return sent == len;
}

uint16_t Rs485Comm::receive(uint8_t *buffer, uint16_t timeoutMs, uint16_t idleTimeoutMs)
{
    if (_serial == nullptr || buffer == nullptr) {
        #if RS485_DEBUG
        Serial0.println("[RS485] 错误: 无效指针");
        #endif
        return 0;
    }
    
    uint16_t len = 0;
    unsigned long startTime = millis();
    unsigned long lastByteTime = millis();
    
    #if RS485_DEBUG
    Serial0.printf("[RS485] 开始接收, 超时=%dms, 空闲=%dms\n", 
                  timeoutMs, idleTimeoutMs);
    #endif
    
    while (millis() - startTime < timeoutMs) {
        while (_serial->available() > 0 && len < RS485_BUFFER_SIZE) {
            buffer[len++] = _serial->read();
            lastByteTime = millis();
            #if RS485_DEBUG
            if (len <= 16 || len % 32 == 0) {
                Serial0.printf("[RS485] RX[%d]: 0x%02X\n", len - 1, buffer[len - 1]);
            }
            #endif
        }
        
        if (len > 0 && millis() - lastByteTime > idleTimeoutMs) {
            break;
        }
        
        if (len >= RS485_BUFFER_SIZE) {
            #if RS485_DEBUG
            Serial0.println("[RS485] 缓冲区已满, 提前返回");
            #endif
            break;
        }
        
        delay(1);
    }
    
    #if RS485_DEBUG
    Serial0.printf("[RS485] 接收完成, %d 字节\n", len);
    #endif
    return len;
}

void Rs485Comm::enableTransmit(bool enable)
{
    if (_dePin >= 0) {
        digitalWrite(_dePin, enable ? HIGH : LOW);
    }
}

void Rs485Comm::flushInput()
{
    if (_serial != nullptr) {
        while (_serial->available() > 0) {
            _serial->read();
        }
    }
}

int Rs485Comm::available()
{
    if (_serial == nullptr) {
        return 0;
    }
    return _serial->available();
}