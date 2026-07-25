#include "OtaManager.h"

static OtaManager* s_instance = nullptr;

OtaManager::OtaManager() 
    : _status(OTA_IDLE), _progress(0), _updateInProgress(false),
      _progressCallback(nullptr), _statusCallback(nullptr) {
    memset(_statusMessage, 0, sizeof(_statusMessage));
    memset(_currentVersion, 0, sizeof(_currentVersion));
}

bool OtaManager::begin() {
    s_instance = this;
    
    httpUpdate.onProgress([](int progress, int total) {
        if (s_instance != nullptr) {
            int percent = 0;
            if (total > 0) {
                percent = (progress * 100) / total;
            }
            s_instance->_progress = percent;
            if (s_instance->_progressCallback != nullptr) {
                s_instance->_progressCallback(percent);
            }
        }
    });
    
    Serial0.println("[OTA] OTA管理器初始化完成");
    return true;
}

void OtaManager::setProgressCallback(OtaProgressCallback callback) {
    _progressCallback = callback;
}

void OtaManager::setStatusCallback(OtaStatusCallback callback) {
    _statusCallback = callback;
}

OtaStatus OtaManager::getStatus() {
    return _status;
}

const char* OtaManager::getStatusMessage() {
    return _statusMessage;
}

int OtaManager::getProgress() {
    return _progress;
}

bool OtaManager::startUpdate(const char* url) {
    return downloadAndUpdate(url);
}

bool OtaManager::checkUpdate(const char* repo, char* latestVersion, int maxLen) {
    if (repo == nullptr || strlen(repo) == 0) {
        setStatus(OTA_FAILED, "无效的仓库地址");
        return false;
    }
    
    setStatus(OTA_CHECKING, "正在检查更新...");
    
    char apiUrl[256];
    snprintf(apiUrl, sizeof(apiUrl), "https://api.github.com/repos/%s/releases/latest", repo);
    
    Serial0.printf("[OTA] 检查更新: %s\n", apiUrl);
    
    HTTPClient http;
    if (!http.begin(apiUrl)) {
        setStatus(OTA_FAILED, "HTTP连接失败");
        http.end();
        return false;
    }
    
    http.addHeader("Accept", "application/vnd.github.v3+json");
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        setStatus(OTA_FAILED, "获取版本信息失败");
        http.end();
        return false;
    }
    
    String payload = http.getString();
    http.end();
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        setStatus(OTA_FAILED, "解析版本信息失败");
        return false;
    }
    
    const char* tagName = doc["tag_name"];
    if (tagName != nullptr) {
        strncpy(latestVersion, tagName, maxLen - 1);
        Serial0.printf("[OTA] 最新版本: %s, 当前版本: %s\n", latestVersion, getCurrentVersion());
        setStatus(OTA_IDLE, "检查完成");
        return strcmp(latestVersion, getCurrentVersion()) != 0;
    }
    
    setStatus(OTA_FAILED, "未找到版本信息");
    return false;
}

bool OtaManager::updateFromGithub(const char* repo, const char* assetName) {
    if (repo == nullptr || strlen(repo) == 0) {
        setStatus(OTA_FAILED, "无效的仓库地址");
        return false;
    }
    
    char downloadUrl[512];
    char version[32];
    
    if (!getLatestReleaseAsset(repo, assetName, downloadUrl, sizeof(downloadUrl), version, sizeof(version))) {
        return false;
    }
    
    Serial0.printf("[OTA] 开始升级，版本: %s, URL: %s\n", version, downloadUrl);
    
    return downloadAndUpdate(downloadUrl);
}

