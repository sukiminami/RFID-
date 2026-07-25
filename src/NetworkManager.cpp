/**
 * @file NetworkManager.cpp
 * @brief 网络连接管理器实现
 * @details 管理 Wi-Fi 和蓝牙网络：
 *          - Wi-Fi：连接后通过 MQTT 与服务器通信
 *          - 蓝牙：用于配置 Wi-Fi 名称和密码
 */

#include "NetworkManager.h"

/**
 * @brief 构造函数
 * @details 初始化所有成员变量为空/默认值
 */
NetworkManager::NetworkManager() {
    _bluetooth = nullptr;
    _wifi = nullptr;
    _activeNetwork = NETWORK_TYPE_NONE;
    _configPhase = CONFIG_PHASE_WAITING;
    _initialized = false;
    _bluetoothInitialized = false;
    _lastUpdateTime = 0;
    _lastSwitchTime = 0;
    _wifiFailTime = 0;
    _dataCallback = nullptr;
    _statusCallback = nullptr;
    _userData = nullptr;
    memset(&_config, 0, sizeof(_config));
}

/**
 * @brief 析构函数
 * @details 断开所有网络连接，释放所有网络模块内存
 */
NetworkManager::~NetworkManager() {
    disconnect();
    if (_bluetooth != nullptr) delete _bluetooth;
    if (_wifi != nullptr) delete _wifi;
}

/**
 * @brief 初始化网络管理器
 * @param config 网络配置指针（可为 nullptr，使用默认配置）
 * @return true-初始化成功，false-失败
 * @details 创建蓝牙和 Wi-Fi 模块实例，并注册回调
 */
bool NetworkManager::begin(NetworkConfig* config) {
    if (config != nullptr) {
        setConfig(config);
    }
    
    if (_bluetooth == nullptr) {
        _bluetooth = new BluetoothNetwork("RFID_Gateway_BT");
        if (_bluetooth != nullptr) {
            _bluetooth->setStatusCallback(staticStatusCallback, this);
            _bluetooth->setDataCallback(staticDataCallback, this);
        }
    }
    
    if (_wifi == nullptr) {
        _wifi = new WiFiNetwork();
        if (_wifi != nullptr) {
            _wifi->setStatusCallback(staticStatusCallback, this);
            _wifi->setDataCallback(staticDataCallback, this);
        }
    }
    
    bool hasFlashConfig = loadWifiConfig();
    
    if (hasFlashConfig) {
        Serial0.println("[SYS] Using WiFi config from Flash");
    } else {
        if (strlen(_config.ssid) > 0 && strcmp(_config.ssid, "YourWiFiSSID") != 0) {
            Serial0.println("[SYS] Using predefined WiFi config");
        } else {
            Serial0.println("[SYS] No valid WiFi config, waiting for BLE config");
            memset(_config.ssid, 0, sizeof(_config.ssid));
            memset(_config.password, 0, sizeof(_config.password));
        }
    }
    
    Serial0.println("[NET] Starting BLE (always on)...");
    if (initBluetooth()) {
        _bluetoothInitialized = true;
        Serial0.println("[NET] BLE initialized, always available for WiFi config");
    } else {
        Serial0.println("[NET] BLE initialization failed");
    }
    
    _initialized = true;
    return true;
}

/**
 * @brief 连接网络
 * @return true-连接成功，false-等待配置或连接失败
 * @details 若已收到 Wi-Fi 配置则连接 Wi-Fi，否则等待蓝牙配置
 */
bool NetworkManager::connect() {
    if (!_initialized) return false;
    
    if (_configPhase == CONFIG_PHASE_WAITING) {
        Serial0.println("[NET] No WiFi config, BLE ready for config");
        Serial0.println("[NET] Config format: SET_WIFI:SSID:PASSWORD");
        return false;
    }
    
    if (_configPhase == CONFIG_PHASE_RECEIVED) {
        Serial0.println("[NET] WiFi config received, connecting...");
        _configPhase = CONFIG_PHASE_CONNECTING;
        
        if (initWiFi()) {
            _activeNetwork = NETWORK_TYPE_WIFI;
            _lastSwitchTime = millis();
            Serial0.println("[NET] Wi-Fi connected");
            Serial0.println("[NET] BLE remains active, can reconfigure anytime");
            
            return true;
        }
        
        Serial0.println("[NET] Wi-Fi connection failed");
        _wifiFailTime = millis();
    }
    
    return false;
}

/**
 * @brief 处理蓝牙接收到的 Wi-Fi 配置数据
 * @param data 数据指针
 * @param length 数据长度
 * @details 解析格式：SET_WIFI:SSID:PASSWORD
 */
