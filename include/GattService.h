// 4. GattService.h - 수정된 서비스 클래스
#pragma once

#include "IGattNode.h"
#include "GattCharacteristic.h"
#include "SDBusObject.h"
#include "BlueZConstants.h"
#include <map>
#include <memory>
#include <mutex>

namespace ggk {

/**
 * @brief GATT 서비스 구현
 */
class GattService : public IGattNode {
public:
    GattService(
        SDBusConnection& connection, 
        const std::string& path,
        const GattUuid& uuid,
        bool isPrimary);
    
    virtual ~GattService() = default;
    
    // IGattNode 구현
    const GattUuid& getUuid() const override { return uuid; }
    const std::string& getPath() const override { return object.getPath(); }
    bool setupDBusInterfaces() override;
    bool isRegistered() const override { return object.isRegistered(); }
    
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
    
    // 속성 접근자
    bool isPrimary() const { return primary; }
    
    // 추가 메서드
    bool finishRegistration() { return object.registerObject(); }
    
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