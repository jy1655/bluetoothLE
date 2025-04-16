#pragma once

#include "DBusObject.h"
#include "BlueZConstants.h"
#include "GattTypes.h"
#include "DBusName.h"
#include <vector>
#include <map>
#include <string>

namespace ggk {

/**
 * @brief BLE 광고 관리 클래스
 * 
 * BlueZ 5.82 호환 버전의 BLE 광고 관리 클래스입니다.
 */
class GattAdvertisement : public DBusObject {
public:
    /**
     * @brief 광고 타입 열거형
     */
    enum class AdvertisementType {
        BROADCAST,
        PERIPHERAL
    };
    
    /**
     * @brief 생성자
     * 
     * @param connection D-Bus 연결
     * @param path 객체 경로
     * @param type 광고 타입 (기본값: PERIPHERAL)
     */
    GattAdvertisement(
        std::shared_ptr<IDBusConnection> connection,
        const DBusObjectPath& path,
        AdvertisementType type = AdvertisementType::PERIPHERAL
    );

    /**
     * @brief DBusName 싱글톤을 사용하는 생성자
     * 
     * @param path 객체 경로
     */
    GattAdvertisement(const DBusObjectPath& path);
    
    /**
     * @brief 소멸자
     */
    virtual ~GattAdvertisement() = default;
    
    /**
     * @brief 서비스 UUID 추가
     * 
     * @param uuid 서비스 UUID
     */
    void addServiceUUID(const GattUuid& uuid);
    
    /**
     * @brief 여러 서비스 UUID 추가
     * 
     * @param uuids 서비스 UUID 목록
     */
    void addServiceUUIDs(const std::vector<GattUuid>& uuids);
    
    /**
     * @brief 제조사 데이터 설정
     * 
     * @param manufacturerId 제조사 ID
     * @param data 제조사별 데이터
     */
    void setManufacturerData(uint16_t manufacturerId, const std::vector<uint8_t>& data);
    
    /**
     * @brief 서비스 데이터 설정
     * 
     * @param serviceUuid 서비스 UUID
     * @param data 서비스별 데이터
     */
    void setServiceData(const GattUuid& serviceUuid, const std::vector<uint8_t>& data);
    
    /**
     * @brief 로컬 이름 설정
     * 
     * @param name 로컬 이름
     */
    void setLocalName(const std::string& name);
    
    /**
     * @brief 조회 가능 여부 설정
     * 
     * @param discoverable 조회 가능 여부
     */
    void setDiscoverable(bool discoverable);
    
    /**
     * @brief 외관 코드 설정
     * 
     * @param appearance 외관 코드
     */
    void setAppearance(uint16_t appearance);
    
    /**
     * @brief 광고 지속 시간 설정
     * 
     * @param duration 지속 시간(초)
     */
    void setDuration(uint16_t duration);
    
    /**
     * @brief TX 파워 포함 여부 설정
     * 
     * @param include TX 파워 포함 여부
     */
    void setIncludeTxPower(bool include);

    /**
     * @brief 광고에 포함할 항목 추가 (BlueZ 5.82+)
     * 
     * @param item 포함할 항목 ("tx-power", "appearance", "local-name", "service-uuids")
     */
    void addInclude(const std::string& item);
    
    /**
     * @brief 포함할 항목 목록 설정 (BlueZ 5.82+)
     * 
     * @param items 포함할 항목 목록
     */
    void setIncludes(const std::vector<std::string>& items);
    
    /**
     * @brief BlueZ에 광고 등록
     * 
     * @return 성공 여부
     */
    bool registerWithBlueZ();
    
    /**
     * @brief BlueZ에서 광고 등록 해제
     * 
     * @return 성공 여부
     */
    bool unregisterFromBlueZ();
    
    /**
     * @brief BlueZ 등록 상태 확인
     */
    bool isRegisteredWithBlueZ() const { return registered; }

    /**
     * @brief D-Bus 인터페이스 설정
     * 
     * @return 성공 여부
     */
    bool setupDBusInterfaces();
    
    /**
     * @brief Release 메서드 처리
     * 
     * @param call 메서드 호출 정보
     */
    void handleRelease(const DBusMethodCall& call);
    
    /**
     * @brief BlueZ 5.82 호환성 확보
     * 
     * BlueZ 5.82에서 요구하는 필수 속성이 제대로 설정되었는지 확인하고 추가합니다.
     */
    void ensureBlueZ582Compatibility();
    
    // 속성 접근자
    const std::vector<std::string>& getIncludes() const { return includes; }
    uint16_t getAppearance() const { return appearance; }
    const std::string& getLocalName() const { return localName; }
    bool getIncludeTxPower() const { return includeTxPower; }

private:
    // D-Bus 속성 취득 함수
    GVariant* getTypeProperty();
    GVariant* getServiceUUIDsProperty();
    GVariant* getManufacturerDataProperty();
    GVariant* getServiceDataProperty();
    GVariant* getLocalNameProperty();
    GVariant* getAppearanceProperty();
    GVariant* getDurationProperty();
    GVariant* getIncludeTxPowerProperty();
    GVariant* getDiscoverableProperty();
    GVariant* getIncludesProperty();
    
    // 속성
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
    bool registered;  // BlueZ 등록 상태
};

} // namespace ggk