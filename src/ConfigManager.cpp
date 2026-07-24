#include "ConfigManager.h"

ConfigManager::ConfigManager() : _whitelistCount(0) {
    initDefaultConfig();
    memset(_whitelist, 0, sizeof(_whitelist));
}

void ConfigManager::initDefaultConfig() {
    memset(&_config, 0, sizeof(SystemConfig));
    
    strncpy(_config.server, "broker.emqx.io", sizeof(_config.server) - 1);
    _config.port = 1883;
    strncpy(_config.clientId, "rfid_gateway_0001", sizeof(_config.clientId) - 1);
    strncpy(_config.topic, "SMtest", sizeof(_config.topic) - 1);
    strncpy(_config.subTopic, "SMtest/cmd", sizeof(_config.subTopic) - 1);
    
    _config.alarmDuration = 10000;
    _config.heartbeatInterval = 300000;
    _config.bleEnabled = 1;
}

bool ConfigManager::begin() {
    loadConfig();
    loadWhitelist();
    return true;
}

bool ConfigManager::loadConfig() {
    if (!_prefs.begin(CONFIG_NAMESPACE, true)) {
        Serial0.println("[配置] 打开配置存储失败，使用默认配置");
        return false;
    }
    
    if (!_prefs.getBool("config_valid", false)) {
        Serial0.println("[配置] 配置无效，使用默认配置");
        _prefs.end();
        return false;
    }
    
    String server = _prefs.getString("server", "");
    String clientId = _prefs.getString("clientId", "");
    String username = _prefs.getString("username", "");
    String password = _prefs.getString("password", "");
    String topic = _prefs.getString("topic", "");
    String subTopic = _prefs.getString("subTopic", "");
    
    strncpy(_config.server, server.c_str(), sizeof(_config.server) - 1);
    _config.port = _prefs.getUInt("port", 1883);
    strncpy(_config.clientId, clientId.c_str(), sizeof(_config.clientId) - 1);
    strncpy(_config.username, username.c_str(), sizeof(_config.username) - 1);
    strncpy(_config.password, password.c_str(), sizeof(_config.password) - 1);
    strncpy(_config.topic, topic.c_str(), sizeof(_config.topic) - 1);
    strncpy(_config.subTopic, subTopic.c_str(), sizeof(_config.subTopic) - 1);
    
    _config.alarmDuration = _prefs.getUInt("alarmDuration", 10000);
    _config.heartbeatInterval = _prefs.getUInt("heartbeatInterval", 300000);
    _config.bleEnabled = _prefs.getUInt("bleEnabled", 1);
    
    _prefs.end();
    
    Serial0.println("[配置] 配置加载成功");
    return true;
}

bool ConfigManager::saveConfig() {
    if (!_prefs.begin(CONFIG_NAMESPACE, false)) {
        Serial0.println("[配置] 打开配置存储失败");
        return false;
    }
    
    _prefs.putString("server", _config.server);
    _prefs.putUInt("port", _config.port);
    _prefs.putString("clientId", _config.clientId);
    _prefs.putString("username", _config.username);
    _prefs.putString("password", _config.password);
    _prefs.putString("topic", _config.topic);
    _prefs.putString("subTopic", _config.subTopic);
    
    _prefs.putUInt("alarmDuration", _config.alarmDuration);
    _prefs.putUInt("heartbeatInterval", _config.heartbeatInterval);
    _prefs.putUInt("bleEnabled", _config.bleEnabled);
    
    _prefs.putBool("config_valid", true);
    
    _prefs.end();
    
    Serial0.println("[配置] 配置保存成功");
    return true;
}

void ConfigManager::resetConfig() {
    initDefaultConfig();
    clearWhitelist();
    saveConfig();
    saveWhitelist();
    Serial0.println("[配置] 配置已重置为默认值");
}

SystemConfig& ConfigManager::getConfig() {
    return _config;
}