bool OtaManager::getLatestReleaseAsset(const char* repo, const char* assetName, char* downloadUrl, int maxLen, char* version, int versionMaxLen) {
    char apiUrl[256];
    snprintf(apiUrl, sizeof(apiUrl), "https://api.github.com/repos/%s/releases/latest", repo);
    
    HTTPClient http;
    if (!http.begin(apiUrl)) {
        setStatus(OTA_FAILED, "HTTP连接失败");
        http.end();
        return false;
    }
    
    http.addHeader("Accept", "application/vnd.github.v3+json");
    
    int httpCode = http.GET();
    Serial0.printf("[OTA] HTTP状态码: %d\n", httpCode);
    if (httpCode != HTTP_CODE_OK) {
        String errorMsg = http.getString();
        http.end();
        Serial0.printf("[OTA] HTTP错误响应: %s\n", errorMsg.c_str());
        setStatus(OTA_FAILED, "获取版本信息失败");
        return false;
    }
    
    String payload = http.getString();
    http.end();
    
    Serial0.printf("[OTA] 响应长度: %d\n", payload.length());
    Serial0.printf("[OTA] 响应内容(前200字符): %s\n", payload.substring(0, min((int)payload.length(), 200)).c_str());
    
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial0.printf("[OTA] JSON解析错误: %s\n", error.c_str());
        setStatus(OTA_FAILED, "解析版本信息失败");
        return false;
    }
    
    const char* tagName = doc["tag_name"];
    if (tagName != nullptr) {
        strncpy(version, tagName, versionMaxLen - 1);
    }
    
    JsonArray assets = doc["assets"];
    if (assets.isNull()) {
        setStatus(OTA_FAILED, "未找到发布资产");
        return false;
    }
    
    const char* zipUrl = nullptr;
    const char* binUrl = nullptr;
    const char* fallbackUrl = nullptr;
    
    for (JsonObject asset : assets) {
        const char* name = asset["name"];
        const char* url = asset["browser_download_url"];
        
        if (name != nullptr && url != nullptr) {
            if (assetName != nullptr) {
                if (strcmp(name, assetName) == 0) {
                    strncpy(downloadUrl, url, maxLen - 1);
                    Serial0.printf("[OTA] 找到资产: %s\n", name);
                    return true;
                }
            } else {
                String nameStr = String(name);
                if (nameStr.endsWith(".bin")) {
                    binUrl = url;
                } else if (nameStr.endsWith(".zip")) {
                    zipUrl = url;
                } else if (fallbackUrl == nullptr) {
                    fallbackUrl = url;
                }
            }
        }
    }
    
    if (assetName != nullptr) {
        setStatus(OTA_FAILED, "未找到匹配的资产文件");
        return false;
    }
    
    if (binUrl != nullptr) {
        strncpy(downloadUrl, binUrl, maxLen - 1);
        Serial0.println("[OTA] 找到.bin固件文件");
        return true;
    } else if (zipUrl != nullptr) {
        strncpy(downloadUrl, zipUrl, maxLen - 1);
        Serial0.println("[OTA] 找到.zip固件文件");
        return true;
    } else if (fallbackUrl != nullptr) {
        strncpy(downloadUrl, fallbackUrl, maxLen - 1);
        Serial0.println("[OTA] 使用备用资产文件");
        return true;
    }
    
    setStatus(OTA_FAILED, "未找到可用的资产文件");
    return false;
}

