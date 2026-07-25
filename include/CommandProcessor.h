#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include <Arduino.h>

class ConfigManager;
class DeviceControl;
class OtaManager;
class NetworkManager;

class CommandProcessor {
public:
    CommandProcessor(ConfigManager* configManager, DeviceControl* deviceControl, 
                     OtaManager* otaManager, NetworkManager* networkManager);
    
    bool processCommand(const char* jsonStr);
    
private:
    ConfigManager* _configManager;
    DeviceControl* _deviceControl;
    OtaManager* _otaManager;
    NetworkManager* _networkManager;
    
    bool processWhitelistCommand(const char* cmd, const char* jsonStr);
    bool processConfigCommand(const char* cmd);
    bool processAlarmCommand(const char* cmd);
    bool processOtaCommand(const char* cmd, const char* jsonStr);
    bool processTagCommand(const char* cmd, const char* jsonStr);
    
    bool isNetworkReady();
};

#endif