/**
 * @file OtaManager.cpp
 * @brief OTA固件升级管理器实现文件
 * @details 实现OtaManager类的所有成员函数，包括：
 *          - GitHub Releases API调用
 *          - HTTP下载与重定向处理
 *          - 固件写入Flash
 *          - 下载进度回调
 *          - 自动重试机制
 */

#include "OtaManager.h"
#include "NetworkManager.h"

/**
 * @var s_instance
 * @brief OtaManager单例指针
 * @details 用于在静态回调函数中访问类成员
 */
static OtaManager* s_instance = nullptr;

/**
 * @brief 构造函数
 * @details 使用初始化列表初始化所有成员变量：
 *          - 状态初始化为OTA_IDLE
 *          - 进度初始化为0
 *          - 升级标志初始化为false
 *          - 回调函数初始化为nullptr
 *          - 清空状态消息和版本号缓冲区
 */
OtaManager::OtaManager() 
    : _status(OTA_IDLE), _progress(0), _updateInProgress(false),
      _progressCallback(nullptr), _statusCallback(nullptr), _networkManager(nullptr),
      _progressUserData(nullptr), _statusUserData(nullptr) {
    memset(_statusMessage, 0, sizeof(_statusMessage));
    memset(_currentVersion, 0, sizeof(_currentVersion));
}

/**
 * @brief 初始化OTA管理器
 * @return true-初始化成功，false-初始化失败
 * @details 1. 设置单例指针
 *          2. 注册HTTPUpdate进度回调，将进度转换为百分比并触发自定义回调
 *          3. 打印初始化完成日志
 */
bool OtaManager::begin() {
    // 设置单例指针，供静态回调使用
    s_instance = this;
    
    // 注册HTTPUpdate进度回调
    httpUpdate.onProgress([](int progress, int total) {
        if (s_instance != nullptr) {
            int percent = 0;
            if (total > 0) {
                // 计算百分比
                percent = (progress * 100) / total;
            }
            // 更新内部进度
            s_instance->_progress = percent;
            // 触发进度回调(如果已设置)
            if (s_instance->_progressCallback != nullptr) {
                s_instance->_progressCallback(percent, s_instance->_progressUserData);
            }
        }
    });
    
    Serial0.println("[OTA] OTA管理器初始化完成");
    return true;
}

/**
 * @brief 设置下载进度回调函数
 * @param callback 进度回调函数指针
 * @param userData 用户数据指针
 * @details 保存回调函数指针和用户数据，下载时触发回调
 */
void OtaManager::setProgressCallback(OtaProgressCallback callback, void* userData) {
    _progressCallback = callback;
    _progressUserData = userData;
}

/**
 * @brief 设置状态变化回调函数
 * @param callback 状态回调函数指针
 * @param userData 用户数据指针
 * @details 保存回调函数指针和用户数据，状态变化时触发回调
 */
void OtaManager::setStatusCallback(OtaStatusCallback callback, void* userData) {
    _statusCallback = callback;
    _statusUserData = userData;
}

/**
 * @brief 设置网络管理器指针
 * @param networkManager 网络管理器指针
 * @details 用于在下载过程中调用networkManager.update()保持MQTT连接活跃
 */
void OtaManager::setNetworkManager(NetworkManager* networkManager) {
    _networkManager = networkManager;
}

/**
 * @brief 获取当前OTA状态
 * @return OtaStatus枚举值
 */
OtaStatus OtaManager::getStatus() {
    return _status;
}

/**
 * @brief 获取当前状态消息
 * @return 状态描述字符串
 */
const char* OtaManager::getStatusMessage() {
    return _statusMessage;
}

/**
 * @brief 获取当前下载进度
 * @return 进度百分比(0-100)
 */
int OtaManager::getProgress() {
    return _progress;
}

/**
 * @brief 从指定URL开始升级
 * @param url 固件下载URL
 * @return true-升级启动成功，false-启动失败
 * @details 直接调用downloadAndUpdate执行升级
 */
bool OtaManager::startUpdate(const char* url) {
    return downloadAndUpdate(url);
}

/**
 * @brief 检查GitHub Releases是否有新版本
 * @param repo GitHub仓库地址(格式: owner/repo)
 * @param latestVersion 输出参数，存储最新版本号
 * @param maxLen latestVersion缓冲区最大长度
 * @return true-有新版本，false-无新版本或检查失败
 * @details 1. 验证仓库地址有效性
 *          2. 构建GitHub API URL
 *          3. 发送HTTP GET请求获取最新发布信息
 *          4. 解析JSON响应提取tag_name
 *          5. 比较最新版本与当前版本
 */
