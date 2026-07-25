/**
 * @file ConfigManager.cpp
 * @brief 配置管理器实现文件
 * @details 实现ConfigManager类的所有成员函数，包括系统配置的加载/保存/重置，
 *          以及白名单的增删查改和Flash持久化存储。
 */

#include "ConfigManager.h"

/**
 * @brief 构造函数
 * @details 初始化白名单计数为0，调用initDefaultConfig()设置默认配置，
 *          清空白名单数组
 */
ConfigManager::ConfigManager() : _whitelistCount(0) {
    initDefaultConfig();
    memset(_whitelist, 0, sizeof(_whitelist));
}

/**
 * @brief 初始化默认配置
 * @details 设置所有配置参数为默认值：
 *          - MQTT服务器: broker.emqx.io
 *          - MQTT端口: 1883
 *          - 客户端ID: rfid_gateway_0001
 *          - 发布主题: SMtest
 *          - 订阅主题: SMtest/cmd
 *          - 报警持续时间: 10000ms(10秒)
 *          - 心跳间隔: 300000ms(5分钟)
 *          - 蓝牙启用: true
 */
void ConfigManager::initDefaultConfig() {
    // 清空配置结构体
    memset(&_config, 0, sizeof(SystemConfig));
    
    // MQTT连接配置
    strncpy(_config.server, "broker.emqx.io", sizeof(_config.server) - 1);
    _config.port = 1883;
    strncpy(_config.clientId, "rfid_gateway_0001", sizeof(_config.clientId) - 1);
    strncpy(_config.topic, "SMtest", sizeof(_config.topic) - 1);
    strncpy(_config.subTopic, "SMtest/cmd", sizeof(_config.subTopic) - 1);
    
    // 系统参数配置
    _config.alarmDuration = 10000;     // 报警持续10秒
    _config.heartbeatInterval = 300000; // 心跳间隔5分钟
    _config.bleEnabled = 1;            // 蓝牙启用
}

/**
 * @brief 初始化配置管理器
 * @return true-初始化成功，false-初始化失败
 * @details 依次调用loadConfig()和loadWhitelist()加载配置，
 *          加载失败时使用默认配置
 */
bool ConfigManager::begin() {
    loadConfig();
    loadWhitelist();
    return true;
}

/**
 * @brief 从Flash加载系统配置
 * @return true-加载成功，false-加载失败(使用默认配置)
 * @details 1. 打开Preferences命名空间(只读模式)
 *          2. 检查配置有效性标记(config_valid)
 *          3. 读取所有配置参数(MQTT服务器、端口、客户端ID等)
 *          4. 关闭Preferences
 */
bool ConfigManager::loadConfig() {
    // 打开Preferences命名空间(只读模式)
    if (!_prefs.begin(CONFIG_NAMESPACE, true)) {
        Serial0.println("[配置] 打开配置存储失败，使用默认配置");
        return false;
    }
    
    // 检查配置是否有效
    if (!_prefs.getBool("config_valid", false)) {
        Serial0.println("[配置] 配置无效，使用默认配置");
        _prefs.end();
        return false;
    }
    
    // 读取配置参数
    String server = _prefs.getString("server", "");
    String clientId = _prefs.getString("clientId", "");
    String username = _prefs.getString("username", "");
    String password = _prefs.getString("password", "");
    String topic = _prefs.getString("topic", "");
    String subTopic = _prefs.getString("subTopic", "");
    
    // 复制到配置结构体
    strncpy(_config.server, server.c_str(), sizeof(_config.server) - 1);
    _config.port = _prefs.getUInt("port", 1883);
    strncpy(_config.clientId, clientId.c_str(), sizeof(_config.clientId) - 1);
    strncpy(_config.username, username.c_str(), sizeof(_config.username) - 1);
    strncpy(_config.password, password.c_str(), sizeof(_config.password) - 1);
    strncpy(_config.topic, topic.c_str(), sizeof(_config.topic) - 1);
    strncpy(_config.subTopic, subTopic.c_str(), sizeof(_config.subTopic) - 1);
    
    // 读取系统参数
    _config.alarmDuration = _prefs.getUInt("alarmDuration", 10000);
    _config.heartbeatInterval = _prefs.getUInt("heartbeatInterval", 300000);
    _config.bleEnabled = _prefs.getUInt("bleEnabled", 1);
    
    // 关闭Preferences
    _prefs.end();
    
    Serial0.println("[配置] 配置加载成功");
    return true;
}

