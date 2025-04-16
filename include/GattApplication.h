#pragma once

#include "DBusObject.h"
#include "GattService.h"
#include <vector>
#include <memory>
#include <mutex>

namespace ggk {

/**
 * @brief GATT Application class that manages GATT services for BlueZ
 * 
 * BlueZ의 GATT 애플리케이션을 관리하는 클래스로, 여러 GATT 서비스를 등록하고 관리합니다.
 * BlueZ 5.82와 호환되는 방식으로 D-Bus 등록을 처리합니다.
 */
class GattApplication : public DBusObject {
public:
    /**
     * @brief 생성자
     * 
     * @param connection D-Bus 연결
     * @param path 객체 경로 (기본값: /com/example/gatt)
     */
    GattApplication(
        std::shared_ptr<IDBusConnection> connection, 
        const DBusObjectPath& path = DBusObjectPath("/com/example/gatt")
    );
    
    /**
     * @brief 소멸자
     */
    virtual ~GattApplication() = default;
    
    /**
     * @brief GATT 서비스 추가
     * 
     * @param service 추가할 서비스
     * @return 성공 여부
     */
    bool addService(GattServicePtr service);
    
    /**
     * @brief GATT 서비스 제거
     * 
     * @param uuid 서비스 UUID
     * @return 성공 여부
     */
    bool removeService(const GattUuid& uuid);
    
    /**
     * @brief UUID로 서비스 찾기
     * 
     * @param uuid 서비스 UUID
     * @return 서비스 포인터 (없으면 nullptr)
     */
    GattServicePtr getService(const GattUuid& uuid) const;
    
    /**
     * @brief BlueZ에 등록
     * 
     * 이 메서드는 모든 서비스, 특성, 설명자를 D-Bus에 등록하고
     * BlueZ에 GATT 애플리케이션을 등록합니다.
     * 
     * @return 성공 여부
     */
    bool registerWithBlueZ();
    
    /**
     * @brief BlueZ에서 등록 해제
     * 
     * @return 성공 여부
     */
    bool unregisterFromBlueZ();
    
    /**
     * @brief BlueZ 등록 상태 확인
     */
    bool isRegistered() const { return registered; }
    
    /**
     * @brief 모든 서비스 가져오기
     * 
     * @return 서비스 벡터
     */
    std::vector<GattServicePtr> getServices() const;
    
    /**
     * @brief D-Bus 인터페이스 설정
     * 
     * @return 성공 여부
     */
    bool setupDBusInterfaces();

    /**
     * @brief 모든 인터페이스 등록 확인
     * 
     * 애플리케이션의 모든 서비스, 특성, 설명자가 D-Bus에 등록되었는지 확인합니다.
     * 등록되지 않은 항목이 있으면 등록을 시도합니다.
     * 
     * @return 성공 여부
     */
    bool ensureInterfacesRegistered();
    
    /**
     * @brief 등록 완료 처리
     * 
     * 모든 서비스와 하위 객체의 등록을 완료합니다.
     * 등록 순서는 애플리케이션 -> 서비스 -> 특성 -> 설명자 순입니다.
     * 
     * @return 성공 여부
     */
    bool finishAllRegistrations();
    
private:
    /**
     * @brief GetManagedObjects 메서드 핸들러
     * 
     * @param call 메서드 호출 정보
     */
    void handleGetManagedObjects(const DBusMethodCall& call);
    
    /**
     * @brief 관리되는 객체 사전 생성
     * 
     * @return GVariant 형태의 객체 사전
     */
    GVariantPtr createManagedObjectsDict() const;
    
    /**
     * @brief 표준 서비스 등록
     * 
     * @return 성공 여부
     */
    bool registerStandardServices();
    
    /**
     * @brief 객체 계층 구조 유효성 검사
     * 
     * @return 유효성 검사 결과
     */
    bool validateObjectHierarchy() const;
    
    /**
     * @brief 디버깅용 객체 계층 구조 로깅
     */
    void logObjectHierarchy() const;
    
    // 애플리케이션 상태
    std::vector<GattServicePtr> services;
    mutable std::mutex servicesMutex;
    bool registered;
    bool standardServicesAdded;
};

} // namespace ggk