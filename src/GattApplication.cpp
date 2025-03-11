#include "GattApplication.h"
#include "GattService.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
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
    if (DBusObject::registerObject()) {
        Logger::info("Registered GATT application at path: " + getPath().toString());
        // 여기서는 DBusObject::isRegistered()가 true가 되지만
        // this->isRegistered는 여전히 false입니다 (BlueZ에 등록되지 않음)
        return true;
    }
    return false;
}

GattServicePtr GattApplication::createService(
    const GattUuid& uuid,
    bool isPrimary
) {
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(servicesMutex);
    
    // 이미 존재하는 경우 기존 서비스 반환
    auto it = services.find(uuidStr);
    if (it != services.end()) {
        return it->second;
    }
    
    try {
        // 새 경로 생성
        DBusObjectPath servicePath = getPath() + "/service" + std::to_string(services.size() + 1);
        
        // 서비스 생성
        GattServicePtr service = std::make_shared<GattService>(
            getConnection(),
            servicePath,
            uuid,
            isPrimary
        );
        
        if (!service) {
            Logger::error("Failed to create service: " + uuidStr);
            return nullptr;
        }
        
        // 서비스 등록
        if (!service->setupDBusInterfaces()) {
            Logger::error("Failed to setup service interfaces for: " + uuidStr);
            return nullptr;
        }
        
        // 맵에 추가
        services[uuidStr] = service;
        
        Logger::info("Created service: " + uuidStr + " at path: " + servicePath.toString());
        return service;
    } catch (const std::exception& e) {
        Logger::error("Exception during service creation: " + std::string(e.what()));
        return nullptr;
    }
}

GattServicePtr GattApplication::getService(const GattUuid& uuid) const {
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(servicesMutex);
    
    auto it = services.find(uuidStr);
    if (it != services.end()) {
        return it->second;
    }
    
    return nullptr;
}

