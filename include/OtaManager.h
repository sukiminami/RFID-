/**
 * @file OtaManager.h
 * @brief OTA固件升级管理器头文件
 * @details 负责管理ESP32的固件升级过程，支持从HTTP URL和GitHub Releases下载升级固件，
 *          提供升级进度回调和状态回调，支持断点续传和自动重连。
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

// 前向声明
class NetworkManager;

// ==================== 枚举定义 ====================

/**
 * @enum OtaStatus
 * @brief OTA升级状态枚举
 */
typedef enum {
    OTA_IDLE,           /**< 空闲状态，未进行升级 */
    OTA_CHECKING,       /**< 正在检查更新版本 */
    OTA_DOWNLOADING,    /**< 正在下载固件 */
    OTA_UPDATING,       /**< 正在写入固件到Flash */
    OTA_COMPLETED,      /**< 升级完成，等待重启 */
    OTA_FAILED          /**< 升级失败 */
} OtaStatus;

// ==================== 回调函数类型 ====================

/**
 * @typedef OtaProgressCallback
 * @brief OTA下载进度回调函数类型
 * @param progress 下载进度百分比(0-100)
 * @param userData 用户数据指针
 */
typedef void (*OtaProgressCallback)(int progress, void* userData);

/**
 * @typedef OtaStatusCallback
 * @brief OTA状态变化回调函数类型
 * @param status 当前OTA状态
 * @param message 状态描述消息
 * @param userData 用户数据指针
 */
typedef void (*OtaStatusCallback)(OtaStatus status, const char* message, void* userData);

// ==================== 类定义 ====================

/**
 * @class OtaManager
 * @brief OTA固件升级管理器类
 * @details 封装ESP32 OTA升级功能，支持：
 *          1. 从GitHub Releases获取最新固件
 *          2. 从HTTP URL直接下载固件
 *          3. 下载进度实时回调
 *          4. 状态变化回调通知
 *          5. 支持HTTP 302重定向处理
 *          6. 下载失败自动重试机制
 *          7. 下载超时保护(30秒无数据)
 */
class OtaManager {
public:
    /**
     * @brief 构造函数
     * @details 初始化所有成员变量为默认值，清空状态消息和版本号
     */
    OtaManager();
    
    /**
     * @brief 初始化OTA管理器
     * @return true-初始化成功，false-初始化失败
     * @details 注册HTTPUpdate进度回调，初始化单例指针
     */
    bool begin();
    
    /**
     * @brief 设置下载进度回调函数
     * @param callback 进度回调函数指针
     * @param userData 用户数据指针(默认nullptr)
     */
    void setProgressCallback(OtaProgressCallback callback, void* userData = nullptr);
    
    /**
     * @brief 设置状态变化回调函数
     * @param callback 状态回调函数指针
     * @param userData 用户数据指针(默认nullptr)
     */
    void setStatusCallback(OtaStatusCallback callback, void* userData = nullptr);
    
    /**
     * @brief 设置网络管理器指针
     * @param networkManager 网络管理器指针
     * @details 用于在下载过程中保持MQTT连接活跃
     */
    void setNetworkManager(NetworkManager* networkManager);
    
    /**
     * @brief 获取当前OTA状态
     * @return OtaStatus枚举值
     */
    OtaStatus getStatus();
    
    /**
     * @brief 获取当前状态消息
     * @return 状态描述字符串
     */
    const char* getStatusMessage();
    
    /**
     * @brief 获取当前下载进度
     * @return 进度百分比(0-100)
     */
    int getProgress();
    
    /**
     * @brief 从指定URL开始升级
     * @param url 固件下载URL
     * @return true-升级启动成功，false-启动失败
     * @details 直接调用downloadAndUpdate执行升级
     */
    bool startUpdate(const char* url);
    