bool OtaManager::checkUpdate(const char* repo, char* latestVersion, int maxLen) {
    // 参数校验
    if (repo == nullptr || strlen(repo) == 0) {
        setStatus(OTA_FAILED, "无效的仓库地址");
        return false;
    }
    
    // 设置状态为检查中
    setStatus(OTA_CHECKING, "正在检查更新...");
    
    // 构建GitHub API URL
    char apiUrl[256];
    snprintf(apiUrl, sizeof(apiUrl), "https://api.github.com/repos/%s/releases/latest", repo);
    
    Serial0.printf("[OTA] 检查更新: %s\n", apiUrl);
    
    // 创建安全客户端(用于HTTPS)
    WiFiClientSecure* secureClient = new WiFiClientSecure();
    if (secureClient == nullptr) {
        setStatus(OTA_FAILED, "创建安全客户端失败");
        return false;
    }
    secureClient->setInsecure();
    
    // 创建HTTPClient实例
    HTTPClient http;
    if (!http.begin(*secureClient, apiUrl)) {
        setStatus(OTA_FAILED, "HTTP连接失败");
        delete secureClient;
        http.end();
        return false;
    }
    
    // 设置连接超时(15秒)
    http.setConnectTimeout(15000);
    // 设置总超时(30秒)
    http.setTimeout(30000);
    
    // 添加GitHub API接受头
    http.addHeader("Accept", "application/vnd.github.v3+json");
    
    // 发送GET请求
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial0.printf("[OTA] GitHub API HTTP状态码: %d\n", httpCode);
        setStatus(OTA_FAILED, "获取版本信息失败");
        http.end();
        delete secureClient;
        return false;
    }
    
    // 获取响应内容到堆缓冲区(避免栈溢出)
    int payloadLen = http.getSize();
    if (payloadLen <= 0 || payloadLen > 4096) {
        payloadLen = 4096;
    }
    char* payload = (char*)malloc(payloadLen);
    if (payload == nullptr) {
        Serial0.println("[OTA] 分配缓冲区失败");
        http.end();
        delete secureClient;
        setStatus(OTA_FAILED, "内存不足");
        return false;
    }
    int readLen = http.getStream().readBytes((uint8_t*)payload, payloadLen - 1);
    payload[readLen] = '\0';
    
    http.end();
    delete secureClient;
    
    // 手动字符串解析提取tag_name(版本号)
    const char* tagNameStart = strstr(payload, "\"tag_name\":\"");
    if (tagNameStart != nullptr) {
        tagNameStart += 12;
        const char* tagNameEnd = strchr(tagNameStart, '\"');
        if (tagNameEnd != nullptr) {
            int tagLen = tagNameEnd - tagNameStart;
            if (tagLen > 0 && tagLen < maxLen) {
                strncpy(latestVersion, tagNameStart, tagLen);
                latestVersion[tagLen] = '\0';
            } else {
                latestVersion[0] = '\0';
            }
        }
    }
    
    free(payload);
    
    if (latestVersion[0] != '\0') {
        Serial0.printf("[OTA] 最新版本: %s, 当前版本: %s\n", latestVersion, getCurrentVersion());
        setStatus(OTA_IDLE, "检查完成");
        return strcmp(latestVersion, getCurrentVersion()) != 0;
    }
    
    setStatus(OTA_FAILED, "未找到版本信息");
    return false;
}

/**
 * @brief 从GitHub Releases升级
 * @param repo GitHub仓库地址(格式: owner/repo)
 * @param assetName 资产文件名(默认nullptr，自动查找.bin文件)
 * @return true-升级启动成功，false-启动失败
 * @details 1. 验证仓库地址有效性
 *          2. 调用getLatestReleaseAsset获取下载URL和版本号
 *          3. 调用downloadAndUpdate执行升级
 */
bool OtaManager::updateFromGithub(const char* repo, const char* assetName) {
    // 参数校验
    if (repo == nullptr || strlen(repo) == 0) {
        setStatus(OTA_FAILED, "无效的仓库地址");
        return false;
    }
    
    char downloadUrl[512];
    char version[32];
    
    // 获取最新发布资产的下载URL
    if (!getLatestReleaseAsset(repo, assetName, downloadUrl, sizeof(downloadUrl), version, sizeof(version))) {
        return false;
    }
    
    Serial0.printf("[OTA] 开始升级，版本: %s, URL: %s\n", version, downloadUrl);
    
    // 执行下载和升级
    return downloadAndUpdate(downloadUrl);
}

