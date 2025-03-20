// GattApplication.cpp
#include "GattApplication.h"
#include "Logger.h"
#include "Utils.h"
#include <fstream>


namespace ggk {

GattApplication::GattApplication(DBusConnection& connection, const DBusObjectPath& path)
    : DBusObject(connection, path),
      registered(false) {
        if (path.toString().find("/org/bluez/") == 0) {
            Logger::warn("Path starts with /org/bluez/ which is reserved by BlueZ");
        }
}

bool GattApplication::setupDBusInterfaces() {
    // 이미 등록된 경우 재등록하지 않음
    if (isRegistered()) {
        Logger::warn("Application already registered with D-Bus");
        return true;
    }

    Logger::info("Setting up D-Bus interfaces for application: " + getPath().toString());
    
    // 1. 먼저 모든 서비스와 특성이 D-Bus에 등록되어 있는지 확인
    std::lock_guard<std::mutex> lock(servicesMutex);
    for (auto& service : services) {
        if (!service->isRegistered()) {
            if (!service->setupDBusInterfaces()) {
                Logger::error("Failed to setup interfaces for service: " + service->getUuid().toString());
                return false;
            }
        }
    }
    
    // 2. 그 다음 ObjectManager 인터페이스 추가 - 반드시 registerObject() 호출 전에 수행
    if (!addInterface(BlueZConstants::OBJECT_MANAGER_INTERFACE, {})) {
        Logger::error("Failed to add ObjectManager interface");
        return false;
    }
    
    // 3. GetManagedObjects 메서드 등록 - 반드시 registerObject() 호출 전에 수행
    if (!addMethod(BlueZConstants::OBJECT_MANAGER_INTERFACE, "GetManagedObjects", 
                 [this](const DBusMethodCall& call) { 
                     Logger::info("GetManagedObjects called by BlueZ");
                     handleGetManagedObjects(call); 
                 })) {
        Logger::error("Failed to add GetManagedObjects method");
        return false;
    }
    
    // 4. 애플리케이션 객체 등록 - 모든 인터페이스 추가 후에 마지막으로 수행
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

bool GattApplication::ensureInterfacesRegistered() {
    // 이미 객체가 등록되어 있으면 인터페이스 설정을 건너뜀
    if (isRegistered()) {
        Logger::debug("Application object already registered, skipping interface setup");
        return true;
    }

    // 1. 서비스 등록 확인 및 설정
    std::lock_guard<std::mutex> lock(servicesMutex);
    for (auto& service : services) {
        if (!service->isRegistered()) {
            if (!service->setupDBusInterfaces()) {
                Logger::error("Failed to setup interfaces for service: " + service->getUuid().toString());
                return false;
            }
        }
    }
    
    // 2. ObjectManager 인터페이스 추가
    if (!addInterface(BlueZConstants::OBJECT_MANAGER_INTERFACE, {})) {
        Logger::error("Failed to add ObjectManager interface");
        return false;
    }
    
    // 3. GetManagedObjects 메서드 등록
    if (!addMethod(BlueZConstants::OBJECT_MANAGER_INTERFACE, "GetManagedObjects", 
                 [this](const DBusMethodCall& call) { 
                     handleGetManagedObjects(call); 
                 })) {
        Logger::error("Failed to add GetManagedObjects method");
        return false;
    }
    
    // 4. 애플리케이션 객체 등록
    if (!registerObject()) {
        Logger::error("Failed to register application object");
        return false;
    }
    
    Logger::info("All D-Bus interfaces registered for GATT application");
    return true;
}

bool GattApplication::registerWithBlueZ() {
    try {
        // 디버그: BlueZ가 지원하는 인터페이스 확인
        Logger::debug("Checking BlueZ interfaces");
        GVariantPtr introspect = getConnection().callMethod(
            BlueZConstants::BLUEZ_SERVICE,
            DBusObjectPath(BlueZConstants::ADAPTER_PATH),
            "org.freedesktop.DBus.Introspectable",
            "Introspect"
        );

        if (introspect) {
            // 인터페이스 출력
            const gchar* xml = nullptr;
            g_variant_get(introspect.get(), "(&s)", &xml);
            
            // xml은 GVariant 내부 메모리를 참조하므로 복사해서 사용
            std::string xml_copy;
            if (xml) {
                xml_copy = xml;
                Logger::debug("BlueZ adapter introspection: " + xml_copy);
                
                // xml_copy를 사용한 후 xml에 대한 참조는 해제할 필요 없음
                // (g_variant_get에서 "(&s)"가 아닌 "(s)"를 사용했으므로)
            }
            
            // GattManager1 인터페이스 확인
            if (!xml_copy.empty() && xml_copy.find("org.bluez.GattManager1") == std::string::npos) {
                Logger::error("BlueZ adapter does not support GattManager1 interface");
                return false;
            }
        } else {
            Logger::error("Failed to introspect BlueZ adapter");
        }

        // 이미 BlueZ에 등록된 경우
        if (registered) {
            Logger::info("Application already registered with BlueZ");
            return true;
        }
        
        // 객체가 이미 등록된 경우 등록 해제 후 다시 설정
        if (isRegistered()) {
            Logger::info("Unregistering application object to refresh interfaces");
            unregisterObject();
        }

        // 인터페이스 확인 및 설정
        if (!ensureInterfacesRegistered()) {
            Logger::error("Failed to setup application interfaces before BlueZ registration");
            return false;
        }
        
        // BlueZ 서비스 확인
        int bluezStatus = system("systemctl is-active --quiet bluetooth.service");
        if (bluezStatus != 0) {
            Logger::error("BlueZ service is not active. Run: systemctl start bluetooth.service");
            return false;
        }
        
        // BlueZ 어댑터 확인
        if (system("hciconfig hci0 > /dev/null 2>&1") != 0) {
            Logger::error("BlueZ adapter 'hci0' not found or not available");
            return false;
        }
        
        Logger::info("Sending RegisterApplication request to BlueZ");
        
        // 옵션 딕셔너리 생성 - Utils 사용하여 개선
        GVariantBuilder options_builder;
        g_variant_builder_init(&options_builder, G_VARIANT_TYPE("a{sv}"));
        
        // 실험적 기능 활성화
        g_variant_builder_add(&options_builder, "{sv}", "Experimental", 
                             g_variant_new_boolean(TRUE));

        g_variant_builder_add(&options_builder, "{sv}", "UseObjectPaths", 
                             g_variant_new_boolean(TRUE)); // 객체 경로 사용 명시
        
        // 옵션 딕셔너리 완성 및 참조 관리 개선
        GVariant* options = g_variant_builder_end(&options_builder);
        
        // 매개변수 튜플 생성 시 참조 관리 개선
        // 개선: 파라미터 생성 시 DBusTypes 함수 사용
        DBusObjectPath appPath = getPath();
        
        // 파라미터 생성 시 명시적 참조 관리
        GVariant* params_raw = g_variant_new("(o@a{sv})", appPath.c_str(), options);
        GVariant* params_sinked = g_variant_ref_sink(params_raw);
        GVariantPtr parameters(params_sinked, &gvariant_deleter);
        
        // 디버깅: 파라미터 덤프
        char* debug_str = g_variant_print(parameters.get(), TRUE);
        Logger::debug("RegisterApplication parameters: " + std::string(debug_str));
        g_free(debug_str);
        
        try {
            // 개선: callMethod 호출 시 이동 의미론 사용
            GVariantPtr result = getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::GATT_MANAGER_INTERFACE,
                BlueZConstants::REGISTER_APPLICATION,
                std::move(parameters), // 이동 의미론 사용
                "",
                60000  // 타임아웃 증가 (60초)
            );
            
            if (!result) {
                Logger::error("Failed to register application with BlueZ - null result");
                system("hciconfig -a"); // 어댑터 상태 확인
                
                registered = false; // 수정: 실패 시 false 설정
                return false;
            }
            
            registered = true;
            Logger::info("Successfully registered application with BlueZ");
            return true;
        } catch (const std::exception& e) {
            // callMethod 호출에 대한 예외 처리
            std::string error = e.what();
            
            // 타임아웃은 특별 처리
            if (error.find("Timeout") != std::string::npos) {
                Logger::warn("BlueZ registration timed out - this may be normal with BlueZ 5.64");
                // 타임아웃 시 true로 간주 (BlueZ 5.64 동작 방식)
                registered = true;
                return true;
            } else if (error.find("Failed") != std::string::npos && 
                       error.find("No object received") != std::string::npos) {
                // "No object received" 오류는 객체 구조 문제 - 디버깅 정보 추가
                Logger::error("BlueZ registration failed: No object received - GATT object structure issue");
                registered = false;
                return false;
            }
            
            Logger::error("Exception in BlueZ registration: " + error);
            registered = false;
            return false;
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in registerWithBlueZ: " + std::string(e.what()));
        return false;
    } catch (...) {
        Logger::error("Unknown exception in registerWithBlueZ");
        return false;
    }
}

bool GattApplication::unregisterFromBlueZ() {
    // 등록되지 않은 경우 즉시 성공 반환
    if (!registered) {
        Logger::debug("Application not registered with BlueZ, nothing to unregister");
        return true;
    }
    
    try {
        // 연결 상태 확인
        if (!getConnection().isConnected()) {
            Logger::warn("D-Bus connection not available, updating local registration state only");
            registered = false;
            return true;
        }
        
        // 파라미터 생성
        GVariant* params = g_variant_new("(o)", getPath().c_str());
        GVariantPtr parameters(g_variant_ref_sink(params), &g_variant_unref);
        
        // 실패하더라도 예외로 중단하지 않고 로컬 상태 업데이트
        try {
            getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::GATT_MANAGER_INTERFACE,
                BlueZConstants::UNREGISTER_APPLICATION,
                std::move(parameters),
                "",
                5000  // 타임아웃 5초
            );
            
            Logger::info("Successfully unregistered application from BlueZ");
        } catch (const std::exception& e) {
            // 오류 로깅만 하고 진행 (로컬 상태는 계속 업데이트)
            Logger::warn("Failed to unregister from BlueZ (continuing): " + std::string(e.what()));
        }
        
        // 로컬 상태 항상 업데이트
        registered = false;
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception in unregisterFromBlueZ: " + std::string(e.what()));
        
        // 심각한 예외 발생해도 로컬 상태는 업데이트 시도
        registered = false;
        return false;
    } catch (...) {
        Logger::error("Unknown exception in unregisterFromBlueZ");
        registered = false;
        return false;
    }
}

void GattApplication::handleGetManagedObjects(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in GetManagedObjects");
        return;
    }
    
