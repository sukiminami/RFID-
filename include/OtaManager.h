#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

typedef enum {
    OTA_IDLE,
    OTA_CHECKING,
    OTA_DOWNLOADING,
    OTA_UPDATING,
    OTA_COMPLETED,
    OTA_FAILED
} OtaStatus;

typedef void (*OtaProgressCallback)(int progress);
typedef void (*OtaStatusCallback)(OtaStatus status, const char* message);

class OtaManager {
public:
    OtaManager();
    
    bool begin();
    
    void setProgressCallback(OtaProgressCallback callback);
    
    void setStatusCallback(OtaStatusCallback callback);
    
    OtaStatus getStatus();
    
    const char* getStatusMessage();
    
    int getProgress();
    
    bool startUpdate(const char* url);
    
    void abortUpdate();
    
    void update();
    
    const char* getCurrentVersion();
    
    void setCurrentVersion(const char* version);
    
private:
    OtaStatus _status;
    char _statusMessage[128];
    int _progress;
    bool _updateInProgress;
    
    char _currentVersion[32];
    
    OtaProgressCallback _progressCallback;
    OtaStatusCallback _statusCallback;
    
    void setStatus(OtaStatus status, const char* message);
    
    void handleUpdateResult(t_httpUpdate_return result);
};

#endif