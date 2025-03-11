// GattApplication.h
#pragma once

#include "GattTypes.h"
#include "DBusObject.h"
#include "DBusConnection.h"
#include "GattService.h"  // GattServicePtr 정의 포함
#include "GattCharacteristic.h"  // GattCharacteristicPtr 정의 포함
#include "GattDescriptor.h"  // GattDescriptorPtr 정의 포함
#include <vector>
#include <map>
#include <memory>
#include <mutex>

namespace ggk {


// 이 파일에서 사용하는 타입
using GattServicePtr = std::shared_ptr<GattService>;

/**
 * GattApplication - GATT 서비스 계층 구조를 관리하는 클래스
 * 
 * BlueZ의 GattApplication1 인터페이스를 구현하며, 서비스, 특성, 설명자의
 * 계층 구조를 구성하고 관리합니다. ObjectManager 인터페이스도 지원하여
 * BlueZ가 객체 구조를 탐색할 수 있도록 합니다.
 */
class GattApplication : public DBusObject {
public:
    // 생성자
    GattApplication(
        DBusConnection& connection,
        const DBusObjectPath& path
    );
    
    virtual ~GattApplication() = default;
    
    // 서비스 관리
    GattServicePtr createService(
        const GattUuid& uuid,
        bool isPrimary = true
    );
    
    GattServicePtr getService(const GattUuid& uuid) const;
    
    // 속성 접근자
    const std::map<std::string, GattServicePtr>& getServices() const {
        std::lock_guard<std::mutex> lock(servicesMutex);
        return services;
    }
    
    // D-Bus 인터페이스 설정 및 등록
    bool setupDBusInterfaces();
    bool registerWithBluez(const DBusObjectPath& adapterPath);
    bool unregisterFromBluez(const DBusObjectPath& adapterPath);
    
//private:
    // 상수
    static constexpr const char* APPLICATION_INTERFACE = "org.bluez.GattApplication1";
    static constexpr const char* GATT_MANAGER_INTERFACE = "org.bluez.GattManager1";
    static constexpr const char* BLUEZ_SERVICE = "org.bluez";
    
    // 서비스 관리
    std::map<std::string, GattServicePtr> services;
    mutable std::mutex servicesMutex;
    
    bool isRegistered;
    mutable std::mutex registrationMutex;
    
    // D-Bus 메서드 핸들러
    void handleGetManagedObjects(const DBusMethodCall& call);
    
    // 애플리케이션 등록/해제 헬퍼
    bool sendRegisterApplication(const DBusObjectPath& adapterPath);
    bool sendUnregisterApplication(const DBusObjectPath& adapterPath);

    // 응답 생성 헬퍼 메서드들
    void addServiceToResponse(GVariantBuilder* builder, const GattServicePtr& service);
    void addCharacteristicToResponse(GVariantBuilder* builder, const GattCharacteristicPtr& characteristic);
    void addDescriptorToResponse(GVariantBuilder* builder, const GattDescriptorPtr& descriptor);

};

} // namespace ggk