bool GattApplication::registerWithBluez(const DBusObjectPath& adapterPath) {
    // 경로가 비어있는지 확인
    if (adapterPath.toString().empty()) {
        Logger::error("Invalid adapter path for registration");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(registrationMutex);
    
    if (isRegistered) {
        Logger::info("Application already registered");
        return true;
    }
    
    try {
        // GattManager 프록시 생성
        GError* rawError = nullptr;
        GDBusProxy* rawProxy = g_dbus_proxy_new_for_bus_sync(
            G_BUS_TYPE_SYSTEM,
            G_DBUS_PROXY_FLAGS_NONE,
            nullptr,
            BLUEZ_SERVICE,
            adapterPath.c_str(),
            GATT_MANAGER_INTERFACE,
            nullptr,
            &rawError
        );
        
        // 오류 또는 NULL 프록시 처리
        if (rawError) {
            std::string errorMsg = rawError->message ? rawError->message : "Unknown error";
            Logger::error("Failed to create GattManager proxy: " + errorMsg);
            g_error_free(rawError);
            return false;
        }
        
        if (!rawProxy) {
            Logger::error("Failed to create GattManager proxy: Unknown error");
            return false;
        }
        
        // 스마트 포인터로 관리 (자동 정리)
        GDBusProxyPtr gattManagerProxy(rawProxy, &g_object_unref);
        
        // *** 문제의 코드: GVariantBuilder 대신 직접 빈 배열 생성 ***
        // 빈 속성 배열 생성 (a{sv})
        GVariant* emptyDict = Utils::createEmptyDictionary();
        if (!emptyDict) {
            Logger::error("Failed to create empty dictionary");
            return false;
        }
        
        // RegisterApplication 호출
        GError* callError = nullptr;
        GVariant* rawResult = g_dbus_proxy_call_sync(
            gattManagerProxy.get(),
            "RegisterApplication",
            g_variant_new("(o@a{sv})", getPath().c_str(), emptyDict),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &callError
        );
        
        // 호출 오류 처리
        if (callError) {
            std::string errorMsg = callError->message ? callError->message : "Unknown error";
            Logger::error("Failed to register application: " + errorMsg);
            g_error_free(callError);
            return false;
        }
        
        // 결과 NULL 체크
        if (!rawResult) {
            Logger::error("Failed to register application: No result returned");
            return false;
        }
        
        // 결과 처리 및 해제
        g_variant_unref(rawResult);
        
        // 성공적으로 등록됨
        isRegistered = true;
        Logger::info("Application registered with BlueZ at " + adapterPath.toString());
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Exception during application registration: " + std::string(e.what()));
        return false;
    } catch (...) {
        Logger::error("Unknown exception during application registration");
        return false;
    }
}

bool GattApplication::unregisterFromBluez(const DBusObjectPath& adapterPath) {
    if (adapterPath.toString().empty()) {
        Logger::error("Invalid adapter path for unregistration");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(registrationMutex);
    
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
        // GattManager 프록시 생성 - 초기화 수정
        GError* rawError = nullptr;  // raw 포인터 사용
        
        GDBusProxy* rawProxy = g_dbus_proxy_new_for_bus_sync(
            G_BUS_TYPE_SYSTEM,
            G_DBUS_PROXY_FLAGS_NONE,
            nullptr,
            BLUEZ_SERVICE,
            adapterPath.c_str(),
            GATT_MANAGER_INTERFACE,
            nullptr,
            &rawError  // raw 포인터 참조로 전달
        );
        
        // 에러 처리 수정
        if (rawError) {
            std::string errorMsg = rawError->message ? rawError->message : "Unknown error";
            Logger::error("Failed to create GattManager proxy: " + errorMsg);
            g_error_free(rawError);  // 직접 해제
            return false;
        }
        
        if (!rawProxy) {
            Logger::error("Failed to create GattManager proxy: Unknown error");
            return false;
        }
        
        // unique_ptr을 올바르게 초기화
        GDBusProxyPtr gattManagerProxy(rawProxy, &g_object_unref);
        
        // 빈 옵션 딕셔너리 생성
        GVariantPtr options(Utils::createEmptyDictionary(), &g_variant_unref);
        
        // RegisterApplication 호출
        GError* callError = nullptr;  // 또 다른 raw 포인터
        GVariant* rawResult = g_dbus_proxy_call_sync(
            gattManagerProxy.get(),
            "RegisterApplication",
            g_variant_new("(oa{sv})", getPath().c_str(), options.get()),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &callError  // raw 포인터 참조로 전달
        );
        
        if (callError) {
            std::string errorMsg = callError->message ? callError->message : "Unknown error";
            Logger::error("Failed to register application: " + errorMsg);
            g_error_free(callError);  // 직접 해제
            return false;
        }
        
        if (!rawResult) {
            Logger::error("Failed to register application: No result returned");
            return false;
        }
        
        // 결과 처리 및 해제
        g_variant_unref(rawResult);
        
    } catch (const std::exception& e) {
        Logger::error(std::string("Exception during application registration: ") + e.what());
        return false;
    }
    
    return true;
}

bool GattApplication::sendUnregisterApplication(const DBusObjectPath& adapterPath) {
    try {
        // GattManager 프록시 생성
        GError* rawError = nullptr;
        GDBusProxy* rawProxy = g_dbus_proxy_new_for_bus_sync(
            G_BUS_TYPE_SYSTEM,
            G_DBUS_PROXY_FLAGS_NONE,
            nullptr,
            BLUEZ_SERVICE,
            adapterPath.c_str(),
            GATT_MANAGER_INTERFACE,
            nullptr,
            &rawError
        );
        
        // 오류 또는 NULL 프록시 처리
        if (rawError) {
            std::string errorMsg = rawError->message ? rawError->message : "Unknown error";
            Logger::error("Failed to create GattManager proxy: " + errorMsg);
            g_error_free(rawError);
            return false;
        }
        
        if (!rawProxy) {
            Logger::error("Failed to create GattManager proxy: Unknown error");
            return false;
        }
        
        // 스마트 포인터로 관리
        GDBusProxyPtr gattManagerProxy(rawProxy, &g_object_unref);
        
        // UnregisterApplication 호출
        GError* callError = nullptr;
        GVariant* rawResult = g_dbus_proxy_call_sync(
            gattManagerProxy.get(),
            "UnregisterApplication",
            g_variant_new("(o)", getPath().c_str()),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &callError
        );
        
        // 호출 오류 처리
        if (callError) {
            std::string errorMsg = callError->message ? callError->message : "Unknown error";
            Logger::error("Failed to unregister application: " + errorMsg);
            g_error_free(callError);
            return false;
        }
        
        // 결과 NULL 체크
        if (!rawResult) {
            Logger::error("Failed to unregister application: No result returned");
            return false;
        }
        
        // 결과 처리 및 해제
        g_variant_unref(rawResult);
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Exception during application unregistration: " + std::string(e.what()));
        return false;
    } catch (...) {
        Logger::error("Unknown exception during application unregistration");
        return false;
    }
}

void GattApplication::handleGetManagedObjects(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in GetManagedObjects");
        return;
    }
    
    Logger::debug("GetManagedObjects called - preparing response with " + 
                 std::to_string(services.size()) + " services");
    
    try {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
        
        // 각 서비스 정보 추가
        {
            std::lock_guard<std::mutex> lock(servicesMutex);
            for (const auto& [uuidStr, service] : services) {
                if (!service) continue;
                
                Logger::debug("Adding service to response: " + uuidStr);
                addServiceToResponse(&builder, service);
            }
        }
        
        // 빌더에서 최종 변형 생성
        GVariant* result = g_variant_builder_end(&builder);
        if (!result) {
            Logger::error("Failed to create managed objects dictionary");
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(), G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                "Failed to create response"
            );
            return;
        }
        
        // 디버깅을 위해 응답 내용 출력
        gchar* resultStr = g_variant_print(result, TRUE);
        Logger::debug("GetManagedObjects response: " + std::string(resultStr));
        g_free(resultStr);
        
        // 응답 보내기
        g_dbus_method_invocation_return_value(call.invocation.get(), result);
        
    } catch (const std::exception& e) {
        Logger::error("Exception in GetManagedObjects: " + std::string(e.what()));
        g_dbus_method_invocation_return_error_literal(
            call.invocation.get(), G_DBUS_ERROR, G_DBUS_ERROR_FAILED, e.what()
        );
    }
}