/**
 * @brief 获取GitHub Releases最新资产下载地址
 * @param repo GitHub仓库地址(格式: owner/repo)
 * @param assetName 指定的资产文件名(可为nullptr)
 * @param downloadUrl 输出参数，存储下载URL
 * @param maxLen downloadUrl缓冲区最大长度
 * @param version 输出参数，存储版本号
 * @param versionMaxLen version缓冲区最大长度
 * @return true-获取成功，false-获取失败
 * @details 1. 构建GitHub API URL
 *          2. 发送HTTP GET请求获取最新发布信息
 *          3. 解析JSON响应提取tag_name和assets列表
 *          4. 根据优先级选择资产(.bin > .zip > 其他)
 */
bool OtaManager::getLatestReleaseAsset(const char* repo, const char* assetName, char* downloadUrl, int maxLen, char* version, int versionMaxLen) {
    // 构建GitHub API URL
    char apiUrl[256];
    snprintf(apiUrl, sizeof(apiUrl), "https://api.github.com/repos/%s/releases/latest", repo);
    
    // 创建安全客户端(用于HTTPS)
    WiFiClientSecure* secureClient = new WiFiClientSecure();
    if (secureClient == nullptr) {
        setStatus(OTA_FAILED, "创建安全客户端失败");
        return false;
    }
    secureClient->setInsecure();
    
    // 创建HTTPClient实例
    HTTPClient http;
    if (!http.begin(*secureClient, apiUrl)) {
        setStatus(OTA_FAILED, "HTTP连接失败");
        delete secureClient;
        http.end();
        return false;
    }
    
    // 设置连接超时(15秒)
    http.setConnectTimeout(15000);
    // 设置总超时(30秒)
    http.setTimeout(30000);
    
    // 添加GitHub API接受头
    http.addHeader("Accept", "application/vnd.github.v3+json");
    
    // 发送GET请求
    int httpCode = http.GET();
    Serial0.printf("[OTA] HTTP状态码: %d\n", httpCode);
    if (httpCode != HTTP_CODE_OK) {
        String errorMsg = http.getString();
        http.end();
        delete secureClient;
        Serial0.printf("[OTA] HTTP错误响应: %s\n", errorMsg.c_str());
        setStatus(OTA_FAILED, "获取版本信息失败");
        return false;
    }
    
    // 获取响应内容到堆缓冲区(避免栈溢出，栈上分配大数组会导致stack overflow)
    int payloadLen = http.getSize();
    if (payloadLen <= 0 || payloadLen > 8192) {
        payloadLen = 8192;
    }
    char* payload = (char*)malloc(payloadLen);
    if (payload == nullptr) {
        Serial0.println("[OTA] 分配缓冲区失败");
        http.end();
        delete secureClient;
        setStatus(OTA_FAILED, "内存不足");
        return false;
    }
    int readLen = http.getStream().readBytes((uint8_t*)payload, payloadLen - 1);
    payload[readLen] = '\0';
    
    http.end();
    delete secureClient;
    
    Serial0.printf("[OTA] 响应长度: %d\n", readLen);
    Serial0.printf("[OTA] 响应内容(前200字符): %s\n", String(payload).substring(0, min(readLen, 200)).c_str());
    
    // 手动字符串解析提取tag_name(版本号)
    const char* tagNameStart = strstr(payload, "\"tag_name\":\"");
    if (tagNameStart != nullptr) {
        tagNameStart += 12;
        const char* tagNameEnd = strchr(tagNameStart, '\"');
        if (tagNameEnd != nullptr) {
            int tagLen = tagNameEnd - tagNameStart;
            if (tagLen > 0 && tagLen < versionMaxLen) {
                strncpy(version, tagNameStart, tagLen);
                version[tagLen] = '\0';
            }
        }
    }
    if (version[0] == '\0') {
        Serial0.println("[OTA] 警告: 未找到版本号");
    }
    
    // 手动字符串解析提取assets中的browser_download_url
    const char* assetsStart = strstr(payload, "\"assets\":[");
    if (assetsStart == nullptr) {
        Serial0.println("[OTA] 警告: 主响应中未找到assets数组，尝试从assets_url获取");
        
        // 尝试从assets_url获取资产列表
        const char* assetsUrlStart = strstr(payload, "\"assets_url\":\"");
        if (assetsUrlStart != nullptr) {
            assetsUrlStart += 14;
            const char* assetsUrlEnd = strchr(assetsUrlStart, '\"');
            if (assetsUrlEnd != nullptr) {
                int assetsUrlLen = assetsUrlEnd - assetsUrlStart;
                if (assetsUrlLen > 0 && assetsUrlLen < 512) {
                    char assetsUrl[512];
                    strncpy(assetsUrl, assetsUrlStart, assetsUrlLen);
                    assetsUrl[assetsUrlLen] = '\0';
                    
                    // 释放当前payload
                    free(payload);
                    
                    // 重新获取资产列表
                    WiFiClientSecure* assetClient = new WiFiClientSecure();
                    if (assetClient != nullptr) {
                        assetClient->setInsecure();
                        HTTPClient assetHttp;
                        if (assetHttp.begin(*assetClient, assetsUrl)) {
                            assetHttp.setConnectTimeout(15000);
                            assetHttp.setTimeout(30000);
                            assetHttp.addHeader("Accept", "application/vnd.github.v3+json");
                            
                            int assetHttpCode = assetHttp.GET();
                            Serial0.printf("[OTA] 资产列表HTTP状态码: %d\n", assetHttpCode);
                            if (assetHttpCode == HTTP_CODE_OK) {
                                int assetPayloadLen = assetHttp.getSize();
                                if (assetPayloadLen <= 0 || assetPayloadLen > 8192) {
                                    assetPayloadLen = 8192;
                                }
                                payload = (char*)malloc(assetPayloadLen);
                                if (payload != nullptr) {
                                    int assetReadLen = assetHttp.getStream().readBytes((uint8_t*)payload, assetPayloadLen - 1);
                                    payload[assetReadLen] = '\0';
                                    Serial0.printf("[OTA] 资产列表响应长度: %d\n", assetReadLen);
                                    
                                    // 再次查找assets数组(这次应该是数组格式)
                                    assetsStart = strstr(payload, "[");
                                    if (assetsStart != nullptr) {
                                        goto parseAssets;
                                    }
                                }
                            }
                            assetHttp.end();
                        }
                        delete assetClient;
                    }
                }
            }
        }
        
        free(payload);
        setStatus(OTA_FAILED, "未找到发布资产");
        return false;
    }
    assetsStart += 10;

parseAssets:
    
    // 优先级变量：.bin > .zip > 其他(使用堆缓冲区，避免栈溢出)
    char* binUrl = (char*)malloc(512);
    char* zipUrl = (char*)malloc(512);
    char* fallbackUrl = (char*)malloc(512);
    char* tempName = (char*)malloc(128);
    char* tempUrl = (char*)malloc(512);
    if (binUrl == nullptr || zipUrl == nullptr || fallbackUrl == nullptr ||
        tempName == nullptr || tempUrl == nullptr) {
        Serial0.println("[OTA] 分配临时缓冲区失败");
        free(binUrl); free(zipUrl); free(fallbackUrl);
        free(tempName); free(tempUrl);
        free(payload);
        setStatus(OTA_FAILED, "内存不足");
        return false;
    }
    memset(binUrl, 0, 512);
    memset(zipUrl, 0, 512);
    memset(fallbackUrl, 0, 512);
    
    // 在assets数组中查找资产信息
    const char* pos = assetsStart;
    while (pos != nullptr && *pos != '\0') {
        const char* nameStart = strstr(pos, "\"name\":\"");
        const char* urlStart = strstr(pos, "\"browser_download_url\":\"");
        
        if (nameStart != nullptr && urlStart != nullptr) {
            nameStart += 8;
            const char* nameEnd = strchr(nameStart, '\"');
            if (nameEnd == nullptr) break;
            int nameLen = nameEnd - nameStart;
            
            urlStart += 24;
            const char* urlEnd = strchr(urlStart, '\"');
            if (urlEnd == nullptr) break;
            int urlLen = urlEnd - urlStart;
            
            if (nameLen > 0 && urlLen > 0 && urlLen < 512) {
                strncpy(tempName, nameStart, nameLen);
                tempName[nameLen] = '\0';
                
                strncpy(tempUrl, urlStart, urlLen);
                tempUrl[urlLen] = '\0';
                
                if (assetName != nullptr) {
                    if (strcmp(tempName, assetName) == 0) {
                        strncpy(downloadUrl, tempUrl, maxLen - 1);
                        downloadUrl[maxLen - 1] = '\0';
                        Serial0.printf("[OTA] 找到资产: %s\n", tempName);
                        free(binUrl); free(zipUrl); free(fallbackUrl);
                        free(tempName); free(tempUrl);
                        free(payload);
                        return true;
                    }
                } else {
                    int nameLenTemp = strlen(tempName);
                    if (nameLenTemp >= 4 && strcmp(tempName + nameLenTemp - 4, ".bin") == 0) {
                        strncpy(binUrl, tempUrl, 511);
                    } else if (nameLenTemp >= 4 && strcmp(tempName + nameLenTemp - 4, ".zip") == 0) {
                        strncpy(zipUrl, tempUrl, 511);
                    } else if (fallbackUrl[0] == '\0') {
                        strncpy(fallbackUrl, tempUrl, 511);
                    }
                }
            }
            
            pos = urlEnd + 1;
        } else {
            break;
        }
    }
    
    if (assetName != nullptr) {
        free(binUrl); free(zipUrl); free(fallbackUrl);
        free(tempName); free(tempUrl);
        free(payload);
        setStatus(OTA_FAILED, "未找到匹配的资产文件");
        return false;
    }
    
    if (binUrl[0] != '\0') {
        strncpy(downloadUrl, binUrl, maxLen - 1);
        downloadUrl[maxLen - 1] = '\0';
        Serial0.println("[OTA] 找到.bin固件文件");
        free(binUrl); free(zipUrl); free(fallbackUrl);
        free(tempName); free(tempUrl);
        free(payload);
        return true;
    } else if (zipUrl[0] != '\0') {
        strncpy(downloadUrl, zipUrl, maxLen - 1);
        downloadUrl[maxLen - 1] = '\0';
        Serial0.println("[OTA] 找到.zip固件文件");
        free(binUrl); free(zipUrl); free(fallbackUrl);
        free(tempName); free(tempUrl);
        free(payload);
        return true;
    } else if (fallbackUrl[0] != '\0') {
        strncpy(downloadUrl, fallbackUrl, maxLen - 1);
        downloadUrl[maxLen - 1] = '\0';
        Serial0.println("[OTA] 使用备用资产文件");
        free(binUrl); free(zipUrl); free(fallbackUrl);
        free(tempName); free(tempUrl);
        free(payload);
        return true;
    }
    
    free(binUrl); free(zipUrl); free(fallbackUrl);
    free(tempName); free(tempUrl);
    free(payload);
    setStatus(OTA_FAILED, "未找到可用的资产文件");
    return false;
}

