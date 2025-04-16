// include/GattAdvertisement.h
#pragma once

#include "SDBusInterface.h"
#include "SDBusObject.h"
#include "BlueZConstants.h"
#include "GattTypes.h"
#include <vector>
#include <map>
#include <string>

namespace ggk {

class GattAdvertisement {
public:
    enum class AdvertisementType {
        BROADCAST,
        PERIPHERAL
    };
    
    GattAdvertisement(
        SDBusConnection& connection,
        const std::string& path,
        AdvertisementType type = AdvertisementType::PERIPHERAL);
    
    virtual ~GattAdvertisement() = default;
    
    // 광고 데이터 설정
    void addServiceUUID(const GattUuid& uuid);
    void addServiceUUIDs(const std::vector<GattUuid>& uuids);
    void setManufacturerData(uint16_t manufacturerId, const std::vector<uint8_t>& data);
    void setServiceData(const GattUuid& serviceUuid, const std::vector<uint8_t>& data);
    void setLocalName(const std::string& name);
    void setDiscoverable(bool discoverable);
    void setAppearance(uint16_t appearance);
    void setDuration(uint16_t duration);
    void setIncludeTxPower(bool include);
    void addInclude(const std::string& item);
    void setIncludes(const std::vector<std::string>& items);
    
    // BlueZ 등록
    bool registerWithBlueZ();
    bool unregisterFromBlueZ();
    bool isRegisteredWithBlueZ() const { return registered; }
    
    // D-Bus 인터페이스 설정
    bool setupDBusInterfaces();
    
    // Release 메서드 처리
    void handleRelease();
    
    // BlueZ 5.82 호환성 확보
    void ensureBlueZ582Compatibility();
    
    // 속성 접근자
    const std::vector<std::string>& getIncludes() const { return includes; }
    uint16_t getAppearance() const { return appearance; }
    const std::string& getLocalName() const { return localName; }
    bool getIncludeTxPower() const { return includeTxPower; }
    const std::string& getPath() const { return object.getPath(); }
    
private:
    // D-Bus 속성 게터
    std::string getTypeProperty() const;
    std::vector<std::string> getServiceUUIDsProperty() const;
    std::map<uint16_t, sdbus::Variant> getManufacturerDataProperty() const;
    std::map<std::string, sdbus::Variant> getServiceDataProperty() const;
    std::string getLocalNameProperty() const;
    uint16_t getAppearanceProperty() const;
    uint16_t getDurationProperty() const;
    bool getIncludeTxPowerProperty() const;
    bool getDiscoverableProperty() const;
    std::vector<std::string> getIncludesProperty() const;
    
    // 내부 상태
    SDBusConnection& connection;
    SDBusObject object;
    AdvertisementType type;
    std::vector<GattUuid> serviceUUIDs;
    std::map<uint16_t, std::vector<uint8_t>> manufacturerData;
    std::map<GattUuid, std::vector<uint8_t>> serviceData;
    std::string localName;
    uint16_t appearance;
    uint16_t duration;
    bool includeTxPower;
    bool discoverable;
    std::vector<std::string> includes;
    bool registered;
    
    // 대체 광고 방법
    bool tryAlternativeAdvertisingMethods();
};

} // namespace ggk