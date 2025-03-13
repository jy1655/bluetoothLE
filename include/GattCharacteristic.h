// GattCharacteristic.h
#pragma once

#include "GattTypes.h"
#include "GattCallbacks.h"
#include "DBusObject.h"
#include "GattService.h" 
#include "BlueZConstants.h"
#include <vector>
#include <map>
#include <memory>
#include <mutex>

namespace ggk {

// 전방 선언 - 필요한 것만
class GattDescriptor;

// 파일 내에서만 사용하는 포인터 타입
using GattDescriptorPtr = std::shared_ptr<GattDescriptor>;

class GattCharacteristic : public DBusObject, public std::enable_shared_from_this<GattCharacteristic> {
public:
    // 생성자
    GattCharacteristic(
        DBusConnection& connection,
        const DBusObjectPath& path,
        const GattUuid& uuid,
        GattService& service,
        uint8_t properties,
        uint8_t permissions
    );
    
    virtual ~GattCharacteristic() = default;
    
    // 속성 접근자
    const GattUuid& getUuid() const { return uuid; }
    
    const std::vector<uint8_t>& getValue() const {
        std::lock_guard<std::mutex> lock(valueMutex);
        return value;
    }
    
    uint8_t getProperties() const { return properties; }
    uint8_t getPermissions() const { return permissions; }
    
    // 값 설정
    void setValue(const std::vector<uint8_t>& value);
    
    // 설명자 관리
    GattDescriptorPtr createDescriptor(
        const GattUuid& uuid,
        uint8_t permissions
    );
    
    GattDescriptorPtr getDescriptor(const GattUuid& uuid) const;
    
    const std::map<std::string, GattDescriptorPtr>& getDescriptors() const {
        std::lock_guard<std::mutex> lock(descriptorsMutex);
        return descriptors;
    }
    
    // 알림(Notification) 관리
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
    
    // BlueZ D-Bus 인터페이스 설정
    bool setupDBusInterfaces();

    GattService& getService() const { return service; }
    
//private:
    
    // 속성
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
    
    // D-Bus 메서드 핸들러
    void handleReadValue(const DBusMethodCall& call);
    void handleWriteValue(const DBusMethodCall& call);
    void handleStartNotify(const DBusMethodCall& call);
    void handleStopNotify(const DBusMethodCall& call);
    
    // D-Bus 프로퍼티 획득
    GVariant* getUuidProperty();
    GVariant* getServiceProperty();
    GVariant* getPropertiesProperty();
    GVariant* getDescriptorsProperty();
    GVariant* getNotifyingProperty();
};

// 스마트 포인터 정의 - 각 헤더에서 자체적으로 정의
using GattCharacteristicPtr = std::shared_ptr<GattCharacteristic>;

} // namespace ggk