/**
 * @brief 下载固件并执行升级(核心函数)
 * @param url 固件下载URL
 * @return true-升级成功(会重启)，false-升级失败
 * @details 核心升级流程：
 *          1. 参数校验和状态检查
 *          2. 外层循环：自动重试机制(最多3次)
 *          3. 内层循环：HTTP重定向处理(最多5次)
 *          4. 内存分配(堆上分配4KB缓冲区)
 *          5. 分段下载并写入Flash
 *          6. 进度回调和MQTT连接保持
 *          7. 下载超时检测(30秒无数据)
 *          8. 固件验证和重启
 */
bool OtaManager::downloadAndUpdate(const char* url) {
    // 检查是否正在升级中
    if (_updateInProgress) {
        setStatus(OTA_FAILED, "升级正在进行中");
        return false;
    }
    
    // 参数校验
    if (url == nullptr || strlen(url) == 0) {
        setStatus(OTA_FAILED, "无效的升级URL");
        return false;
    }
    
    // 设置升级标志和初始进度
    _updateInProgress = true;
    _progress = 0;
    
    // 设置状态为下载中
    setStatus(OTA_DOWNLOADING, "开始下载固件...");
    
    Serial0.printf("[OTA] 开始升级，URL: %s\n", url);
    
    // 最大重试次数(增加到5次以应对不稳定网络)
    const int maxRetries = 5;
    for (int retry = 0; retry < maxRetries; retry++) {
        Serial0.printf("[OTA] 下载尝试: %d/%d\n", retry + 1, maxRetries);
        unsigned int freeHeap = ESP.getFreeHeap();
        Serial0.printf("[OTA] 可用内存: %d 字节\n", freeHeap);
        
        // 检查内存是否充足(WiFiClientSecure需要约50KB TLS缓冲区)
        if (freeHeap < 50000) {
            Serial0.println("[OTA] 内存不足，等待GC回收...");
            for (int i = 0; i < 5; i++) {
                delay(100);
                yield();
            }
            freeHeap = ESP.getFreeHeap();
            Serial0.printf("[OTA] GC后可用内存: %d 字节\n", freeHeap);
            if (freeHeap < 50000) {
                Serial0.println("[OTA] 内存仍然不足，跳过本次尝试");
                if (retry < maxRetries - 1) {
                    delay(3000);
                    continue;
                }
                setStatus(OTA_FAILED, "内存不足");
                _updateInProgress = false;
                return false;
            }
        }
        
        // 检查WiFi信号强度(信号太差时等待)
        int rssi = WiFi.RSSI();
        Serial0.printf("[OTA] 当前WiFi信号强度: %d dBm\n", rssi);
        if (rssi < -85) {
            Serial0.println("[OTA] WiFi信号太弱，等待信号改善...");
            int waitCount = 0;
            while (WiFi.RSSI() < -85 && waitCount < 30) {
                delay(500);
                yield();
                waitCount++;
            }
            Serial0.printf("[OTA] 等待后信号强度: %d dBm\n", WiFi.RSSI());
        }
        
        String currentUrl = url;
        HTTPClient http;
        WiFiClientSecure* secureClient = nullptr;
        int httpCode = 0;
        
        // 处理HTTP重定向(最多5次)
        for (int redirectCount = 0; redirectCount < 5; redirectCount++) {
            // 创建安全客户端(用于HTTPS)
            secureClient = new WiFiClientSecure();
            if (secureClient == nullptr) {
                Serial0.println("[OTA] WiFiClientSecure创建失败");
                httpCode = -2;
                break;
            }
            
            // 禁用证书验证(适用于自签名证书或无法验证的服务器)
            secureClient->setInsecure();
            
            // 检查可用内存(WiFiClientSecure需要约50KB TLS缓冲区)
            unsigned int freeHeap = ESP.getFreeHeap();
            Serial0.printf("[OTA] HTTP连接前内存: %d 字节\n", freeHeap);
            if (freeHeap < 40960) {
                Serial0.println("[OTA] 内存不足，无法创建HTTPS连接");
                httpCode = -4;
                delete secureClient;
                secureClient = nullptr;
                break;
            }
            
            // 使用安全客户端连接
            if (!http.begin(*secureClient, currentUrl)) {
                Serial0.println("[OTA] HTTP连接初始化失败");
                httpCode = -3;
                delete secureClient;
                secureClient = nullptr;
                break;
            }
            
            // 设置连接超时(30秒，适配慢网络)
            http.setConnectTimeout(30000);
            // 设置总超时(60秒，适配慢网络，uint16_t最大值限制)
            http.setTimeout(60000);
            
            // 收集Location响应头(用于重定向)
            const char* headerKeys[] = {"Location"};
            http.collectHeaders(headerKeys, 1);
            
            // 发送GET请求
            httpCode = http.GET();
            Serial0.printf("[OTA] HTTP状态码: %d (重定向次数: %d)\n", httpCode, redirectCount);
            
            // 处理重定向(301或302)
            if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND) {
                String redirectUrl = http.header("Location");
                Serial0.printf("[OTA] 重定向到: %s\n", redirectUrl.c_str());
                http.end();
                delete secureClient;
                secureClient = nullptr;
                
                // 检查重定向URL是否有效
                if (redirectUrl.isEmpty()) {
                    Serial0.println("[OTA] 重定向地址为空");
                    break;
                }
                // 更新URL继续处理
                currentUrl = redirectUrl;
                continue;
            }
            
            // 非重定向状态码，跳出重定向循环
            break;
        }
        
        // 检查HTTP状态码是否为200
        if (httpCode != HTTP_CODE_OK) {
            Serial0.printf("[OTA] HTTP下载失败，状态码: %d\n", httpCode);
            // 清理资源
            http.end();
            if (secureClient != nullptr) {
                delete secureClient;
                secureClient = nullptr;
            }
            // 判断是否需要重试(使用指数退避策略)
            if (retry < maxRetries - 1) {
                // 指数退避: 3秒, 6秒, 12秒, 24秒, 48秒...(最大60秒)
                int retryDelay = (1 << retry) * 3; // 2^retry * 3
                if (retryDelay > 60) retryDelay = 60;
                Serial0.printf("[OTA] 指数退避，%d秒后重试...\n", retryDelay);
                // 等待重试(不处理MQTT消息，防止重复触发OTA命令)
                for (int i = 0; i < retryDelay; i++) {
                    delay(1000);
                    yield();
                }
                continue;
            }
            // 重试次数用完，升级失败
            setStatus(OTA_FAILED, "固件下载失败");
            _updateInProgress = false;
            return false;
        }
        
        // 获取固件大小
        int contentLength = http.getSize();
        Serial0.printf("[OTA] 固件大小: %d 字节\n", contentLength);
        
        // 检查固件大小有效性
        if (contentLength <= 0) {
            Serial0.println("[OTA] 无效的固件大小");
            http.end();
            delete secureClient;
            secureClient = nullptr;
            if (retry < maxRetries - 1) {
                Serial0.println("[OTA] 3秒后重试...");
                for (int i = 0; i < 3; i++) {
                    delay(1000);
                    yield();
                    if (_networkManager != nullptr) {
                        _networkManager->update();
                    }
                }
                continue;
            }
            setStatus(OTA_FAILED, "无效的固件大小");
            _updateInProgress = false;
            return false;
        }
        
        // 获取HTTP流指针
        WiFiClient* stream = http.getStreamPtr();
        if (stream == nullptr) {
            Serial0.println("[OTA] 获取HTTP流失败");
            http.end();
            delete secureClient;
            secureClient = nullptr;
            if (retry < maxRetries - 1) {
                Serial0.println("[OTA] 3秒后重试...");
                for (int i = 0; i < 3; i++) {
                    delay(1000);
                    yield();
                }
                continue;
            }
            setStatus(OTA_FAILED, "获取HTTP流失败");
            _updateInProgress = false;
            return false;
        }
        
        // 初始化OTA更新(写入Flash)
        if (!Update.begin(contentLength, U_FLASH)) {
            Serial0.printf("[OTA] Update.begin失败: %s\n", Update.errorString());
            // 清理资源
            http.end();
            delete secureClient;
            secureClient = nullptr;
            // 判断是否需要重试
            if (retry < maxRetries - 1) {
                Serial0.println("[OTA] 3秒后重试...");
                for (int i = 0; i < 3; i++) {
                    delay(1000);
                    yield();
                }
                continue;
            }
            setStatus(OTA_FAILED, "升级初始化失败");
            _updateInProgress = false;
            return false;
        }
        
        // 设置MD5校验(如果服务器提供)
        Update.setMD5(http.header("X-MD5").c_str());
        
        Serial0.println("[OTA] 开始下载并写入固件...");
        
        // 在堆上分配4KB缓冲区(避免栈溢出)
        const size_t BUFFER_SIZE = 4096;
        uint8_t* buffer = (uint8_t*)malloc(BUFFER_SIZE);
        if (buffer == nullptr) {
            Serial0.println("[OTA] 内存分配失败");
            // 清理资源
            http.end();
            delete secureClient;
            secureClient = nullptr;
            Update.abort();
            setStatus(OTA_FAILED, "内存分配失败");
            _updateInProgress = false;
            return false;
        }
        
        // 下载进度变量
        size_t totalWritten = 0;      // 已写入字节数
        int lastProgress = -1;        // 上次报告的进度
        int64_t lastHeartbeat = millis();  // 上次心跳时间
        int64_t lastDataTime = millis();   // 上次收到数据时间
        
        // 分段下载循环
        while (http.connected() && (totalWritten < contentLength)) {
            // 获取可用数据量
            size_t available = stream->available();
            if (available > 0) {
                // 计算本次读取量(不超过缓冲区大小和剩余数据量)
                size_t toRead = min((size_t)available, min((size_t)(contentLength - totalWritten), BUFFER_SIZE));
                // 读取数据到缓冲区
                size_t readBytes = stream->readBytes(buffer, toRead);
                
                if (readBytes > 0) {
                    // 写入Flash
                    size_t written = Update.write(buffer, readBytes);
                    totalWritten += written;
                    // 更新最后数据时间
                    lastDataTime = millis();
                    
                    // 计算并报告进度
                    if (contentLength > 0) {
                        int progress = (totalWritten * 100) / contentLength;
                        if (progress != lastProgress) {
                            lastProgress = progress;
                            _progress = progress;
                            Serial0.printf("[OTA] 进度: %d%%\n", progress);
                            
                            // 每5%触发一次进度回调(减少MQTT消息频率)
                            if (_progressCallback != nullptr && progress % 5 == 0) {
                                _progressCallback(progress, _progressUserData);
                            }
                        }
                    }
                }
            }
            
            // 让出CPU时间(避免看门狗复位)
            yield();
            
            // 更新网络管理器(保持MQTT连接活跃)
            if (_networkManager != nullptr) {
                _networkManager->update();
            }
            
            // 心跳日志(每5秒输出一次)
            int64_t now = millis();
            if (now - lastHeartbeat > 5000) {
                lastHeartbeat = now;
                Serial0.printf("[OTA] 进度: %d%%, 已下载: %d/%d\n", lastProgress, (int)totalWritten, contentLength);
            }
            
            // 下载超时检测(120秒无数据，适配慢网络环境)
            if (now - lastDataTime > 120000) {
                Serial0.println("[OTA] 下载超时，连接挂起");
                break;
            }
            
            // 检查HTTP连接是否断开
            if (!http.connected()) {
                Serial0.println("[OTA] HTTP连接断开");
                break;
            }
        }
        
        // 释放缓冲区内存
        free(buffer);
        
        // 清理HTTP资源
        http.end();
        delete secureClient;
        secureClient = nullptr;
        
        // 检查是否下载完整
        if (totalWritten != contentLength) {
            Serial0.printf("[OTA] 写入字节数: %d, 预期: %d\n", (int)totalWritten, contentLength);
            Serial0.printf("[OTA] 下载失败: %s\n", Update.errorString());
            // 中止升级(安全检查)
            if (Update.isRunning()) {
                Update.abort();
            }
            // 判断是否需要重试(使用指数退避策略)
            if (retry < maxRetries - 1) {
                // 指数退避: 3秒, 6秒, 12秒, 24秒, 48秒...(最大60秒)
                int retryDelay = (1 << retry) * 3;
                if (retryDelay > 60) retryDelay = 60;
                Serial0.printf("[OTA] 指数退避，%d秒后重试...\n", retryDelay);
                // 不处理MQTT消息，防止重复触发OTA命令
                for (int i = 0; i < retryDelay; i++) {
                    delay(1000);
                    yield();
                }
                continue;
            }
            setStatus(OTA_FAILED, "固件下载失败");
            _updateInProgress = false;
            return false;
        }
        
        // 下载完成，开始验证固件
        Serial0.println("[OTA] 下载完成，开始验证固件...");
        
        // 结束升级并验证(参数true表示验证固件)
        if (!Update.end(true)) {
            Serial0.printf("[OTA] Update.end失败: %s\n", Update.errorString());
            // 中止升级(安全检查)
            if (Update.isRunning()) {
                Update.abort();
            }
            // 判断是否需要重试
            if (retry < maxRetries - 1) {
                Serial0.println("[OTA] 3秒后重试...");
                for (int i = 0; i < 3; i++) {
                    delay(1000);
                    yield();
                }
                continue;
            }
            setStatus(OTA_FAILED, "升级结束失败");
            _updateInProgress = false;
            return false;
        }
        
        // 升级成功，打印日志并重启
        Serial0.println("[OTA] 升级成功！");
        delay(500);
        ESP.restart();
    }
    
    // 所有重试都失败
    setStatus(OTA_FAILED, "固件下载失败");
    _updateInProgress = false;
    return false;
}

