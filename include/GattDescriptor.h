// include/GattDescriptor.h
#pragma once

#include "GattTypes.h"
#include "GattCallbacks.h"
#include "SDBusObject.h" 
#include "BlueZConstants.h"
#include <vector>
#include <memory>
#include <mutex>

namespace ggk {

// 전방 선언 (순환 의존성 방지)
class GattCharacteristic;

class GattDescriptor {
public:
    // 생성자
    GattDescriptor(
        SDBusConnection& connection,  // DBusConnection 대신 SDBusConnection 사용
        const std::string& path,
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
    const std::string& getPath() const { return object.getPath(); }
    
    // 값 설정
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
    bool isRegistered() const { return object.isRegistered(); }

    // 부모 특성 접근자
    GattCharacteristic& getCharacteristic() const { return characteristic; }
    
private:
    // 내부 상태
    SDBusConnection& connection;
    SDBusObject object;
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
    std::vector<uint8_t> handleReadValue(const std::map<std::string, sdbus::Variant>& options);
    void handleWriteValue(const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options);
};

// 공유 포인터 타입 정의
using GattDescriptorPtr = std::shared_ptr<GattDescriptor>;

} // namespace ggk