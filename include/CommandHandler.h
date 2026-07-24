#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>
#include "rs485.h"

#define MAX_FRAME_SIZE 256
#define DEVICE_ADDRESS 0x0000

class CommandHandler {
public:
    CommandHandler(Rs485Comm* rs485);
    
    bool buildFrame(uint8_t frameCode, const uint8_t* params, uint16_t paramLen, uint8_t* outFrame, uint16_t& outLen);
    bool sendFrame(uint8_t frameCode, const uint8_t* params = nullptr, uint16_t paramLen = 0);
    
    bool queryVersion();
    bool startInventory();
    bool stopInventory();
    bool singleInventory();
    
    bool setPower(uint8_t power);
    bool setBeep(bool enable);
    bool setFilterTime(uint8_t seconds);
    bool queryParam(uint8_t paramType);
    
    bool setWorkParams(uint8_t power, uint8_t interval, uint8_t mode, uint8_t membank, 
                       uint8_t startAddr, uint8_t length, uint8_t filterTime, 
                       uint16_t deviceAddr, bool beepEnable, uint16_t antennaFlag);
    
    bool reboot();
    
    bool readTag(uint32_t password, uint8_t membank, uint8_t address, uint8_t length);
    bool writeTag(uint32_t password, uint8_t membank, uint8_t address, uint8_t length, const uint8_t* data);
    bool lockTag(uint32_t password, uint8_t membank, uint8_t address, uint8_t length);
    bool destroyTag(uint32_t password);
    
    bool controlRelay(uint8_t relayNo, bool open, uint8_t duration);
    
    bool playAudio(const char* text);
    
    bool parseMqttCommand(const char* jsonStr);
    
    const char* getStatusMessage(uint8_t statusCode);
    
private:
    Rs485Comm* _rs485;
    
    uint8_t calculateChecksum(const uint8_t* buffer, uint16_t len);
    
    void buildStatusTLV(uint8_t statusCode, uint8_t* tlv, uint16_t& tlvLen);
    void buildSingleParamTLV(uint8_t paramType, const uint8_t* value, uint8_t valueLen, uint8_t* tlv, uint16_t& tlvLen);
    void buildOperationTLV(uint32_t password, uint8_t type, uint8_t membank, uint8_t address, 
                           uint8_t length, const uint8_t* data, uint8_t* tlv, uint16_t& tlvLen);
    void buildRelayTLV(uint8_t relayNo, bool open, uint8_t duration, uint8_t* tlv, uint16_t& tlvLen);
    void buildAudioTLV(uint8_t operation, const char* text, uint8_t* tlv, uint16_t& tlvLen);
};

#endif