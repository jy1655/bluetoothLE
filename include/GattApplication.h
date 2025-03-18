// GattApplication.h
#pragma once

#include "DBusObject.h"
#include "GattService.h"
#include "GattCharacteristic.h" 
#include "GattDescriptor.h"
#include "BlueZConstants.h"
#include <vector>
#include <memory>
#include <mutex>

namespace ggk {

class GattApplication : public DBusObject {
public:
    // 생성자 - 기본 경로는 /com/example/gatt
    GattApplication(DBusConnection& connection, 
                   const DBusObjectPath& path = DBusObjectPath("/com/example/gatt"));
    virtual ~GattApplication() = default;
    
    // 서비스 관리
    bool addService(GattServicePtr service);
    bool removeService(const GattUuid& uuid);
    GattServicePtr getService(const GattUuid& uuid) const;
    
    // BlueZ 등록
    bool registerWithBlueZ();
    bool unregisterFromBlueZ();
    bool isRegistered() const { return registered; }
    
    // 서비스 조회
    std::vector<GattServicePtr> getServices() const;

    // D-Bus 인터페이스 설정
    bool setupDBusInterfaces();
    bool ensureInterfacesRegistered();
    
    // D-Bus 메서드 핸들러
    void handleGetManagedObjects(const DBusMethodCall& call);
    
//private:
    
    
    // 관리 객체 딕셔너리 생성
    GVariantPtr createManagedObjectsDict() const;
    
    // 속성
    std::vector<GattServicePtr> services;
    mutable std::mutex servicesMutex;
    bool registered;
};

} // namespace ggk