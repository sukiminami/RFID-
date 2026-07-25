#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

#define MAX_WHITELIST_SIZE 32
#define MAX_EPC_LENGTH 32

#define CONFIG_NAMESPACE "rfid_gateway"

typedef struct {
    char server[64];
    uint16_t port;
    char clientId[32];
    char username[32];
    char password[32];
    char topic[64];
    char subTopic[64];
    
    uint16_t alarmDuration;
    uint32_t heartbeatInterval;
    uint8_t bleEnabled;
} SystemConfig;

class ConfigManager {
public:
    ConfigManager();
    
    bool begin();
    
    bool loadConfig();
    
    bool saveConfig();
    
    void resetConfig();
    
    SystemConfig& getConfig();
    
    bool setConfig(const SystemConfig& config);
    
    bool addToWhitelist(const char* epc);
    
    bool removeFromWhitelist(const char* epc);
    
    bool isInWhitelist(const char* epc);
    
    int getWhitelistCount();
    
    bool getWhitelistItem(int index, char* epc, int maxLen);
    
    bool clearWhitelist();
    
    bool saveWhitelist();
    
    bool loadWhitelist();
    
private:
    SystemConfig _config;
    char _whitelist[MAX_WHITELIST_SIZE][MAX_EPC_LENGTH];
    int _whitelistCount;
    Preferences _prefs;
    
    void initDefaultConfig();
};

#endif