#include "GattApplication.h"
#include "GattService.h"
#include "Logger.h"
#include "Utils.h"

namespace ggk {

GattApplication::GattApplication(
    DBusConnection& connection,
    const DBusObjectPath& path
) : DBusObject(connection, path),
    isRegistered(false) {
}

bool GattApplication::setupDBusInterfaces() {
    // 속성 정의
    std::vector<DBusProperty> properties;
    
    // 인터페이스 추가
    if (!addInterface(APPLICATION_INTERFACE, properties)) {
        Logger::error("Failed to add application interface");
        return false;
    }
    
    // ObjectManager 인터페이스 추가
    if (!addInterface(DBusInterface::OBJECTMANAGER, properties)) {
        Logger::error("Failed to add object manager interface");
        return false;
    }
    
    // 메서드 핸들러 등록 - const 참조로 수정
    if (!addMethod(DBusInterface::OBJECTMANAGER, "GetManagedObjects", 
                  [this](const DBusMethodCall& call) { handleGetManagedObjects(call); })) {
        Logger::error("Failed to add GetManagedObjects method");
        return false;
    }
    
    // 객체 등록
    if (!registerObject()) {
        Logger::error("Failed to register application object");
        return false;
    }
    
    Logger::info("Registered GATT application at path: " + getPath().toString());
    return true;
}

GattServicePtr GattApplication::createService(
    const GattUuid& uuid,
    bool isPrimary
) {
    std::string uuidStr = uuid.toString();
    
    // 이미 존재하는 경우 기존 서비스 반환
    auto it = services.find(uuidStr);
    if (it != services.end()) {
        return it->second;
    }
    
    // 새 경로 생성
    DBusObjectPath servicePath = getPath() + "/service" + std::to_string(services.size() + 1);
    
    // 서비스 생성
    GattServicePtr service = std::make_shared<GattService>(
        getConnection(),
        servicePath,
        uuid,
        isPrimary
    );
    
    // 서비스 등록
    if (!service->setupDBusInterfaces()) {
        Logger::error("Failed to setup service interfaces for: " + uuidStr);
        return nullptr;
    }
    
    // 맵에 추가
    services[uuidStr] = service;
    
    Logger::info("Created service: " + uuidStr + " at path: " + servicePath.toString());
    return service;
}

GattServicePtr GattApplication::getService(const GattUuid& uuid) const {
    std::string uuidStr = uuid.toString();
    auto it = services.find(uuidStr);
    
    if (it != services.end()) {
        return it->second;
    }
    
    return nullptr;
}

bool GattApplication::registerWithBluez(const DBusObjectPath& adapterPath) {
    if (isRegistered) {
        Logger::info("Application already registered");
        return true;
    }
    
    if (!sendRegisterApplication(adapterPath)) {
        Logger::error("Failed to register application with BlueZ");
        return false;
    }
    
    isRegistered = true;
    Logger::info("Application registered with BlueZ at " + adapterPath.toString());
    return true;
}

bool GattApplication::unregisterFromBluez(const DBusObjectPath& adapterPath) {
    if (!isRegistered) {
        Logger::debug("Application not registered, nothing to unregister");
        return true;
    }
    
    if (!sendUnregisterApplication(adapterPath)) {
        Logger::error("Failed to unregister application from BlueZ");
        return false;
    }
    
    isRegistered = false;
    Logger::info("Application unregistered from BlueZ");
    return true;
}

bool GattApplication::sendRegisterApplication(const DBusObjectPath& adapterPath) {
    try {
        // GattManager 프록시 생성
        GDBusProxyPtr gattManagerProxy = GDBusProxyPtr(
            g_dbus_proxy_new_for_bus_sync(
                G_BUS_TYPE_SYSTEM,
                G_DBUS_PROXY_FLAGS_NONE,
                nullptr,
                BLUEZ_SERVICE,
                adapterPath.c_str(),
                GATT_MANAGER_INTERFACE,
                nullptr,
                nullptr
            ),
            &g_object_unref
        );
        
        if (!gattManagerProxy) {
            Logger::error("Failed to create GattManager proxy");
            return false;
        }
        
        // 빈 옵션 딕셔너리 생성
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
        GVariantPtr options = GVariantPtr(
            g_variant_builder_end(&builder),
            &g_variant_unref
        );
        
        // RegisterApplication 호출
        GVariantPtr result = GVariantPtr(
            g_dbus_proxy_call_sync(
                gattManagerProxy.get(),
                "RegisterApplication",
                g_variant_new("(oa{sv})",
                              getPath().c_str(),
                              options.get()),
                G_DBUS_CALL_FLAGS_NONE,
                -1,
                nullptr,
                nullptr
            ),
            &g_variant_unref
        );
        
        if (!result) {
            Logger::error("Failed to register application");
            return false;
        }
        
    } catch (const std::exception& e) {
        Logger::error(std::string("Exception during application registration: ") + e.what());
        return false;
    }
    
    return true;
}

bool GattApplication::sendUnregisterApplication(const DBusObjectPath& adapterPath) {
    try {
        // GattManager 프록시 생성
        GDBusProxyPtr gattManagerProxy = GDBusProxyPtr(
            g_dbus_proxy_new_for_bus_sync(
                G_BUS_TYPE_SYSTEM,
                G_DBUS_PROXY_FLAGS_NONE,
                nullptr,
                BLUEZ_SERVICE,
                adapterPath.c_str(),
                GATT_MANAGER_INTERFACE,
                nullptr,
                nullptr
            ),
            &g_object_unref
        );
        
        if (!gattManagerProxy) {
            Logger::error("Failed to create GattManager proxy");
            return false;
        }
        
        // UnregisterApplication 호출
        GVariantPtr result = GVariantPtr(
            g_dbus_proxy_call_sync(
                gattManagerProxy.get(),
                "UnregisterApplication",
                g_variant_new("(o)",
                              getPath().c_str()),
                G_DBUS_CALL_FLAGS_NONE,
                -1,
                nullptr,
                nullptr
            ),
            &g_variant_unref
        );
        
        if (!result) {
            Logger::error("Failed to unregister application");
            return false;
        }
        
    } catch (const std::exception& e) {
        Logger::error(std::string("Exception during application unregistration: ") + e.what());
        return false;
    }
    
    return true;
}

// 핸들러 시그니처 수정 - const 참조 사용
void GattApplication::handleGetManagedObjects(const DBusMethodCall& call) {
    Logger::debug("GetManagedObjects called");
    
    // 객체 맵을 위한 빌더 생성
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
    
    // 모든 서비스 추가
    for (const auto& [uuidStr, service] : services) {
        // 서비스 경로 문자열
        std::string servicePath = service->getPath().toString();
        
        // 서비스 인터페이스 빌더
        GVariantBuilder ifaceBuilder;
        g_variant_builder_init(&ifaceBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
        
        // 서비스 인터페이스 속성 빌더
        GVariantBuilder propsBuilder;
        g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));
        
        // UUID 속성 추가
        g_variant_builder_add(&propsBuilder, "{sv}", "UUID",
                              Utils::gvariantFromString(service->getUuid().toBlueZFormat()));
        
        // Primary 속성 추가
        g_variant_builder_add(&propsBuilder, "{sv}", "Primary",
                              Utils::gvariantFromBoolean(service->isPrimary()));
        
        // 인터페이스에 속성 추가
        g_variant_builder_add(&ifaceBuilder, "{sa{sv}}", "org.bluez.GattService1",
                              g_variant_builder_end(&propsBuilder));
        
        // 객체에 인터페이스 추가
        g_variant_builder_add(&builder, "{oa{sa{sv}}}", servicePath.c_str(),
                              g_variant_builder_end(&ifaceBuilder));
        
        // 모든 특성도 추가 (현재는 간략하게 구현, 실제로는 모든 특성과 설명자도 추가해야 함)
        // ...
    }
    
    // 응답 생성
    GVariantPtr resultVariant = GVariantPtr(
        g_variant_builder_end(&builder),
        &g_variant_unref
    );
    
    // 메서드 응답 생성 및 전송 - GDBus 직접 호출 방식으로 변경
    if (call.invocation) {
        g_dbus_method_invocation_return_value(call.invocation.get(), resultVariant.get());
    } else {
        Logger::error("Invalid method invocation in GetManagedObjects");
    }
}

} // namespace ggk