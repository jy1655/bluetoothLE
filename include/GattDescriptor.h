// GattDescriptor.h
#pragma once

#include "GattTypes.h"
#include "GattCallbacks.h"
#include "DBusObject.h"
#include "GattCharacteristic.h" 
#include "BlueZConstants.h"
#include <vector>
#include <memory>
#include <mutex>

namespace ggk {


class GattDescriptor : public DBusObject, public std::enable_shared_from_this<GattDescriptor> {
public:
    // 생성자
    GattDescriptor(
        DBusConnection& connection,
        const DBusObjectPath& path,
        const GattUuid& uuid,
        GattCharacteristic& characteristic,
        uint8_t permissions
    );
    
    virtual ~GattDescriptor() = default;
    
    // 속성 접근자
    const GattUuid& getUuid() const { return uuid; }
    
    const std::vector<uint8_t>& getValue() const {
        std::lock_guard<std::mutex> lock(valueMutex);
        return value;
    }
    
    uint8_t getPermissions() const { return permissions; }
    
    // 값 설정/획득
    void setValue(const std::vector<uint8_t>& value);
    
    // 콜백 설정
    void setReadCallback(GattReadCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        readCallback = callback;
    }
    
    void setWriteCallback(GattWriteCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        writeCallback = callback;
    }
    
    // BlueZ D-Bus 인터페이스 설정
    bool setupDBusInterfaces();

    GattCharacteristic& getCharacteristic() const { return characteristic; }
    
//private:
    
    // 속성
    GattUuid uuid;
    GattCharacteristic& characteristic;
    uint8_t permissions;
    std::vector<uint8_t> value;
    mutable std::mutex valueMutex;
    
    // 콜백
    GattReadCallback readCallback;
    GattWriteCallback writeCallback;
    mutable std::mutex callbackMutex;
    
    // D-Bus 메서드 핸들러
    void handleReadValue(const DBusMethodCall& call);
    void handleWriteValue(const DBusMethodCall& call);
    
    // D-Bus 프로퍼티 획득
    GVariant* getUuidProperty();
    GVariant* getCharacteristicProperty();
    GVariant* getPermissionsProperty();
};

// 스마트 포인터 정의 - 각 헤더에서 자체적으로 정의
using GattDescriptorPtr = std::shared_ptr<GattDescriptor>;

} // namespace ggk