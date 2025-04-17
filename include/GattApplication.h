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
    bool registerWithBlueZ();
    bool unregisterFromBlueZ();
    bool isRegistered() const { return registered; }
    
    // 인터페이스 설정
    bool setupDBusInterfaces();
    bool finishAllRegistrations();
    
    // 객체 경로 접근자
    const std::string& getPath() const { return object.getPath(); }
    
private:
    // D-Bus 객체 핸들러
    ManagedObjectsDict handleGetManagedObjects();
    ManagedObjectsDict createManagedObjectsDict();
    
    // 내부 상태
    SDBusConnection& connection;
    SDBusObject object;
    std::vector<GattServicePtr> services;
    mutable std::mutex servicesMutex;
    bool registered;
};

} // namespace ggk