// 서비스를 응답에 추가하는 헬퍼 메서드
void GattApplication::addServiceToResponse(GVariantBuilder* builder, const GattServicePtr& service) {
    // 서비스 경로
    const std::string& path = service->getPath().toString();
    
    // 인터페이스 빌더
    GVariantBuilder ifacesBuilder;
    g_variant_builder_init(&ifacesBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
    
    // 서비스 인터페이스 속성
    GVariantBuilder propsBuilder;
    g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));
    
    // UUID 속성
    g_variant_builder_add(&propsBuilder, "{sv}", "UUID", 
        g_variant_new_string(service->getUuid().toBlueZFormat().c_str()));
    
    // Primary 속성
    g_variant_builder_add(&propsBuilder, "{sv}", "Primary", 
        g_variant_new_boolean(service->isPrimary()));
    
    // 인터페이스에 속성 추가
    g_variant_builder_add(&ifacesBuilder, "{sa{sv}}", 
        "org.bluez.GattService1", g_variant_builder_end(&propsBuilder));
    
    // 객체에 인터페이스 추가
    g_variant_builder_add(builder, "{oa{sa{sv}}}", 
        path.c_str(), g_variant_builder_end(&ifacesBuilder));
    
    // 각 특성도 추가
    for (const auto& [charUuid, characteristic] : service->getCharacteristics()) {
        if (characteristic) {
            addCharacteristicToResponse(builder, characteristic);
        }
    }
}

// GattApplication.cpp에 추가

// 특성을 응답에 추가하는 헬퍼 메서드
void GattApplication::addCharacteristicToResponse(GVariantBuilder* builder, const GattCharacteristicPtr& characteristic) {
    // 특성 경로
    const std::string& path = characteristic->getPath().toString();
    
    // 인터페이스 빌더
    GVariantBuilder ifacesBuilder;
    g_variant_builder_init(&ifacesBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
    
    // 특성 인터페이스 속성
    GVariantBuilder propsBuilder;
    g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));
    
    // UUID 속성
    g_variant_builder_add(&propsBuilder, "{sv}", "UUID", 
        g_variant_new_string(characteristic->getUuid().toBlueZFormat().c_str()));
    
    // Service 속성 (부모 서비스에 대한 참조)
    g_variant_builder_add(&propsBuilder, "{sv}", "Service", 
        g_variant_new_string(characteristic->getService().getPath().c_str()));
    
    // Flags 속성 (속성을 나타내는 문자열 배열)
    GVariantBuilder flagsBuilder;
    g_variant_builder_init(&flagsBuilder, G_VARIANT_TYPE("as"));
    
    uint8_t props = characteristic->getProperties();
    if (props & static_cast<uint8_t>(GattProperty::BROADCAST))
        g_variant_builder_add(&flagsBuilder, "s", "broadcast");
    if (props & static_cast<uint8_t>(GattProperty::READ))
        g_variant_builder_add(&flagsBuilder, "s", "read");
    if (props & static_cast<uint8_t>(GattProperty::WRITE_WITHOUT_RESPONSE))
        g_variant_builder_add(&flagsBuilder, "s", "write-without-response");
    if (props & static_cast<uint8_t>(GattProperty::WRITE))
        g_variant_builder_add(&flagsBuilder, "s", "write");
    if (props & static_cast<uint8_t>(GattProperty::NOTIFY))
        g_variant_builder_add(&flagsBuilder, "s", "notify");
    if (props & static_cast<uint8_t>(GattProperty::INDICATE))
        g_variant_builder_add(&flagsBuilder, "s", "indicate");
    if (props & static_cast<uint8_t>(GattProperty::AUTHENTICATED_SIGNED_WRITES))
        g_variant_builder_add(&flagsBuilder, "s", "authenticated-signed-writes");
    
    g_variant_builder_add(&propsBuilder, "{sv}", "Flags", g_variant_builder_end(&flagsBuilder));
    
    // Descriptors 속성 (객체 경로 배열)
    GVariantBuilder descriptorsBuilder;
    g_variant_builder_init(&descriptorsBuilder, G_VARIANT_TYPE("ao"));
    
    for (const auto& [descUuid, descriptor] : characteristic->getDescriptors()) {
        if (descriptor) {
            g_variant_builder_add(&descriptorsBuilder, "o", descriptor->getPath().c_str());
        }
    }
    
    g_variant_builder_add(&propsBuilder, "{sv}", "Descriptors", g_variant_builder_end(&descriptorsBuilder));
    
    // Notifying 속성 (부울)
    g_variant_builder_add(&propsBuilder, "{sv}", "Notifying", 
        g_variant_new_boolean(characteristic->isNotifying()));
    
    // 인터페이스에 속성 추가
    g_variant_builder_add(&ifacesBuilder, "{sa{sv}}", 
        "org.bluez.GattCharacteristic1", g_variant_builder_end(&propsBuilder));
    
    // 객체에 인터페이스 추가
    g_variant_builder_add(builder, "{oa{sa{sv}}}", 
        path.c_str(), g_variant_builder_end(&ifacesBuilder));
    
    // 각 설명자도 추가
    for (const auto& [descUuid, descriptor] : characteristic->getDescriptors()) {
        if (descriptor) {
            addDescriptorToResponse(builder, descriptor);
        }
    }
}

