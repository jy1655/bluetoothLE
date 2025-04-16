// include/GattService.h 수정
#pragma once

#include "SDBusInterface.h"
#include "SDBusObject.h"
#include "GattTypes.h"
#include "BlueZConstants.h"
#include <map>
#include <memory>
#include <mutex>

namespace ggk {

// 전방 선언
class GattCharacteristic;
using GattCharacteristicPtr = std::shared_ptr<GattCharacteristic>;

class GattService {
public:
    GattService(SDBusConnection& connection, 
               const std::string& path,
               const GattUuid& uuid,
               bool isPrimary);
    
    virtual ~GattService() = default;
    
    // 기본 접근자
    const GattUuid& getUuid() const { return uuid; }
    bool isPrimary() const { return primary; }
    const std::string& getPath() const { return object.getPath(); }
    
    // 특성 관리
    GattCharacteristicPtr createCharacteristic(
        const GattUuid& uuid,
        uint8_t properties,
        uint8_t permissions);
    
    GattCharacteristicPtr getCharacteristic(const GattUuid& uuid) const;
    
    const std::map<std::string, GattCharacteristicPtr>& getCharacteristics() const {
        std::lock_guard<std::mutex> lock(characteristicsMutex);
        return characteristics;
    }
    
    // D-Bus 인터페이스 설정
    bool setupDBusInterfaces();
    bool finishRegistration() { return object.registerObject(); }
    bool isRegistered() const { return object.isRegistered(); }
    
private:
    // D-Bus 속성 게터
    std::string getUuidProperty() const;
    bool getPrimaryProperty() const;
    std::vector<sdbus::ObjectPath> getCharacteristicsProperty() const;
    
    // 내부 상태
    SDBusConnection& connection;
    SDBusObject object;
    GattUuid uuid;
    bool primary;
    
    // 특성 관리
    std::map<std::string, GattCharacteristicPtr> characteristics;
    mutable std::mutex characteristicsMutex;
};

using GattServicePtr = std::shared_ptr<GattService>;

} // namespace ggk