void NetworkManager::handleWifiConfig(const uint8_t* data, uint16_t length) {
    if (data == nullptr || length == 0) return;
    
    char buffer[256];
    if (length >= sizeof(buffer)) length = sizeof(buffer) - 1;
    memcpy(buffer, data, length);
    buffer[length] = '\0';
    
    Serial0.print("[NET] BLE data received: ");
    Serial0.println(buffer);
    
    if (strncmp(buffer, "SET_WIFI:", 9) == 0) {
        char* ssidStart = buffer + 9;
        char* passwordPos = strchr(ssidStart, ':');
        
        if (passwordPos != nullptr) {
            *passwordPos = '\0';
            char* password = passwordPos + 1;
            
            strncpy(_config.ssid, ssidStart, sizeof(_config.ssid) - 1);
            strncpy(_config.password, password, sizeof(_config.password) - 1);
            
            Serial0.print("[NET] WiFi config saved - SSID: ");
            Serial0.print(_config.ssid);
            Serial0.print(", Password length: ");
            Serial0.println(strlen(_config.password));
            
            _configPhase = CONFIG_PHASE_RECEIVED;
            
            if (_activeNetwork != NETWORK_TYPE_NONE) {
                Serial0.println("[NET] New WiFi config detected, switching...");
                
                if (_activeNetwork == NETWORK_TYPE_WIFI && _wifi != nullptr) {
                    _wifi->disconnect();
                    Serial0.println("[NET] Disconnected current WiFi");
                }
                
                _activeNetwork = NETWORK_TYPE_NONE;
                _lastSwitchTime = millis();
            }
            
            Serial0.println("[NET] New config ready, connecting in next cycle");
            saveWifiConfig();
        } else {
            Serial0.println("[NET] WiFi config format error, missing password");
        }
    }
}

/**
 * @brief 检查是否已收到 Wi-Fi 配置
 * @return true-已收到配置，false-未收到
 */
bool NetworkManager::hasWifiConfig() {
    return strlen(_config.ssid) > 0 && _configPhase >= CONFIG_PHASE_RECEIVED;
}

/**
 * @brief 周期性更新函数
 * @details 在主循环中调用，更新网络状态，处理自动重连
 */
void NetworkManager::update() {
    if (!_initialized) return;
    
    unsigned long now = millis();
    if (now - _lastUpdateTime < 100) return;
    _lastUpdateTime = now;
    
    if (_configPhase == CONFIG_PHASE_WAITING && _bluetooth != nullptr) {
        _bluetooth->update();
        return;
    }
    
    if (_configPhase == CONFIG_PHASE_RECEIVED) {
        connect();
        return;
    }
    
    if (_activeNetwork == NETWORK_TYPE_WIFI && _wifi != nullptr) {
        _wifi->update();
    }
    
    if (_bluetooth != nullptr) {
        _bluetooth->update();
    }
    
    checkAutoReconnect();
}

/**
 * @brief 断开所有网络连接
 * @details 依次断开 Wi-Fi、蓝牙连接
 */
void NetworkManager::disconnect() {
    if (_wifi != nullptr) {
        _wifi->disconnect();
    }
    if (_bluetooth != nullptr) {
        _bluetooth->disconnect();
    }
    _activeNetwork = NETWORK_TYPE_NONE;
    _lastSwitchTime = millis();
}

/**
 * @brief 发送数据到服务器
 * @param data 数据指针
 * @param length 数据长度
 * @return true-发送成功，false-失败
 * @details 使用 Wi-Fi 发送数据
 */
bool NetworkManager::send(const uint8_t* data, uint16_t length) {
    if (!_initialized) return false;
    
    if (_activeNetwork == NETWORK_TYPE_WIFI && _wifi != nullptr) {
        if (_wifi->getStatus() == NETWORK_CONNECTED) {
            return _wifi->send(data, length);
        }
    }
    
    return false;
}

/**
 * @brief 获取当前使用的网络类型
 * @return NetworkType 枚举值
 */
NetworkType NetworkManager::getCurrentNetworkType() {
    return _activeNetwork;
}

/**
 * @brief 获取当前网络状态
 * @return NetworkStatus 枚举值
 */
NetworkStatus NetworkManager::getNetworkStatus() {
    if (_activeNetwork == NETWORK_TYPE_WIFI && _wifi != nullptr) {
        return _wifi->getStatus();
    }
    return NETWORK_DISCONNECTED;
}

/**
 * @brief 检查是否有网络连接
 * @return true-网络已连接，false-未连接
 */
bool NetworkManager::isConnected() {
    return isWifiConnected();
}