// 설명자를 응답에 추가하는 헬퍼 메서드
void GattApplication::addDescriptorToResponse(GVariantBuilder* builder, const GattDescriptorPtr& descriptor) {
    // 설명자 경로
    const std::string& path = descriptor->getPath().toString();
    
    // 인터페이스 빌더
    GVariantBuilder ifacesBuilder;
    g_variant_builder_init(&ifacesBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
    
    // 설명자 인터페이스 속성
    GVariantBuilder propsBuilder;
    g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));
    
    // UUID 속성
    g_variant_builder_add(&propsBuilder, "{sv}", "UUID", 
        g_variant_new_string(descriptor->getUuid().toBlueZFormat().c_str()));
    
    // Characteristic 속성 (부모 특성에 대한 참조)
    g_variant_builder_add(&propsBuilder, "{sv}", "Characteristic", 
        g_variant_new_string(descriptor->getCharacteristic().getPath().c_str()));
    
    // Flags 속성 (권한을 나타내는 문자열 배열)
    GVariantBuilder flagsBuilder;
    g_variant_builder_init(&flagsBuilder, G_VARIANT_TYPE("as"));
    
    uint8_t perms = descriptor->getPermissions();
    if (perms & static_cast<uint8_t>(GattPermission::READ))
        g_variant_builder_add(&flagsBuilder, "s", "read");
    if (perms & static_cast<uint8_t>(GattPermission::WRITE))
        g_variant_builder_add(&flagsBuilder, "s", "write");
    if (perms & static_cast<uint8_t>(GattPermission::READ_ENCRYPTED))
        g_variant_builder_add(&flagsBuilder, "s", "encrypt-read");
    if (perms & static_cast<uint8_t>(GattPermission::WRITE_ENCRYPTED))
        g_variant_builder_add(&flagsBuilder, "s", "encrypt-write");
    if (perms & static_cast<uint8_t>(GattPermission::READ_AUTHENTICATED))
        g_variant_builder_add(&flagsBuilder, "s", "auth-read");
    if (perms & static_cast<uint8_t>(GattPermission::WRITE_AUTHENTICATED))
        g_variant_builder_add(&flagsBuilder, "s", "auth-write");
    
    g_variant_builder_add(&propsBuilder, "{sv}", "Flags", g_variant_builder_end(&flagsBuilder));
    
    // 인터페이스에 속성 추가
    g_variant_builder_add(&ifacesBuilder, "{sa{sv}}", 
        "org.bluez.GattDescriptor1", g_variant_builder_end(&propsBuilder));
    
    // 객체에 인터페이스 추가
    g_variant_builder_add(builder, "{oa{sa{sv}}}", 
        path.c_str(), g_variant_builder_end(&ifacesBuilder));
}

} // namespace ggk