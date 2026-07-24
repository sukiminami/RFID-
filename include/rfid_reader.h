#ifndef RFID_READER_H
#define RFID_READER_H

#include <stdint.h>
#include "rfid_protocol.h"
#include "rs485.h"

#define RFID_READ_TIMEOUT_MS 2000
#define RFID_READ_BUFFER_SIZE 512

class RfidReader {
public:
    RfidReader(HardwareSerial *serial, int txPin, int rxPin, int dePin);
    bool begin(uint32_t baudRate = 115200);
    
    bool singleInventory(uint16_t deviceAddr, RfidResponse *response);
    bool startContinuousInventory(uint16_t deviceAddr, RfidResponse *response);
    bool stopInventory(uint16_t deviceAddr, RfidResponse *response);
    bool readTag(uint16_t deviceAddr, RfidMemoryBank bank, uint16_t startAddr, uint16_t wordCount, RfidResponse *response, const uint8_t *accessPassword = NULL);
    bool writeTag(uint16_t deviceAddr, RfidMemoryBank bank, uint16_t startAddr, const uint8_t *data, uint16_t wordCount, RfidResponse *response, const uint8_t *accessPassword = NULL);
    bool queryVersion(uint16_t deviceAddr, RfidResponse *response);
    bool setParam(uint16_t deviceAddr, RfidSingleParamType paramType, const uint8_t *value, uint8_t valueLen, RfidResponse *response);
    bool processNotification(RfidResponse *response);
    
    void printTagInfo(const RfidTagInfo *tag);
    void printResponse(const RfidResponse *response);
    
private:
    bool sendFrame(const uint8_t *frame, uint16_t len);
    uint16_t receiveFrame(uint8_t *buffer, uint16_t timeoutMs);
    const char* getStatusMessage(uint8_t status);
    
    Rs485Comm _rs485;
    uint8_t _sendFrame[RFID_MAX_FRAME_LEN];
    uint8_t _readBuffer[RFID_READ_BUFFER_SIZE];
    RfidFrame _parsedFrame;
};

#endif