/**
 * @brief 保存系统配置到Flash
 * @return true-保存成功，false-保存失败
 * @details 1. 打开Preferences命名空间(读写模式)
 *          2. 写入所有配置参数
 *          3. 设置配置有效标记(config_valid)
 *          4. 关闭Preferences
 */
bool ConfigManager::saveConfig() {
    // 打开Preferences命名空间(读写模式)
    if (!_prefs.begin(CONFIG_NAMESPACE, false)) {
        Serial0.println("[配置] 打开配置存储失败");
        return false;
    }
    
    // 写入MQTT连接配置
    _prefs.putString("server", _config.server);
    _prefs.putUInt("port", _config.port);
    _prefs.putString("clientId", _config.clientId);
    _prefs.putString("username", _config.username);
    _prefs.putString("password", _config.password);
    _prefs.putString("topic", _config.topic);
    _prefs.putString("subTopic", _config.subTopic);
    
    // 写入系统参数配置
    _prefs.putUInt("alarmDuration", _config.alarmDuration);
    _prefs.putUInt("heartbeatInterval", _config.heartbeatInterval);
    _prefs.putUInt("bleEnabled", _config.bleEnabled);
    
    // 设置配置有效标记
    _prefs.putBool("config_valid", true);
    
    // 关闭Preferences
    _prefs.end();
    
    Serial0.println("[配置] 配置保存成功");
    return true;
}

/**
 * @brief 重置配置为默认值
 * @details 1. 调用initDefaultConfig()初始化默认配置
 *          2. 调用clearWhitelist()清空白名单
 *          3. 调用saveConfig()保存系统配置到Flash
 *          4. 调用saveWhitelist()保存白名单到Flash
 */
void ConfigManager::resetConfig() {
    initDefaultConfig();
    clearWhitelist();
    saveConfig();
    saveWhitelist();
    Serial0.println("[配置] 配置已重置为默认值");
}

/**
 * @brief 获取系统配置引用
 * @return SystemConfig引用
 * @details 返回当前配置的引用，调用方可以直接修改配置参数
 */
SystemConfig& ConfigManager::getConfig() {
    return _config;
}

/**
 * @brief 设置系统配置
 * @param config 新的系统配置
 * @return true-设置成功，false-设置失败
 * @details 复制配置参数并保存到Flash
 */
bool ConfigManager::setConfig(const SystemConfig& config) {
    memcpy(&_config, &config, sizeof(SystemConfig));
    return saveConfig();
}

/**
 * @brief 添加EPC到白名单
 * @param epc 标签EPC字符串
 * @return true-添加成功，false-参数无效或白名单已满
 * @details 1. 参数校验(非空检查)
 *          2. 检查白名单是否已满(超过MAX_WHITELIST_SIZE)
 *          3. 检查EPC是否已存在(避免重复)
 *          4. 添加EPC到白名单数组
 *          5. 增加计数
 */
bool ConfigManager::addToWhitelist(const char* epc) {
    // 参数校验
    if (epc == nullptr || strlen(epc) == 0) {
        return false;
    }
    
    // 检查白名单是否已满
    if (_whitelistCount >= MAX_WHITELIST_SIZE) {
        Serial0.println("[配置] 白名单已满");
        return false;
    }
    
    // 检查EPC是否已存在
    for (int i = 0; i < _whitelistCount; i++) {
        if (strcmp(_whitelist[i], epc) == 0) {
            Serial0.println("[配置] EPC已在白名单中");
            return true;
        }
    }
    
    // 添加EPC到白名单
    strncpy(_whitelist[_whitelistCount], epc, MAX_EPC_LENGTH - 1);
    _whitelistCount++;
    
    Serial0.printf("[配置] 添加EPC到白名单: %s, 当前数量: %d\n", epc, _whitelistCount);
    
    return true;
}

/**
 * @brief 从白名单移除EPC
 * @param epc 标签EPC字符串
 * @return true-移除成功，false-参数无效或EPC不在白名单中
 * @details 1. 参数校验(非空检查)
 *          2. 遍历白名单查找EPC
 *          3. 找到后将后面的元素前移
 *          4. 减少计数，清空最后一个位置
 */
bool ConfigManager::removeFromWhitelist(const char* epc) {
    // 参数校验
    if (epc == nullptr || strlen(epc) == 0) {
        return false;
    }
    
    // 遍历白名单查找EPC
    for (int i = 0; i < _whitelistCount; i++) {
        if (strcmp(_whitelist[i], epc) == 0) {
            // 将后面的元素前移
            for (int j = i; j < _whitelistCount - 1; j++) {
                strncpy(_whitelist[j], _whitelist[j + 1], MAX_EPC_LENGTH);
            }
            // 减少计数
            _whitelistCount--;
            // 清空最后一个位置
            memset(_whitelist[_whitelistCount], 0, MAX_EPC_LENGTH);
            
            Serial0.printf("[配置] 从白名单移除EPC: %s, 当前数量: %d\n", epc, _whitelistCount);
            
            return true;
        }
    }
    
    Serial0.println("[配置] EPC不在白名单中");
    return false;
}

