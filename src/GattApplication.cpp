// GattApplication.cpp
#include "GattApplication.h"
#include "Logger.h"
#include "Utils.h"

namespace ggk {

GattApplication::GattApplication(DBusConnection& connection, const DBusObjectPath& path)
    : DBusObject(connection, path),
      registered(false) {
    
    if (connection.isConnected()) {
        try {
            // 시스템 버스에 이름 등록
            const std::string busName = "com.example.gatt";
            GVariantPtr result = connection.callMethod(
                "org.freedesktop.DBus",
                DBusObjectPath("/org/freedesktop/DBus"),
                "org.freedesktop.DBus",
                "RequestName",
                GVariantPtr(g_variant_new("(su)", busName.c_str(), 0x4), &g_variant_unref)  // 0x4 = DBUS_NAME_FLAG_DO_NOT_QUEUE
            );
            
            guint32 ret = 0;
            g_variant_get(result.get(), "(u)", &ret);
            
            if (ret == 1) {  // DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER
                Logger::info("Successfully acquired bus name: " + busName);
            } else {
                Logger::error("Failed to acquire bus name: " + busName + ", return code: " + std::to_string(ret));
            }
        } catch (const std::exception& e) {
            Logger::error("Failed to request bus name: " + std::string(e.what()));
        }
    }
}

bool GattApplication::setupDBusInterfaces() {
    // 1. 먼저 ObjectManager 인터페이스 추가
    if (!addInterface(BlueZConstants::OBJECT_MANAGER_INTERFACE, {})) {
        Logger::error("Failed to add ObjectManager interface");
        return false;
    }
    
    // 2. 그 다음 GetManagedObjects 메서드 핸들러 등록
    if (!addMethod(BlueZConstants::OBJECT_MANAGER_INTERFACE, "GetManagedObjects", 
                  [this](const DBusMethodCall& call) { 
                      Logger::info("GetManagedObjects called by BlueZ");
                      handleGetManagedObjects(call); 
                  })) {
        Logger::error("Failed to add GetManagedObjects method");
        return false;
    }
    
    // 3. 마지막으로 객체 등록
    if (!registerObject()) {
        Logger::error("Failed to register application object");
        return false;
    }
    
    Logger::info("Registered GATT application: " + getPath().toString());
    return true;
}

bool GattApplication::addService(GattServicePtr service) {
    if (!service) {
        Logger::error("Cannot add null service");
        return false;
    }
    
    std::string uuidStr = service->getUuid().toString();
    
    std::lock_guard<std::mutex> lock(servicesMutex);
    
    // 중복 서비스 검사
    for (const auto& existingService : services) {
        if (existingService->getUuid().toString() == uuidStr) {
            Logger::warn("Service already exists: " + uuidStr);
            return false;
        }
    }
    
    // 서비스 추가
    services.push_back(service);
    
    Logger::info("Added service to application: " + uuidStr);
    return true;
}

bool GattApplication::removeService(const GattUuid& uuid) {
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(servicesMutex);
    
    for (auto it = services.begin(); it != services.end(); ++it) {
        if ((*it)->getUuid().toString() == uuidStr) {
            services.erase(it);
            Logger::info("Removed service from application: " + uuidStr);
            return true;
        }
    }
    
    Logger::warn("Service not found: " + uuidStr);
    return false;
}

GattServicePtr GattApplication::getService(const GattUuid& uuid) const {
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(servicesMutex);
    
    for (const auto& service : services) {
        if (service->getUuid().toString() == uuidStr) {
            return service;
        }
    }
    
    return nullptr;
}

std::vector<GattServicePtr> GattApplication::getServices() const {
    std::lock_guard<std::mutex> lock(servicesMutex);
    return services;
}

bool GattApplication::registerWithBlueZ() {
    try {
        if (registered) {
            Logger::info("Application already registered with BlueZ");
            return true;
        }
        
        if (!setupDBusInterfaces()) {
            Logger::error("Failed to setup D-Bus interfaces");
            return false;
        }

        // D-Bus 메시지 큐가 처리될 시간 제공
        usleep(100000); // 100ms 대기
        
        // 더 안전한 방식으로 빈 딕셔너리 생성
        GVariant* emptyDict = g_variant_new_array(G_VARIANT_TYPE("{sv}"), nullptr, 0);
        
        // 명시적 유형 지정과 함께 파라미터 생성
        GVariant* params = g_variant_new("(o@a{sv})",
                                        getPath().c_str(),
                                        emptyDict);
        
        // 스마트 포인터로 래핑
        GVariantPtr parameters(params, &g_variant_unref);
        
        // 메서드 호출
        GVariantPtr result = getConnection().callMethod(
            BlueZConstants::BLUEZ_SERVICE,
            DBusObjectPath(BlueZConstants::ADAPTER_PATH),
            BlueZConstants::GATT_MANAGER_INTERFACE,
            BlueZConstants::REGISTER_APPLICATION,
            std::move(parameters)
        );
        
        if (!result) {
            Logger::error("Failed to register application with BlueZ");
            return false;
        }
        
        registered = true;
        Logger::info("Successfully registered application with BlueZ");
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception in registerWithBlueZ: " + std::string(e.what()));
        return false;
    }
}

bool GattApplication::unregisterFromBlueZ() {
    try {
        // 등록되어 있지 않으면 성공으로 간주
        if (!registered) {
            return true;
        }
        
        // 더 간단한 방식으로 매개변수 생성
        GVariant* params = g_variant_new("(o)", getPath().c_str());
        GVariantPtr parameters(params, &g_variant_unref);
        
        GVariantPtr result = getConnection().callMethod(
            BlueZConstants::BLUEZ_SERVICE,
            DBusObjectPath(BlueZConstants::ADAPTER_PATH),
            BlueZConstants::GATT_MANAGER_INTERFACE,
            BlueZConstants::UNREGISTER_APPLICATION,
            std::move(parameters)
        );
        
        if (!result) {
            Logger::error("Failed to unregister application from BlueZ");
            return false;
        }
        
        registered = false;
        Logger::info("Successfully unregistered application from BlueZ");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Exception in unregisterFromBlueZ: " + std::string(e.what()));
        return false;
    }
}

void GattApplication::handleGetManagedObjects(const DBusMethodCall& call) {
    Logger::info("GetManagedObjects called by BlueZ");
    try {
        if (!call.invocation) {
            Logger::error("Invalid method invocation in GetManagedObjects");
            return;
        }
        
        Logger::debug("GetManagedObjects called for application: " + getPath().toString());
        
        // 관리 객체 딕셔너리 생성
        GVariantPtr result = createManagedObjectsDict();
        
        if (!result) {
            Logger::error("Failed to create managed objects dictionary");
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(),
                G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED,
                "Failed to create response"
            );
            return;
        }
        
        // 메서드 응답 생성 및 전송
        g_dbus_method_invocation_return_value(call.invocation.get(), result.get());
        
    } catch (const std::exception& e) {
        Logger::error("Exception in handleGetManagedObjects: " + std::string(e.what()));
        g_dbus_method_invocation_return_error_literal(
            call.invocation.get(),
            G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED,
            e.what()
        );
    }
}