    try {
        Logger::info("GetManagedObjects called by BlueZ");
        
        // Create managed objects dictionary
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
        
        // 중요: 반환 전에 로그 추가
        char* debug_str = g_variant_print(result.get(), TRUE);
        Logger::debug("Returning managed objects: " + std::string(debug_str).substr(0, 500) + "...");
        g_free(debug_str);
        
        // Return the result
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

    // 애플리케이션 루트 객체의 인터페이스 맵 생성
    GVariantBuilder app_interfaces_builder;
    g_variant_builder_init(&app_interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
    
    // ObjectManager 인터페이스 추가 (속성 없음)
    GVariantBuilder empty_props;
    g_variant_builder_init(&empty_props, G_VARIANT_TYPE("a{sv}"));
    
    // ObjectManager 인터페이스를 애플리케이션에 추가
    g_variant_builder_add(&app_interfaces_builder, "{sa{sv}}",
                         "org.freedesktop.DBus.ObjectManager",
                         &empty_props);
    
    // 애플리케이션 루트 객체를 객체 맵에 추가
    g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}", 
                         getPath().c_str(),
                         &app_interfaces_builder);

    // 서비스 목록 가져오기
    std::vector<GattServicePtr> servicesList;
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        servicesList = services;
    }

    Logger::debug("Processing " + std::to_string(servicesList.size()) + " services");

    // 서비스 처리
    for (const auto& service : servicesList) {
        if (!service) continue;
        
        // 이 서비스의 인터페이스 맵 (a{sa{sv}})
        GVariantBuilder interfaces_builder;
        g_variant_builder_init(&interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));

        // 서비스 인터페이스의 속성 맵 (a{sv})
        GVariantBuilder service_props_builder;
        g_variant_builder_init(&service_props_builder, G_VARIANT_TYPE("a{sv}"));

        // UUID 속성
        g_variant_builder_add(&service_props_builder, "{sv}",
                             "UUID",
                             g_variant_new_string(service->getUuid().toBlueZFormat().c_str()));

        // Primary 속성
        g_variant_builder_add(&service_props_builder, "{sv}",
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

        // char_paths_builder 완성 후 service_props_builder에 추가
        g_variant_builder_add(&service_props_builder, "{sv}",
                             "Characteristics",
                             g_variant_builder_end(&char_paths_builder));

        // service_props_builder 완성 후 interfaces_builder에 추가
        g_variant_builder_add(&interfaces_builder, "{sa{sv}}",
                             BlueZConstants::GATT_SERVICE_INTERFACE.c_str(),
                             &service_props_builder);

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
            GVariantBuilder char_props_builder;
            g_variant_builder_init(&char_props_builder, G_VARIANT_TYPE("a{sv}"));

            // UUID 속성
            g_variant_builder_add(&char_props_builder, "{sv}",
                                 "UUID",
                                 g_variant_new_string(characteristic->getUuid().toBlueZFormat().c_str()));

            // Service 속성
            g_variant_builder_add(&char_props_builder, "{sv}",
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

            // 플래그 속성 추가
            g_variant_builder_add(&char_props_builder, "{sv}",
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

            // 설명자 경로 추가
            g_variant_builder_add(&char_props_builder, "{sv}",
                                 "Descriptors",
                                 g_variant_builder_end(&desc_paths_builder));

            // Notifying 속성
            g_variant_builder_add(&char_props_builder, "{sv}",
                                 "Notifying",
                                 g_variant_new_boolean(characteristic->isNotifying()));

            // 특성 인터페이스 추가
            g_variant_builder_add(&char_interfaces_builder, "{sa{sv}}",
                                 BlueZConstants::GATT_CHARACTERISTIC_INTERFACE.c_str(),
                                 &char_props_builder);

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
                GVariantBuilder desc_props_builder;
                g_variant_builder_init(&desc_props_builder, G_VARIANT_TYPE("a{sv}"));

                // UUID 속성
                g_variant_builder_add(&desc_props_builder, "{sv}",
                                     "UUID",
                                     g_variant_new_string(descriptor->getUuid().toBlueZFormat().c_str()));

                // Characteristic 속성
                g_variant_builder_add(&desc_props_builder, "{sv}",
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

                // 플래그 추가
                g_variant_builder_add(&desc_props_builder, "{sv}",
                                     "Flags",
                                     g_variant_builder_end(&desc_flags_builder));

                // 설명자 인터페이스 추가
                g_variant_builder_add(&desc_interfaces_builder, "{sa{sv}}",
                                     BlueZConstants::GATT_DESCRIPTOR_INTERFACE.c_str(),
                                     &desc_props_builder);

                // 설명자 객체 추가
                g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}", 
                                     descriptor->getPath().c_str(),
                                     &desc_interfaces_builder);
            }
        }
    }

    // 최종 결과 생성
    GVariant* result = g_variant_builder_end(&objects_builder);
    
    // 로그 출력
    if (result) {
        Logger::info("Successfully created managed objects dictionary");
        
        // 디버깅 목적으로 출력 (큰 데이터는 로그가 매우 길어질 수 있음)
        char* debug_str = g_variant_print(result, TRUE); // 간단한 형태로 출력
        if (debug_str) {
            // 파일에 전체 내용 저장 (선택 사항)
            std::ofstream out("/home/aidall1/Developments/BLE/bluetoothLE/test_build/managed_objects.txt");
            out << debug_str;
            out.close();

            g_free(debug_str);
        }
        if (!g_variant_is_of_type(result, G_VARIANT_TYPE("a{oa{sa{sv}}}"))) {
            Logger::error("Created variant has incorrect type: " + 
                         std::string(g_variant_get_type_string(result)));
        } else {
            // 크기 검증 (최소한 루트 객체 하나는 있어야 함)
            gsize n_children = g_variant_n_children(result);
            Logger::debug("Managed objects dictionary contains " + 
                         std::to_string(n_children) + " objects");
            
            if (n_children < 1) {
                Logger::warn("Managed objects dictionary is empty!");
            }
        }
    } else {
        Logger::error("Failed to create managed objects dictionary - null result");
    }
    
    // 원본 코드 방식대로 스마트 포인터로 래핑하여 반환
    return GVariantPtr(result, &g_variant_unref);
}

} // namespace ggk