/**
 * @brief 检查EPC是否在白名单中
 * @param epc 标签EPC字符串
 * @return true-在白名单中，false-不在白名单中
 * @details 遍历白名单数组，逐个比较EPC字符串
 */
bool ConfigManager::isInWhitelist(const char* epc) {
    // 参数校验
    if (epc == nullptr || strlen(epc) == 0) {
        return false;
    }
    
    // 遍历白名单查找EPC
    for (int i = 0; i < _whitelistCount; i++) {
        if (strcmp(_whitelist[i], epc) == 0) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief 获取白名单数量
 * @return 白名单中EPC的数量
 */
int ConfigManager::getWhitelistCount() {
    return _whitelistCount;
}

/**
 * @brief 获取白名单指定位置的EPC
 * @param index 索引位置(0-based)
 * @param epc 输出参数，存储EPC字符串
 * @param maxLen epc缓冲区最大长度
 * @return true-获取成功，false-索引无效或参数为空
 * @details 1. 参数校验(索引范围检查)
 *          2. 复制EPC到输出缓冲区
 */
bool ConfigManager::getWhitelistItem(int index, char* epc, int maxLen) {
    // 参数校验
    if (index < 0 || index >= _whitelistCount || epc == nullptr) {
        return false;
    }
    
    // 复制EPC到输出缓冲区
    strncpy(epc, _whitelist[index], maxLen - 1);
    return true;
}

/**
 * @brief 清空白名单
 * @return true-清空成功
 * @details 重置计数为0，使用memset清空白名单数组
 */
bool ConfigManager::clearWhitelist() {
    _whitelistCount = 0;
    memset(_whitelist, 0, sizeof(_whitelist));
    Serial0.println("[配置] 白名单已清空");
    return true;
}

/**
 * @brief 保存白名单到Flash
 * @return true-保存成功，false-保存失败
 * @details 1. 打开Preferences命名空间(读写模式)
 *          2. 写入白名单数量
 *          3. 逐个写入每个EPC(键名为"wl_0", "wl_1", ...)
 *          4. 关闭Preferences
 */
bool ConfigManager::saveWhitelist() {
    // 打开Preferences命名空间(读写模式)
    if (!_prefs.begin(CONFIG_NAMESPACE, false)) {
        Serial0.println("[配置] 打开配置存储失败");
        return false;
    }
    
    // 写入白名单数量
    _prefs.putUInt("whitelist_count", _whitelistCount);
    
    // 逐个写入每个EPC
    for (int i = 0; i < _whitelistCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "wl_%d", i);
        _prefs.putString(key, _whitelist[i]);
    }
    
    // 关闭Preferences
    _prefs.end();
    
    Serial0.printf("[配置] 白名单保存成功, 数量: %d\n", _whitelistCount);
    return true;
}

/**
 * @brief 从Flash加载白名单
 * @return true-加载成功，false-加载失败
 * @details 1. 打开Preferences命名空间(只读模式)
 *          2. 读取白名单数量(限制最大为MAX_WHITELIST_SIZE)
 *          3. 逐个读取每个EPC(键名为"wl_0", "wl_1", ...)
 *          4. 关闭Preferences
 */
bool ConfigManager::loadWhitelist() {
    // 打开Preferences命名空间(只读模式)
    if (!_prefs.begin(CONFIG_NAMESPACE, true)) {
        Serial0.println("[配置] 打开配置存储失败");
        return false;
    }
    
    // 读取白名单数量
    _whitelistCount = _prefs.getUInt("whitelist_count", 0);
    
    // 限制最大数量
    if (_whitelistCount > MAX_WHITELIST_SIZE) {
        _whitelistCount = MAX_WHITELIST_SIZE;
    }
    
    // 逐个读取每个EPC
    for (int i = 0; i < _whitelistCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "wl_%d", i);
        String epc = _prefs.getString(key, "");
        strncpy(_whitelist[i], epc.c_str(), MAX_EPC_LENGTH - 1);
    }
    
    // 关闭Preferences
    _prefs.end();
    
    Serial0.printf("[配置] 白名单加载成功, 数量: %d\n", _whitelistCount);
    return true;
}