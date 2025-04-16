// include/GattCharacteristic.h
#pragma once

#include "SDBusInterface.h"
#include "SDBusObject.h"
#include "GattTypes.h"
#include "GattCallbacks.h"
#include "BlueZConstants.h"
#include <vector>
#include <map>
#include <memory>
#include <mutex>

namespace ggk {

// 전방 선언
class GattService;
class GattDescriptor;
using GattDescriptorPtr = std::shared_ptr<GattDescriptor>;

class GattCharacteristic {
public:
    GattCharacteristic(
        SDBusConnection& connection,
        const std::string& path,
        const GattUuid& uuid,
        GattService& service,
        uint8_t properties,
        uint8_t permissions);
    
    virtual ~GattCharacteristic() = default;
    
    // 기본 접근자
    const GattUuid& getUuid() const { return uuid; }
    const std::string& getPath() const { return object.getPath(); }
    uint8_t getProperties() const { return properties; }
    uint8_t getPermissions() const { return permissions; }
    
    // 값 관리
    const std::vector<uint8_t>& getValue() const {
        std::lock_guard<std::mutex> lock(valueMutex);
        return value;
    }
    
    void setValue(const std::vector<uint8_t>& value);
    void setValue(std::vector<uint8_t>&& value);
    
    // 설명자 관리
    GattDescriptorPtr createDescriptor(
        const GattUuid& uuid,
        uint8_t permissions);
    
    GattDescriptorPtr getDescriptor(const GattUuid& uuid) const;
    
    const std::map<std::string, GattDescriptorPtr>& getDescriptors() const {
        std::lock_guard<std::mutex> lock(descriptorsMutex);
        return descriptors;
    }
    
    // 알림 관리
    bool startNotify();
    bool stopNotify();
    bool isNotifying() const {
        std::lock_guard<std::mutex> lock(notifyMutex);
        return notifying;
    }
    
    // 콜백 설정
    void setReadCallback(GattReadCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        readCallback = callback;
    }
    
    void setWriteCallback(GattWriteCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        writeCallback = callback;
    }
    
    void setNotifyCallback(GattNotifyCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        notifyCallback = callback;
    }
    
    // D-Bus 인터페이스 설정
    bool setupDBusInterfaces();
    bool finishRegistration() { return object.registerObject(); }
    bool isRegistered() const { return object.isRegistered(); }
    
    // 부모 서비스 접근자
    GattService& getService() const { return service; }
    
private:
    // D-Bus 메서드 핸들러
    std::vector<uint8_t> handleReadValue(const std::map<std::string, sdbus::Variant>& options);
    void handleWriteValue(const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options);
    void handleStartNotify();
    void handleStopNotify();
    
    // 내부 상태
    SDBusConnection& connection;
    SDBusObject object;
    GattUuid uuid;
    GattService& service;
    uint8_t properties;
    uint8_t permissions;
    std::vector<uint8_t> value;
    mutable std::mutex valueMutex;
    
    bool notifying;
    mutable std::mutex notifyMutex;
    
    // 설명자 관리
    std::map<std::string, GattDescriptorPtr> descriptors;
    mutable std::mutex descriptorsMutex;
    
    // 콜백
    GattReadCallback readCallback;
    GattWriteCallback writeCallback;
    GattNotifyCallback notifyCallback;
    mutable std::mutex callbackMutex;
};

using GattCharacteristicPtr = std::shared_ptr<GattCharacteristic>;

} // namespace ggk