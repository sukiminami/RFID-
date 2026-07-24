/**
 * @file NetworkInterface.cpp
 * @brief 统一网络连接接口实现
 * @details 实现基类构造函数、析构函数、回调注册和状态触发机制。
 */

#include "NetworkInterface.h"

/**
 * @brief 构造函数
 * @details 初始化状态为断开，回调为空，重试计数为 0
 */
NetworkInterface::NetworkInterface() {
    _status = NETWORK_DISCONNECTED;
    _dataCallback = nullptr;
    _statusCallback = nullptr;
    _userData = nullptr;
    _lastUpdateTime = 0;
    _retryCount = 0;
}

/**
 * @brief 虚析构函数
 */
NetworkInterface::~NetworkInterface() {
}

/**
 * @brief 注册数据接收回调
 * @param callback 回调函数指针
 * @param userData 用户数据指针（传递给回调函数）
 */
void NetworkInterface::setDataCallback(NetworkDataCallback callback, void* userData) {
    _dataCallback = callback;
    _userData = userData;
}

/**
 * @brief 注册状态变化回调
 * @param callback 回调函数指针
 * @param userData 用户数据指针（传递给回调函数）
 */
void NetworkInterface::setStatusCallback(NetworkStatusCallback callback, void* userData) {
    _statusCallback = callback;
    _userData = userData;
}

/**
 * @brief 触发状态变化回调
 * @param status 网络状态
 * @param message 状态信息
 * @details 更新内部 _status 成员，若用户已注册回调则调用之
 */
void NetworkInterface::triggerStatusCallback(NetworkStatus status, const char* message) {
    _status = status;
    if (_statusCallback != nullptr) {
        _statusCallback(status, getType(), message, _userData);
    }
}
