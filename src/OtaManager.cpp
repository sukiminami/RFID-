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
    
    WiFiClient client;
    t_httpUpdate_return result = httpUpdate.update(client, String(url), String(getCurrentVersion()));
    
    handleUpdateResult(result);
    
    return result == HTTP_UPDATE_OK;
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