/**
 * @brief 取消正在进行的升级
 * @details 设置升级标志为false，状态为OTA_FAILED，打印日志
 */
void OtaManager::abortUpdate() {
    if (_updateInProgress) {
        _updateInProgress = false;
        setStatus(OTA_FAILED, "升级已取消");
        Serial0.println("[OTA] 升级已取消");
    }
}

/**
 * @brief 周期性更新函数(需在loop中调用)
 * @details 检查升级完成状态，若完成则重启设备
 */
void OtaManager::update() {
    if (_status == OTA_COMPLETED) {
        Serial0.println("[OTA] 升级完成，即将重启...");
        delay(1000);
        ESP.restart();
    }
}

/**
 * @brief 获取当前固件版本号
 * @return 当前版本号字符串，若未设置返回"unknown"
 */
const char* OtaManager::getCurrentVersion() {
    if (_currentVersion[0] == '\0') {
        return "unknown";
    }
    return _currentVersion;
}

/**
 * @brief 设置当前固件版本号
 * @param version 版本号字符串
 */
void OtaManager::setCurrentVersion(const char* version) {
    if (version != nullptr) {
        strncpy(_currentVersion, version, sizeof(_currentVersion) - 1);
    }
}

/**
 * @brief 设置OTA状态和消息
 * @param status 新状态
 * @param message 状态描述消息
 * @details 更新内部状态，打印日志，并触发状态回调(如果已设置)
 */
