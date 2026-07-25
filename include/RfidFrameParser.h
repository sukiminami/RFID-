#ifndef RFID_FRAME_PARSER_H
#define RFID_FRAME_PARSER_H

#include <Arduino.h>

class ConfigManager;
class DeviceControl;
class NetworkManager;

class RfidFrameParser {
public:
    RfidFrameParser(ConfigManager* configManager, DeviceControl* deviceControl, NetworkManager* networkManager);
    
    void parseFrame(uint8_t* buffer, uint16_t len);
    
private:
    ConfigManager* _configManager;
    DeviceControl* _deviceControl;
    NetworkManager* _networkManager;
    
    void parseNotificationFrame(uint8_t* buffer, uint16_t len);
    void parseResponseFrame(uint8_t* buffer, uint16_t len, uint8_t frameCode, uint16_t paramLen);
    void parseCommandFrame(uint8_t frameCode);
    
    void publishResponse(const char* json);
};

#endif