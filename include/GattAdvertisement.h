#pragma once

#include "DBusObject.h"
#include "BlueZConstants.h"
#include "GattTypes.h"
#include "DBusName.h"
#include <vector>
#include <map>
#include <string>

namespace ggk {

class GattAdvertisement : public DBusObject {
public:
    enum class AdvertisementType {
        BROADCAST,
        PERIPHERAL
    };
    
    GattAdvertisement(
        DBusConnection& connection,
        const DBusObjectPath& path,
        AdvertisementType type = AdvertisementType::PERIPHERAL
    );

    // DBusName 사용하는 새 생성자 추가
    GattAdvertisement(const DBusObjectPath& path);
    
    virtual ~GattAdvertisement() = default;
    
    // 서비스 UUID 설정
    void addServiceUUID(const GattUuid& uuid);
    void addServiceUUIDs(const std::vector<GattUuid>& uuids);
    
    // 제조사 데이터 설정 (Company ID + 임의 데이터)
    void setManufacturerData(uint16_t manufacturerId, const std::vector<uint8_t>& data);
    
    // 서비스 데이터 설정 (Service UUID + 임의 데이터)
    void setServiceData(const GattUuid& serviceUuid, const std::vector<uint8_t>& data);
    
    // 기타 광고 속성 설정
    void setLocalName(const std::string& name);
    void setAppearance(uint16_t appearance);
    void setDuration(uint16_t duration);
    void setIncludeTxPower(bool include);
    
    // BlueZ 등록
    bool registerWithBlueZ();
    bool unregisterFromBlueZ();
    bool isRegistered() const { return registered; }

    // D-Bus 인터페이스 설정
    bool setupDBusInterfaces();
    
    // D-Bus 메서드 핸들러
    void handleRelease(const DBusMethodCall& call);

private:
    
    
    // D-Bus 속성 획득
    GVariant* getTypeProperty();
    GVariant* getServiceUUIDsProperty();
    GVariant* getManufacturerDataProperty();
    GVariant* getServiceDataProperty();
    GVariant* getLocalNameProperty();
    GVariant* getAppearanceProperty();
    GVariant* getDurationProperty();
    GVariant* getIncludeTxPowerProperty();
    
    // 속성
    AdvertisementType type;
    std::vector<GattUuid> serviceUUIDs;
    std::map<uint16_t, std::vector<uint8_t>> manufacturerData;
    std::map<GattUuid, std::vector<uint8_t>> serviceData;
    std::string localName;
    uint16_t appearance;
    uint16_t duration;
    bool includeTxPower;
    bool registered;
};

} // namespace ggk