bool ConfigManager::setConfig(const SystemConfig& config) {
    memcpy(&_config, &config, sizeof(SystemConfig));
    return saveConfig();
}

bool ConfigManager::addToWhitelist(const char* epc) {
    if (epc == nullptr || strlen(epc) == 0) {
        return false;
    }
    
    if (_whitelistCount >= MAX_WHITELIST_SIZE) {
        Serial0.println("[配置] 白名单已满");
        return false;
    }
    
    for (int i = 0; i < _whitelistCount; i++) {
        if (strcmp(_whitelist[i], epc) == 0) {
            Serial0.println("[配置] EPC已在白名单中");
            return true;
        }
    }
    
    strncpy(_whitelist[_whitelistCount], epc, MAX_EPC_LENGTH - 1);
    _whitelistCount++;
    
    Serial0.printf("[配置] 添加EPC到白名单: %s, 当前数量: %d\n", epc, _whitelistCount);
    
    return true;
}

bool ConfigManager::removeFromWhitelist(const char* epc) {
    if (epc == nullptr || strlen(epc) == 0) {
        return false;
    }
    
    for (int i = 0; i < _whitelistCount; i++) {
        if (strcmp(_whitelist[i], epc) == 0) {
            for (int j = i; j < _whitelistCount - 1; j++) {
                strncpy(_whitelist[j], _whitelist[j + 1], MAX_EPC_LENGTH);
            }
            _whitelistCount--;
            memset(_whitelist[_whitelistCount], 0, MAX_EPC_LENGTH);
            
            Serial0.printf("[配置] 从白名单移除EPC: %s, 当前数量: %d\n", epc, _whitelistCount);
            
            return true;
        }
    }
    
    Serial0.println("[配置] EPC不在白名单中");
    return false;
}

bool ConfigManager::isInWhitelist(const char* epc) {
    if (epc == nullptr || strlen(epc) == 0) {
        return false;
    }
    
    for (int i = 0; i < _whitelistCount; i++) {
        if (strcmp(_whitelist[i], epc) == 0) {
            return true;
        }
    }
    
    return false;
}

int ConfigManager::getWhitelistCount() {
    return _whitelistCount;
}

bool ConfigManager::getWhitelistItem(int index, char* epc, int maxLen) {
    if (index < 0 || index >= _whitelistCount || epc == nullptr) {
        return false;
    }
    
    strncpy(epc, _whitelist[index], maxLen - 1);
    return true;
}

bool ConfigManager::clearWhitelist() {
    _whitelistCount = 0;
    memset(_whitelist, 0, sizeof(_whitelist));
    Serial0.println("[配置] 白名单已清空");
    return true;
}

bool ConfigManager::saveWhitelist() {
    if (!_prefs.begin(CONFIG_NAMESPACE, false)) {
        Serial0.println("[配置] 打开配置存储失败");
        return false;
    }
    
    _prefs.putUInt("whitelist_count", _whitelistCount);
    
    for (int i = 0; i < _whitelistCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "wl_%d", i);
        _prefs.putString(key, _whitelist[i]);
    }
    
    _prefs.end();
    
    Serial0.printf("[配置] 白名单保存成功, 数量: %d\n", _whitelistCount);
    return true;
}

bool ConfigManager::loadWhitelist() {
    if (!_prefs.begin(CONFIG_NAMESPACE, true)) {
        Serial0.println("[配置] 打开配置存储失败");
        return false;
    }
    
    _whitelistCount = _prefs.getUInt("whitelist_count", 0);
    
    if (_whitelistCount > MAX_WHITELIST_SIZE) {
        _whitelistCount = MAX_WHITELIST_SIZE;
    }
    
    for (int i = 0; i < _whitelistCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "wl_%d", i);
        String epc = _prefs.getString(key, "");
        strncpy(_whitelist[i], epc.c_str(), MAX_EPC_LENGTH - 1);
    }
    
    _prefs.end();
    
    Serial0.printf("[配置] 白名单加载成功, 数量: %d\n", _whitelistCount);
    return true;
}