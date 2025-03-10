// GattDescriptor.h
#pragma once

#include "GattTypes.h"
#include "GattCallbacks.h"
#include "DBusObject.h"
#include <vector>
#include <memory>

namespace ggk {

// 전방 선언 - 필요한 것만 선언
class GattCharacteristic;

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
    const std::vector<uint8_t>& getValue() const { return value; }
    uint8_t getPermissions() const { return permissions; }
    
    // 값 설정/획득
    void setValue(const std::vector<uint8_t>& value);
    
    // 콜백 설정
    void setReadCallback(GattReadCallback callback) { readCallback = callback; }
    void setWriteCallback(GattWriteCallback callback) { writeCallback = callback; }
    
    // BlueZ D-Bus 인터페이스 설정
    bool setupDBusInterfaces();
    
    
private:
    // 상수
    static constexpr const char* DESCRIPTOR_INTERFACE = "org.bluez.GattDescriptor1";
    
    // 속성
    GattUuid uuid;
    GattCharacteristic& characteristic;
    uint8_t permissions;
    std::vector<uint8_t> value;
    
    // 콜백
    GattReadCallback readCallback;
    GattWriteCallback writeCallback;
    
    // D-Bus 메서드 핸들러 - const 참조로 수정
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