/**
 * @brief 检查 Wi-Fi 是否已连接
 * @return true-已连接，false-未连接
 */
bool NetworkManager::isWifiConnected() {
    if (_wifi == nullptr) return false;
    return _wifi->getStatus() == NETWORK_CONNECTED;
}

/**
 * @brief 设置网络配置参数
 * @param config 配置结构体指针
 * @return true-设置成功，false-参数为空
 */
bool NetworkManager::setConfig(NetworkConfig* config) {
    if (config == nullptr) return false;
    memcpy(&_config, config, sizeof(NetworkConfig));
    return true;
}

/**
 * @brief 获取蓝牙模块指针
 * @return BluetoothNetwork 指针（可能为 nullptr）
 */
BluetoothNetwork* NetworkManager::getBluetoothModule() {
    return _bluetooth;
}

/**
 * @brief 获取 Wi-Fi 模块指针
 * @return WiFiNetwork 指针（可能为 nullptr）
 */
WiFiNetwork* NetworkManager::getWiFiModule() {
    return _wifi;
}

/**
 * @brief 设置数据接收回调函数
 * @param callback 回调函数指针
 * @param userData 用户数据指针
 */
void NetworkManager::setDataCallback(NetworkDataCallback callback, void* userData) {
    _dataCallback = callback;
    _userData = userData;
}

/**
 * @brief 设置网络状态变化回调函数
 * @param callback 回调函数指针
 * @param userData 用户数据指针
 */
void NetworkManager::setStatusCallback(NetworkStatusCallback callback, void* userData) {
    _statusCallback = callback;
    _userData = userData;
}

/**
 * @brief 初始化 Wi-Fi 模块并连接服务器
 * @return true-成功，false-失败
 */
bool NetworkManager::initWiFi() {
    if (_wifi == nullptr) return false;
    if (!_wifi->begin()) return false;
    
    if (strlen(_config.ssid) > 0) {
        if (!_wifi->connectToAP(_config.ssid, _config.password)) {
            return false;
        }
        
        if (strlen(_config.server) > 0 && _config.port > 0) {
            if (_config.useMqtt) {
                Serial0.println("[NET] Using MQTT protocol");
                return _wifi->connectMQTT(_config.server, _config.port, 
                                         _config.mqttClientId,
                                         _config.mqttUsername,
                                         _config.mqttPassword,
                                         _config.mqttTopic,
                                         _config.mqttSubTopic);
            } else {
                Serial0.println("[NET] Using TCP protocol");
                return _wifi->connectTCP(_config.server, _config.port);
            }
        }
    }
    return true;
}

/**
 * @brief 初始化蓝牙模块
 * @return true-成功，false-失败
 */
bool NetworkManager::initBluetooth() {
    if (_bluetooth == nullptr) return false;
    return _bluetooth->begin();
}

/**
 * @brief 检查并自动重连 Wi-Fi
 * @details 若 Wi-Fi 断开且超过冷却时间，尝试重新连接
 */
void NetworkManager::checkAutoReconnect() {
    unsigned long now = millis();
    
    if (_activeNetwork == NETWORK_TYPE_WIFI) {
        if (!isWifiConnected()) {
            if (_wifiFailTime == 0) {
                _wifiFailTime = now;
                Serial0.println("[NET] Wi-Fi disconnected");
            }
            
            unsigned long failDuration = now - _wifiFailTime;
            
            if (failDuration < 5000) {
                static unsigned long lastQuickReconnect = 0;
                if (now - lastQuickReconnect >= 2000) {
                    lastQuickReconnect = now;
                    Serial0.println("[NET] Trying quick WiFi reconnect...");
                    if (_wifi != nullptr) {
                        _wifi->disconnect();
                        initWiFi();
                    }
                }
            } else if (now - _lastSwitchTime > SWITCH_COOLDOWN) {
                Serial0.print("[NET] WiFi disconnected for ");
                Serial0.print(failDuration / 1000);
                Serial0.println("s, trying reconnect");
                if (_wifi != nullptr) {
                    _wifi->disconnect();
                    if (initWiFi()) {
                        _activeNetwork = NETWORK_TYPE_WIFI;
                        _lastSwitchTime = millis();
                        _wifiFailTime = 0;
                    }
                }
            }
        } else {
            _wifiFailTime = 0;
        }
    } else {
        if (_wifiFailTime == 0 || now - _wifiFailTime > WIFI_RETRY_INTERVAL) {
            Serial0.println("[NET] No network, trying Wi-Fi");
            if (initWiFi()) {
                _activeNetwork = NETWORK_TYPE_WIFI;
                _lastSwitchTime = millis();
            }
        }
    }
}

