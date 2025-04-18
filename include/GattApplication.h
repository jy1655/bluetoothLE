// include/GattApplication.h
#pragma once

#include "SDBusInterface.h"
#include "SDBusObject.h"
#include "GattService.h"
#include <vector>
#include <memory>
#include <mutex>

namespace ggk {

    using ManagedObjectsDict = std::map<sdbus::ObjectPath, 
                            std::map<std::string, 
                            std::map<std::string, sdbus::Variant>>>;

class GattApplication {
public:


    GattApplication(SDBusConnection& connection, const std::string& path = "/com/example/gatt");
    virtual ~GattApplication();
    
    // 서비스 관리
    bool addService(GattServicePtr service);
    bool removeService(const GattUuid& uuid);
    GattServicePtr getService(const GattUuid& uuid) const;
    std::vector<GattServicePtr> getServices() const;
    
    // BlueZ 등록
    bool isRegistered() const { return registered; }
    
    // 인터페이스 설정
    /**
     * @brief 모든 D-Bus 인터페이스 설정 (자신과 모든 하위 객체)
     * 
     * sdbus-c++ 2.1.0 호환을 위해 addObjectManager()를 사용하여 
     * GetManagedObjects 메서드를 자동으로 구현합니다.
     * 
     * @return 설정 성공 여부
     */
    bool setupInterfaces();
    
    /**
     * @brief BlueZ에 애플리케이션 바인딩 (등록)
     * @return 바인딩 성공 여부
     */
    bool bindToBlueZ();
    
    /**
     * @brief BlueZ에서 애플리케이션 바인딩 해제
     * @return 바인딩 해제 성공 여부
     */
    bool unbindFromBlueZ();
    
    /**
     * @brief 인터페이스 설정 상태 확인
     * @return 설정 완료 여부
     */
    bool isInterfaceSetup() const { return interfaceSetup; }
    
    /**
     * @brief BlueZ 바인딩 상태 확인
     * @return 바인딩 완료 여부
     */
    bool isBoundToBlueZ() const { return boundToBlueZ; }
    
    // 객체 경로 접근자
    const std::string& getPath() const { return object.getPath(); }
    
private:
    // D-Bus 객체 핸들러 - 이 메서드들은 더 이상 필요하지 않지만 호환성을 위해 유지
    // sdbus-c++ 2.1.0에서는 addObjectManager()가 자동으로 GetManagedObjects를 처리함
    ManagedObjectsDict handleGetManagedObjects();
    ManagedObjectsDict createManagedObjectsDict();
    
    // 내부 상태
    SDBusConnection& connection;
    SDBusObject object;
    std::vector<GattServicePtr> services;
    mutable std::mutex servicesMutex;
    bool registered;
    bool interfaceSetup{false}; // 인터페이스 설정 완료 여부
    bool boundToBlueZ{false};   // BlueZ 바인딩 완료 여부
};

} // namespace ggk