    /**
     * @brief 从GitHub Releases升级
     * @param repo GitHub仓库地址(格式: owner/repo)
     * @param assetName 资产文件名(默认nullptr，自动查找.bin文件)
     * @return true-升级启动成功，false-启动失败
     * @details 先调用getLatestReleaseAsset获取下载URL，再调用downloadAndUpdate
     */
    bool updateFromGithub(const char* repo, const char* assetName = nullptr);
    
    /**
     * @brief 检查GitHub Releases是否有新版本
     * @param repo GitHub仓库地址(格式: owner/repo)
     * @param latestVersion 输出参数，存储最新版本号
     * @param maxLen latestVersion缓冲区最大长度
     * @return true-有新版本，false-无新版本或检查失败
     */
    bool checkUpdate(const char* repo, char* latestVersion, int maxLen);
    
    /**
     * @brief 取消正在进行的升级
     * @details 设置状态为OTA_FAILED，停止升级进程
     */
    void abortUpdate();
    
    /**
     * @brief 周期性更新函数(需在loop中调用)
     * @details 检查升级完成状态，若完成则重启设备
     */
    void update();
    
    /**
     * @brief 获取当前固件版本号
     * @return 当前版本号字符串，若未设置返回"unknown"
     */
    const char* getCurrentVersion();
    
    /**
     * @brief 设置当前固件版本号
     * @param version 版本号字符串
     */
    void setCurrentVersion(const char* version);
    
private:
    /**
     * @var _status
     * @brief 当前OTA状态
     */
    OtaStatus _status;
    
    /**
     * @var _statusMessage
     * @brief 当前状态描述消息
     */
    char _statusMessage[128];
    
    /**
     * @var _progress
     * @brief 当前下载进度(0-100)
     */
    int _progress;
    
    /**
     * @var _updateInProgress
     * @brief 是否正在进行升级
     */
    bool _updateInProgress;
    
    /**
     * @var _currentVersion
     * @brief 当前固件版本号
     */
    char _currentVersion[32];
    
    /**
     * @var _progressCallback
     * @brief 下载进度回调函数
     */
    OtaProgressCallback _progressCallback;
    
    /**
     * @var _statusCallback
     * @brief 状态变化回调函数
     */
    OtaStatusCallback _statusCallback;
    
    /**
     * @var _networkManager
     * @brief 网络管理器指针，用于保持MQTT连接
     */
    NetworkManager* _networkManager;
    
    /**
     * @var _progressUserData
     * @brief 进度回调用户数据指针
     */
    void* _progressUserData;
    
    /**
     * @var _statusUserData
     * @brief 状态回调用户数据指针
     */
    void* _statusUserData;
    
    /**
     * @brief 设置OTA状态和消息
     * @param status 新状态
     * @param message 状态描述消息
     * @details 更新内部状态并触发状态回调
     */
    void setStatus(OtaStatus status, const char* message);
    
    /**
     * @brief 处理HTTPUpdate返回结果
     * @param result HTTPUpdate返回值
     * @details 根据结果设置对应的OTA状态
     */
    void handleUpdateResult(t_httpUpdate_return result);
    
    /**
     * @brief 下载固件并执行升级(核心函数)
     * @param url 固件下载URL
     * @return true-升级成功(会重启)，false-升级失败
     * @details 处理HTTP重定向、内存分配、分段下载、写入Flash、验证固件
     */
    bool downloadAndUpdate(const char* url);
    
    /**
     * @brief 获取GitHub Releases最新资产下载地址
     * @param repo GitHub仓库地址(格式: owner/repo)
     * @param assetName 指定的资产文件名(可为nullptr)
     * @param downloadUrl 输出参数，存储下载URL
     * @param maxLen downloadUrl缓冲区最大长度
     * @param version 输出参数，存储版本号
     * @param versionMaxLen version缓冲区最大长度
     * @return true-获取成功，false-获取失败
     * @details 调用GitHub API获取最新发布信息，解析资产列表
     */
    bool getLatestReleaseAsset(const char* repo, const char* assetName, char* downloadUrl, int maxLen, char* version, int versionMaxLen);
};

#endif