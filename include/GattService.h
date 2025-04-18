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
    
    // IGattNode 인터페이스 구현
    const GattUuid& getUuid() const override { return uuid; }
    const std::string& getPath() const override { return object.getPath(); }
    bool setupInterfaces() override;
    bool isInterfaceSetup() const override { return interfaceSetup; }
    
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
    
    // D-Bus 객체 등록 관리
    bool registerObject();
    bool unregisterObject();
    bool isRegistered() const { return objectRegistered; }
    
    // 신호 발생 도우미
    void emitInterfacesAddedForCharacteristic(GattCharacteristicPtr characteristic);
    void emitInterfacesRemovedForCharacteristic(GattCharacteristicPtr characteristic);
    
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
    bool interfaceSetup;
    bool objectRegistered;
    
    // 특성 관리
    std::map<std::string, GattCharacteristicPtr> characteristics;
    mutable std::mutex characteristicsMutex;
};

using GattServicePtr = std::shared_ptr<GattService>;
}