GVariantPtr GattApplication::createManagedObjectsDict() const {
    Logger::info("Creating managed objects dictionary");
    
    // 최상위 빌더: 객체 경로 -> 인터페이스 맵 딕셔너리 (a{oa{sa{sv}}})
    GVariantBuilder objects_builder;
    g_variant_builder_init(&objects_builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));

    // 서비스 목록 가져오기
    std::vector<GattServicePtr> servicesList;
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        servicesList = services;
    }

    // 서비스 처리
    for (const auto& service : servicesList) {
        if (!service) continue;
        
        // 이 서비스의 인터페이스 맵 (a{sa{sv}})
        GVariantBuilder interfaces_builder;
        g_variant_builder_init(&interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));

        // 서비스 인터페이스의 속성 맵 (a{sv})
        GVariantBuilder service_properties_builder;
        g_variant_builder_init(&service_properties_builder, G_VARIANT_TYPE("a{sv}"));

        // UUID 속성
        g_variant_builder_add(&service_properties_builder, "{sv}",
                             "UUID",
                             g_variant_new_string(service->getUuid().toBlueZFormat().c_str()));

        // Primary 속성
        g_variant_builder_add(&service_properties_builder, "{sv}",
                             "Primary",
                             g_variant_new_boolean(service->isPrimary()));

        // Characteristics 속성 (객체 경로 배열)
        GVariantBuilder char_paths_builder;
        g_variant_builder_init(&char_paths_builder, G_VARIANT_TYPE("ao"));

        auto characteristics = service->getCharacteristics();
        for (const auto& pair : characteristics) {
            if (pair.second) {
                g_variant_builder_add(&char_paths_builder, "o", 
                                     pair.second->getPath().c_str());
            }
        }

        g_variant_builder_add(&service_properties_builder, "{sv}",
                             "Characteristics",
                             g_variant_builder_end(&char_paths_builder));

        // 서비스 인터페이스 추가
        g_variant_builder_add(&interfaces_builder, "{sa{sv}}",
                             BlueZConstants::GATT_SERVICE_INTERFACE.c_str(),
                             &service_properties_builder);

        // 서비스 객체 추가
        g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}", 
                             service->getPath().c_str(),
                             &interfaces_builder);

        // 이 서비스의 모든 특성 처리
        for (const auto& char_pair : characteristics) {
            const auto& characteristic = char_pair.second;
            if (!characteristic) continue;

            // 특성의 인터페이스 맵
            GVariantBuilder char_interfaces_builder;
            g_variant_builder_init(&char_interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));

            // 특성 속성 맵
            GVariantBuilder char_properties_builder;
            g_variant_builder_init(&char_properties_builder, G_VARIANT_TYPE("a{sv}"));

            // UUID 속성
            g_variant_builder_add(&char_properties_builder, "{sv}",
                                 "UUID",
                                 g_variant_new_string(characteristic->getUuid().toBlueZFormat().c_str()));

            // Service 속성
            g_variant_builder_add(&char_properties_builder, "{sv}",
                                 "Service",
                                 g_variant_new_object_path(service->getPath().c_str()));

            // Flags 속성 (문자열 배열)
            GVariantBuilder flags_builder;
            g_variant_builder_init(&flags_builder, G_VARIANT_TYPE("as"));

            // 속성 플래그 추가
            uint8_t props = characteristic->getProperties();
            if (props & GattProperty::PROP_BROADCAST)
                g_variant_builder_add(&flags_builder, "s", "broadcast");
            if (props & GattProperty::PROP_READ)
                g_variant_builder_add(&flags_builder, "s", "read");
            if (props & GattProperty::PROP_WRITE_WITHOUT_RESPONSE)
                g_variant_builder_add(&flags_builder, "s", "write-without-response");
            if (props & GattProperty::PROP_WRITE)
                g_variant_builder_add(&flags_builder, "s", "write");
            if (props & GattProperty::PROP_NOTIFY)
                g_variant_builder_add(&flags_builder, "s", "notify");
            if (props & GattProperty::PROP_INDICATE)
                g_variant_builder_add(&flags_builder, "s", "indicate");
            if (props & GattProperty::PROP_AUTHENTICATED_SIGNED_WRITES)
                g_variant_builder_add(&flags_builder, "s", "authenticated-signed-writes");

            // 권한 기반 플래그 추가
            uint8_t perms = characteristic->getPermissions();
            if (perms & GattPermission::PERM_READ_ENCRYPTED)
                g_variant_builder_add(&flags_builder, "s", "encrypt-read");
            if (perms & GattPermission::PERM_WRITE_ENCRYPTED)
                g_variant_builder_add(&flags_builder, "s", "encrypt-write");
            if (perms & GattPermission::PERM_READ_AUTHENTICATED)
                g_variant_builder_add(&flags_builder, "s", "auth-read");
            if (perms & GattPermission::PERM_WRITE_AUTHENTICATED)
                g_variant_builder_add(&flags_builder, "s", "auth-write");

            g_variant_builder_add(&char_properties_builder, "{sv}",
                                 "Flags",
                                 g_variant_builder_end(&flags_builder));

            // Descriptors 속성 (객체 경로 배열)
            GVariantBuilder desc_paths_builder;
            g_variant_builder_init(&desc_paths_builder, G_VARIANT_TYPE("ao"));

            auto descriptors = characteristic->getDescriptors();
            for (const auto& desc_pair : descriptors) {
                if (desc_pair.second) {
                    g_variant_builder_add(&desc_paths_builder, "o", 
                                         desc_pair.second->getPath().c_str());
                }
            }

            g_variant_builder_add(&char_properties_builder, "{sv}",
                                 "Descriptors",
                                 g_variant_builder_end(&desc_paths_builder));

            // Notifying 속성
            g_variant_builder_add(&char_properties_builder, "{sv}",
                                 "Notifying",
                                 g_variant_new_boolean(characteristic->isNotifying()));

            // 특성 인터페이스 추가
            g_variant_builder_add(&char_interfaces_builder, "{sa{sv}}",
                                 BlueZConstants::GATT_CHARACTERISTIC_INTERFACE.c_str(),
                                 &char_properties_builder);

            // 특성 객체 추가
            g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}", 
                                 characteristic->getPath().c_str(),
                                 &char_interfaces_builder);

            // 이 특성의 모든 설명자 처리
            for (const auto& desc_pair : descriptors) {
                const auto& descriptor = desc_pair.second;
                if (!descriptor) continue;

                // 설명자의 인터페이스 맵
                GVariantBuilder desc_interfaces_builder;
                g_variant_builder_init(&desc_interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));

                // 설명자 속성 맵
                GVariantBuilder desc_properties_builder;
                g_variant_builder_init(&desc_properties_builder, G_VARIANT_TYPE("a{sv}"));

                // UUID 속성
                g_variant_builder_add(&desc_properties_builder, "{sv}",
                                     "UUID",
                                     g_variant_new_string(descriptor->getUuid().toBlueZFormat().c_str()));

                // Characteristic 속성
                g_variant_builder_add(&desc_properties_builder, "{sv}",
                                     "Characteristic",
                                     g_variant_new_object_path(characteristic->getPath().c_str()));

                // Flags 속성 (문자열 배열)
                GVariantBuilder desc_flags_builder;
                g_variant_builder_init(&desc_flags_builder, G_VARIANT_TYPE("as"));

                // 설명자 플래그 추가
                uint8_t desc_perms = descriptor->getPermissions();
                if (desc_perms & GattPermission::PERM_READ)
                    g_variant_builder_add(&desc_flags_builder, "s", "read");
                if (desc_perms & GattPermission::PERM_WRITE)
                    g_variant_builder_add(&desc_flags_builder, "s", "write");
                if (desc_perms & GattPermission::PERM_READ_ENCRYPTED)
                    g_variant_builder_add(&desc_flags_builder, "s", "encrypt-read");
                if (desc_perms & GattPermission::PERM_WRITE_ENCRYPTED)
                    g_variant_builder_add(&desc_flags_builder, "s", "encrypt-write");
                if (desc_perms & GattPermission::PERM_READ_AUTHENTICATED)
                    g_variant_builder_add(&desc_flags_builder, "s", "auth-read");
                if (desc_perms & GattPermission::PERM_WRITE_AUTHENTICATED)
                    g_variant_builder_add(&desc_flags_builder, "s", "auth-write");

                g_variant_builder_add(&desc_properties_builder, "{sv}",
                                     "Flags",
                                     g_variant_builder_end(&desc_flags_builder));

                // 설명자 인터페이스 추가
                g_variant_builder_add(&desc_interfaces_builder, "{sa{sv}}",
                                     BlueZConstants::GATT_DESCRIPTOR_INTERFACE.c_str(),
                                     &desc_properties_builder);

                // 설명자 객체 추가
                g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}", 
                                     descriptor->getPath().c_str(),
                                     &desc_interfaces_builder);
            }
        }
    }

    // 결과 GVariant 생성
    GVariant* result = g_variant_builder_end(&objects_builder);
    Logger::info("Successfully created managed objects dictionary");
    
    // 디버깅 목적으로 출력
    char* debug_str = g_variant_print(result, TRUE);
    Logger::debug("Managed objects dictionary: " + std::string(debug_str));
    g_free(debug_str);
    
    // 스마트 포인터로 래핑하여 반환
    return GVariantPtr(result, &g_variant_unref);
}

} // namespace ggk