void OtaManager::setStatus(OtaStatus status, const char* message) {
    _status = status;
    if (message != nullptr) {
        strncpy(_statusMessage, message, sizeof(_statusMessage) - 1);
        _statusMessage[sizeof(_statusMessage) - 1] = '\0';
    } else {
        _statusMessage[0] = '\0';
    }
    
    Serial0.printf("[OTA] 状态: %d, 消息: %s\n", status, _statusMessage);
    
    // 触发状态回调(安全检查)
    if (_statusCallback != nullptr) {
        try {
            _statusCallback(status, _statusMessage, _statusUserData);
        } catch (...) {
            Serial0.println("[OTA] 状态回调执行异常");
        }
    }
}

/**
 * @brief 处理HTTPUpdate返回结果
 * @param result HTTPUpdate返回值
 * @details 根据结果设置对应的OTA状态：
 *          - HTTP_UPDATE_OK: OTA_COMPLETED
 *          - HTTP_UPDATE_FAILED: OTA_FAILED
 *          - HTTP_UPDATE_NO_UPDATES: OTA_IDLE
 */
void OtaManager::handleUpdateResult(t_httpUpdate_return result) {
    _updateInProgress = false;
    
    switch (result) {
        case HTTP_UPDATE_OK:
            setStatus(OTA_COMPLETED, "固件升级成功");
            break;
            
        case HTTP_UPDATE_FAILED:
            setStatus(OTA_FAILED, httpUpdate.getLastErrorString().c_str());
            break;
            
        case HTTP_UPDATE_NO_UPDATES:
            setStatus(OTA_IDLE, "当前已是最新版本");
            break;
            
        default:
            setStatus(OTA_FAILED, "未知错误");
            break;
    }
}