/**
 * @brief 内部状态变化处理函数
 * @param status 网络状态
 * @param type 网络类型
 * @param message 状态信息
 */
void NetworkManager::onNetworkStatusChanged(NetworkStatus status, NetworkType type, const char* message) {
    Serial0.print("[NET] ");
    Serial0.print(type == NETWORK_TYPE_WIFI ? "Wi-Fi" : "BLE");
    Serial0.print(": ");
    Serial0.println(message);
    
    if (_statusCallback != nullptr) {
        _statusCallback(status, type, message, _userData);
    }
}

/**
 * @brief 内部数据接收处理函数
 * @param data 数据指针
 * @param length 数据长度
 */
void NetworkManager::onNetworkDataReceived(const uint8_t* data, uint16_t length) {
    Serial0.print("[NET] Data received: ");
    Serial0.print(length);
    Serial0.println(" bytes");
    
    if (length > 9 && strncmp((const char*)data, "SET_WIFI:", 9) == 0) {
        Serial0.println("[NET] WiFi config command detected");
        handleWifiConfig(data, length);
        return;
    }
    
    if (_dataCallback != nullptr) {
        _dataCallback(data, length, _userData);
    }
}

/**
 * @brief 静态回调转发函数（状态变化）
 * @param status 网络状态
 * @param type 网络类型
 * @param message 状态信息
 * @param userData 用户数据指针（指向 NetworkManager 实例）
 */
void NetworkManager::staticStatusCallback(NetworkStatus status, NetworkType type, const char* message, void* userData) {
    if (userData != nullptr) {
        NetworkManager* manager = static_cast<NetworkManager*>(userData);
        manager->onNetworkStatusChanged(status, type, message);
    }
}

/**
 * @brief 静态回调转发函数（数据接收）
 * @param data 数据指针
 * @param length 数据长度
 * @param userData 用户数据指针（指向 NetworkManager 实例）
 */
void NetworkManager::staticDataCallback(const uint8_t* data, uint16_t length, void* userData) {
    if (userData != nullptr) {
        NetworkManager* manager = static_cast<NetworkManager*>(userData);
        manager->onNetworkDataReceived(data, length);
    }
}

// ==================== WiFi配置持久化存储实现 ====================

/**
 * @brief 保存Wi-Fi配置到Flash持久化存储
 * @return true-保存成功，false-失败
 */
bool NetworkManager::saveWifiConfig() {
    if (strlen(_config.ssid) == 0) {
        Serial0.println("[STORE] Error: SSID is empty");
        return false;
    }
    
    Preferences prefs;
    if (!prefs.begin("wifi_config", false)) {
        Serial0.println("[STORE] Failed to open Preferences");
        return false;
    }
    
    prefs.putString("ssid", _config.ssid);
    prefs.putString("pass", _config.password);
    prefs.putBool("valid", true);
    
    prefs.end();
    
    Serial0.println("[STORE] WiFi config saved to Flash");
    return true;
}

/**
 * @brief 从Flash加载已保存的Wi-Fi配置
 * @return true-加载成功且有有效配置，false-无保存的配置
 */
bool NetworkManager::loadWifiConfig() {
    Preferences prefs;
    if (!prefs.begin("wifi_config", true)) {
        Serial0.println("[STORE] No saved WiFi config");
        return false;
    }
    
    if (!prefs.getBool("valid", false)) {
        Serial0.println("[STORE] WiFi config invalid");
        prefs.end();
        return false;
    }
    
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    
    prefs.end();
    
    if (ssid.length() == 0) {
        Serial0.println("[STORE] Saved SSID is empty");
        return false;
    }
    
    strncpy(_config.ssid, ssid.c_str(), sizeof(_config.ssid) - 1);
    strncpy(_config.password, pass.c_str(), sizeof(_config.password) - 1);
    _configPhase = CONFIG_PHASE_RECEIVED;
    
    Serial0.println("[STORE] WiFi config loaded from Flash");
    return true;
}

/**
 * @brief 清除已保存的Wi-Fi配置
 * @return true-清除成功，false-失败
 */
bool NetworkManager::clearWifiConfig() {
    Preferences prefs;
    if (!prefs.begin("wifi_config", false)) {
        Serial0.println("[STORE] Failed to open Preferences");
        return false;
    }
    
    prefs.clear();
    prefs.end();
    
    memset(_config.ssid, 0, sizeof(_config.ssid));
    memset(_config.password, 0, sizeof(_config.password));
    _configPhase = CONFIG_PHASE_WAITING;
    
    Serial0.println("[STORE] WiFi config cleared");
    return true;
}