#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>
#include "AppConfig.h"
#include "NetworkInterface.h"
#include "NetworkManager.h"

class Rs485Comm;
class CommandHandler;
class DeviceControl;
class ConfigManager;
class OtaManager;
class CommandProcessor;
class RfidFrameParser;

class SystemManager {
public:
    SystemManager();
    
    bool begin();
    
    void loop();
    
private:
    Rs485Comm* _rs485;
    CommandHandler* _cmdHandler;
    NetworkManager* _networkManager;
    NetworkConfig _networkConfig;
    DeviceControl* _deviceControl;
    ConfigManager* _configManager;
    OtaManager* _otaManager;
    CommandProcessor* _commandProcessor;
    RfidFrameParser* _rfidFrameParser;
    
    uint8_t _receiveBuffer[RECEIVE_BUFFER_SIZE];
    uint16_t _receiveLen;
    unsigned long _lastReceiveTime;
    unsigned long _lastHeartbeatTime;
    
    void initNetworkConfig();
    void onNetworkStatusChanged(NetworkStatus status, NetworkType type, const char* message);
    void onNetworkDataReceived(const uint8_t* data, uint16_t length);
    void sendHeartbeat();
    void processRfidData();
    
    static void staticNetworkStatusCallback(NetworkStatus status, NetworkType type, const char* message, void* userData);
    static void staticNetworkDataCallback(const uint8_t* data, uint16_t length, void* userData);
};

extern bool networkReady;

#endif