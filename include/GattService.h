// GattService.h
#pragma once

#include "GattTypes.h"
#include "DBusObject.h"
#include <vector>
#include <map>
#include <memory>

namespace ggk {

// 전방 선언
class GattCharacteristic;

// 이 파일에서 사용하는 포인터 타입
using GattCharacteristicPtr = std::shared_ptr<GattCharacteristic>;

class GattService : public DBusObject, public std::enable_shared_from_this<GattService> {
public:
    // 생성자
    GattService(
        DBusConnection& connection,
        const DBusObjectPath& path,
        const GattUuid& uuid,
        bool isPrimary
    );
    
    virtual ~GattService() = default;
    
    // 속성 접근자
    const GattUuid& getUuid() const { return uuid; }
    bool isPrimary() const { return primary; }
    
    // 특성 관리
    GattCharacteristicPtr createCharacteristic(
        const GattUuid& uuid,
        uint8_t properties,
        uint8_t permissions
    );
    
    GattCharacteristicPtr getCharacteristic(const GattUuid& uuid) const;
    const std::map<std::string, GattCharacteristicPtr>& getCharacteristics() const { return characteristics; }
    
    // BlueZ D-Bus 인터페이스 설정
    bool setupDBusInterfaces();
    
    
private:
    // 상수
    static constexpr const char* SERVICE_INTERFACE = "org.bluez.GattService1";
    
    // 속성
    GattUuid uuid;
    bool primary;
    
    // 특성 관리
    std::map<std::string, GattCharacteristicPtr> characteristics;
    
    // D-Bus 프로퍼티 획득
    GVariant* getUuidProperty();
    GVariant* getPrimaryProperty();
    GVariant* getCharacteristicsProperty();
};

// 스마트 포인터 정의 - 각 헤더에서 자체적으로 정의
using GattServicePtr = std::shared_ptr<GattService>;

} // namespace ggk