bool OtaManager::downloadAndUpdate(const char* url) {
    if (_updateInProgress) {
        setStatus(OTA_FAILED, "升级正在进行中");
        return false;
    }
    
    if (url == nullptr || strlen(url) == 0) {
        setStatus(OTA_FAILED, "无效的升级URL");
        return false;
    }
    
    _updateInProgress = true;
    _progress = 0;
    
    setStatus(OTA_DOWNLOADING, "开始下载固件...");
    
    Serial0.printf("[OTA] 开始升级，URL: %s\n", url);
    
    const int maxRetries = 3;
    for (int retry = 0; retry < maxRetries; retry++) {
        Serial0.printf("[OTA] 下载尝试: %d/%d\n", retry + 1, maxRetries);
        Serial0.printf("[OTA] 可用内存: %d 字节\n", ESP.getFreeHeap());
        
        String currentUrl = url;
        HTTPClient http;
        WiFiClientSecure* secureClient = nullptr;
        int httpCode = 0;
        
        for (int redirectCount = 0; redirectCount < 5; redirectCount++) {
            secureClient = new WiFiClientSecure();
            secureClient->setInsecure();
            
            Serial0.printf("[OTA] HTTP连接前内存: %d 字节\n", ESP.getFreeHeap());
            
            http.begin(*secureClient, currentUrl);
            http.setConnectTimeout(15000);
            http.setTimeout(30000);
            const char* headerKeys[] = {"Location"};
            http.collectHeaders(headerKeys, 1);
            
            httpCode = http.GET();
            Serial0.printf("[OTA] HTTP状态码: %d (重定向次数: %d)\n", httpCode, redirectCount);
            
            if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND) {
                String redirectUrl = http.header("Location");
                Serial0.printf("[OTA] 重定向到: %s\n", redirectUrl.c_str());
                http.end();
                delete secureClient;
                secureClient = nullptr;
                
                if (redirectUrl.isEmpty()) {
                    Serial0.println("[OTA] 重定向地址为空");
                    break;
                }
                currentUrl = redirectUrl;
                continue;
            }
            
            break;
        }
        
        if (httpCode != HTTP_CODE_OK) {
            Serial0.printf("[OTA] HTTP下载失败，状态码: %d\n", httpCode);
            if (secureClient != nullptr) {
                http.end();
                delete secureClient;
                secureClient = nullptr;
            }
            if (retry < maxRetries - 1) {
                Serial0.printf("[OTA] %d秒后重试...\n", (retry + 1) * 5);
                for (int i = 0; i < (retry + 1) * 5; i++) {
                    delay(1000);
                    yield();
                }
                continue;
            }
            setStatus(OTA_FAILED, "固件下载失败");
            _updateInProgress = false;
            return false;
        }
        
        int contentLength = http.getSize();
        Serial0.printf("[OTA] 固件大小: %d 字节\n", contentLength);
        
        WiFiClient* stream = http.getStreamPtr();
        
        if (!Update.begin(contentLength, U_FLASH)) {
            Serial0.printf("[OTA] Update.begin失败: %s\n", Update.errorString());
            http.end();
            delete secureClient;
            secureClient = nullptr;
            if (retry < maxRetries - 1) {
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
        
        Update.setMD5(http.header("X-MD5").c_str());
        
        Serial0.println("[OTA] 开始写入固件...");
        size_t written = Update.writeStream(*stream);
        
        http.end();
        delete secureClient;
        secureClient = nullptr;
        
        if (written != contentLength) {
            Serial0.printf("[OTA] 写入字节数: %d, 预期: %d\n", written, contentLength);
            Serial0.printf("[OTA] Update.writeStream失败: %s\n", Update.errorString());
            Update.end();
            if (retry < maxRetries - 1) {
                for (int i = 0; i < 3; i++) {
                    delay(1000);
                    yield();
                }
                continue;
            }
            setStatus(OTA_FAILED, "固件写入失败");
            _updateInProgress = false;
            return false;
        }
        
        if (!Update.end(true)) {
            Serial0.printf("[OTA] Update.end失败: %s\n", Update.errorString());
            if (retry < maxRetries - 1) {
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
        
        Serial0.println("[OTA] 升级成功！");
        delay(500);
        ESP.restart();
    }
    
    setStatus(OTA_FAILED, "固件下载失败");
    _updateInProgress = false;
    return false;
}

void OtaManager::abortUpdate() {
    if (_updateInProgress) {
        _updateInProgress = false;
        setStatus(OTA_FAILED, "升级已取消");
        Serial0.println("[OTA] 升级已取消");
    }
}

void OtaManager::update() {
    if (_status == OTA_COMPLETED) {
        Serial0.println("[OTA] 升级完成，即将重启...");
        delay(1000);
        ESP.restart();
    }
}

const char* OtaManager::getCurrentVersion() {
    if (_currentVersion[0] == '\0') {
        return "unknown";
    }
    return _currentVersion;
}

void OtaManager::setCurrentVersion(const char* version) {
    if (version != nullptr) {
        strncpy(_currentVersion, version, sizeof(_currentVersion) - 1);
    }
}

void OtaManager::setStatus(OtaStatus status, const char* message) {
    _status = status;
    if (message != nullptr) {
        strncpy(_statusMessage, message, sizeof(_statusMessage) - 1);
    }
    
    Serial0.printf("[OTA] 状态: %d, 消息: %s\n", status, _statusMessage);
    
    if (_statusCallback != nullptr) {
        _statusCallback(status, _statusMessage);
    }
}

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