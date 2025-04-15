// src/GattAdvertisement.cpp
#include "BlueZUtils.h"
#include "GattAdvertisement.h"
#include "Logger.h"
#include "Utils.h"
#include <sstream>
#include <iomanip>


namespace ggk {

GattAdvertisement::GattAdvertisement(
    DBusConnection& connection,
    const DBusObjectPath& path,
    AdvertisementType type)
    : DBusObject(connection, path),
      type(type),
      appearance(0),
      duration(0),
      includeTxPower(false),
      discoverable(true), 
      registered(false) {
}

GattAdvertisement::GattAdvertisement(const DBusObjectPath& path)
    : DBusObject(DBusName::getInstance().getConnection(), path),
      type(AdvertisementType::PERIPHERAL),
      appearance(0),
      duration(0),
      includeTxPower(false),
      discoverable(true),    // 기본값은 true로 설정
      registered(false) {
}

void GattAdvertisement::addServiceUUID(const GattUuid& uuid) {
    if (!uuid.toString().empty()) {
        for (const auto& existingUuid : serviceUUIDs) {
            if (existingUuid.toString() == uuid.toString()) {
                return; // UUID already exists
            }
        }
        serviceUUIDs.push_back(uuid);
        Logger::debug("Added service UUID to advertisement: " + uuid.toString());
    }
}

void GattAdvertisement::addServiceUUIDs(const std::vector<GattUuid>& uuids) {
    for (const auto& uuid : uuids) {
        addServiceUUID(uuid);
    }
}

void GattAdvertisement::setManufacturerData(uint16_t manufacturerId, const std::vector<uint8_t>& data) {
    manufacturerData[manufacturerId] = data;
    Logger::debug("Set manufacturer data for ID 0x" + Utils::hex(manufacturerId) + " with " + 
                 std::to_string(data.size()) + " bytes");
}

void GattAdvertisement::setServiceData(const GattUuid& serviceUuid, const std::vector<uint8_t>& data) {
    if (!serviceUuid.toString().empty()) {
        serviceData[serviceUuid] = data;
        Logger::debug("Set service data for UUID: " + serviceUuid.toString());
    }
}

void GattAdvertisement::setLocalName(const std::string& name) {
    localName = name;
    Logger::debug("Set local name: " + name);
}

void GattAdvertisement::setDiscoverable(bool value) {
    discoverable = value;
    Logger::debug("Set discoverable: " + std::string(value ? "true" : "false"));
}

void GattAdvertisement::setAppearance(uint16_t value) {
    appearance = value;
    Logger::debug("Set appearance: 0x" + Utils::hex(value));
}

void GattAdvertisement::setDuration(uint16_t value) {
    duration = value;
    Logger::debug("Set advertisement duration: " + std::to_string(value) + " seconds");
}

void GattAdvertisement::setIncludeTxPower(bool include) {
    includeTxPower = include;
    Logger::debug("Set include TX power: " + std::string(include ? "true" : "false"));
}

bool GattAdvertisement::setupDBusInterfaces() {
    // 이미 등록되었는지 확인
    if (isRegistered()) {
        Logger::warn("Advertisement already registered with D-Bus");
        return true;
    }
    
    Logger::info("Setting up D-Bus interfaces for advertisement: " + getPath().toString());
    
    // 속성 정의
    std::vector<DBusProperty> properties = {
        {
            "Type",
            "s",
            true,
            false,
            false,
            [this]() { return getTypeProperty(); },
            nullptr
        },
        {
            "ServiceUUIDs",
            "as",
            true,
            false,
            false,
            [this]() { return getServiceUUIDsProperty(); },
            nullptr
        },
        {
            "ManufacturerData",
            "a{qv}",
            true,
            false,
            false,
            [this]() { return getManufacturerDataProperty(); },
            nullptr
        },
        {
            "ServiceData",
            "a{sv}",
            true,
            false,
            false,
            [this]() { return getServiceDataProperty(); },
            nullptr
        },
        {
            "Discoverable",
            "b",
            true,
            false,
            false,
            [this]() { return getDiscoverableProperty(); },
            nullptr
        },
        {
            "Includes",
            "as",
            true,
            false,
            false,
            [this]() { return getIncludesProperty(); },
            nullptr
        }
    };
    
    // 이전 버전 호환성을 위한 속성 추가
    if (includeTxPower) {
        properties.push_back({
            "IncludeTxPower",
            "b",
            true,
            false,
            false,
            [this]() { return getIncludeTxPowerProperty(); },
            nullptr
        });
    }
    
    // 선택적 속성 추가
    if (!localName.empty()) {
        properties.push_back({
            "LocalName",
            "s",
            true,
            false,
            false,
            [this]() { return getLocalNameProperty(); },
            nullptr
        });
    }
    
    if (appearance != 0) {
        properties.push_back({
            "Appearance",
            "q",
            true,
            false,
            false,
            [this]() { return getAppearanceProperty(); },
            nullptr
        });
    }
    
    if (duration != 0) {
        properties.push_back({
            "Duration",
            "q",
            true,
            false,
            false,
            [this]() { return getDurationProperty(); },
            nullptr
        });
    }

    // 인터페이스 추가
    if (!addInterface(BlueZConstants::LE_ADVERTISEMENT_INTERFACE, properties)) {
        Logger::error("Failed to add LEAdvertisement interface");
        return false;
    }
    
    // Release 메서드 추가
    if (!addMethod(BlueZConstants::LE_ADVERTISEMENT_INTERFACE, "Release", 
                  [this](const DBusMethodCall& call) { handleRelease(call); })) {
        Logger::error("Failed to add Release method");
        return false;
    }
    
    // 객체 등록
    if (!registerObject()) {
        Logger::error("Failed to register advertisement object");
        return false;
    }
    
    Logger::info("Registered LE Advertisement at: " + getPath().toString());
    return true;
}

void GattAdvertisement::handleRelease(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in Release");
        return;
    }
    
    Logger::info("BlueZ called Release on advertisement: " + getPath().toString());
    
    // Release has no return value
    g_dbus_method_invocation_return_value(call.invocation.get(), nullptr);
    
    // Update registration state
    registered = false;
}

bool GattAdvertisement::registerWithBlueZ() {
    try {
        Logger::info("Registering GATT advertisement with BlueZ");
        
        // 1. 이미 등록되었는지 확인
        if (registered) {
            Logger::info("Advertisement already registered with BlueZ");
            return true;
        }
        
        // 2. 인터페이스가 설정되었는지 확인
        if (!setupDBusInterfaces()) {
            Logger::error("Failed to setup advertisement D-Bus interfaces");
            return false;
        }
        
        // 3. 이전 광고 정리
        cleanupExistingAdvertisements();
        
        // 4. BlueZ 서비스 상태 확인
        int bluezStatus = system("systemctl is-active --quiet bluetooth.service");
        if (bluezStatus != 0) {
            Logger::error("BlueZ service is not active. Attempting to restart...");
            system("sudo systemctl restart bluetooth.service");
            sleep(2);
            
            if (system("systemctl is-active --quiet bluetooth.service") != 0) {
                Logger::error("Failed to restart BlueZ service");
                return false;
            }
            Logger::info("BlueZ service restarted successfully");
        }
        
        // 5. BlueZ 5.82 호환성을 위한 구성 확인
        
        // 6. D-Bus API로 등록 시도
        if (!registerWithDBusApi()) {
            Logger::warn("Failed to register advertisement via standard D-Bus API, trying alternatives");
            
            // 대체 방법으로 시도
            if (activateAdvertisingFallback()) {
                Logger::info("Advertisement activated via fallback methods");
                registered = true;
                return true;
            }
            
            Logger::error("All advertisement registration methods failed");
            return false;
        }
        
        Logger::info("Successfully registered advertisement with BlueZ");
        return true;
    }
    catch (const std::exception& e) {
        Logger::error("Exception in registerWithBlueZ: " + std::string(e.what()));
        return false;
    }
}

bool GattAdvertisement::activateAdvertising() {
    Logger::info("Activating BLE advertisement");
    
    // First try using hciconfig
    if (system("sudo hciconfig hci0 leadv 3 > /dev/null 2>&1") == 0) {
        Logger::info("Advertisement activated via hciconfig");
        return true;
    }
    
    // Next try using bluetoothctl
    if (system("echo -e 'menu advertise\\non\\nexit\\n' | bluetoothctl > /dev/null 2>&1") == 0) {
        Logger::info("Advertisement activated via bluetoothctl");
        return true;
    }
    
    // Finally try direct HCI commands
    return activateAdvertisingFallback();
}

bool GattAdvertisement::activateAdvertisingFallback() {
    Logger::info("Using direct HCI commands for advertising");
    
    // 1. 원시 광고 데이터 구성
    std::vector<uint8_t> adData = buildRawAdvertisingData();
    
    if (adData.empty()) {
        Logger::error("Failed to build advertising data");
        return false;
    }
    
    // 2. 디버깅용 광고 데이터 출력
    Logger::debug("Raw advertising data: " + Utils::hex(adData.data(), adData.size()));
    
    // 3. HCI 어댑터 재설정
    if (system("sudo hciconfig hci0 reset > /dev/null 2>&1") != 0) {
        Logger::warn("Failed to reset HCI adapter");
    }
    usleep(300000);  // 300ms 대기
    
    // 4. 광고 데이터 설정을 위한 HCI 명령 구성
    std::string hciCmd = "sudo hcitool -i hci0 cmd 0x08 0x0008";
    hciCmd += " " + std::to_string(adData.size());  // 길이 바이트
    
    // 각 바이트를 16진수로 추가
    for (uint8_t byte : adData) {
        char hex[4];
        sprintf(hex, " %02X", byte);
        hciCmd += hex;
    }
    
    // 5. 광고 데이터 설정 명령 실행
    Logger::debug("Executing HCI command: " + hciCmd);
    if (system(hciCmd.c_str()) != 0) {
        Logger::error("Failed to set advertising data with HCI command");
        return false;
    }
    
    // 6. 광고 활성화
    Logger::debug("Enabling advertising with HCI command");
    if (system("sudo hcitool -i hci0 cmd 0x08 0x000A 01") != 0) {
        Logger::error("Failed to enable advertising with HCI command");
        return false;
    }
    
    // 7. 광고 매개변수 설정 - 범용 비연결형 광고
    if (system("sudo hcitool -i hci0 cmd 0x08 0x0006 A0 00 A0 00 03 00 00 00 00 00 00 00 00 07 00") != 0) {
        Logger::warn("Failed to set advertising parameters - continuing anyway");
        // 실패해도 계속 진행
    }
    
    // 8. 4가지 방법으로 광고 활성화 시도
    bool success = false;
    
    // 방법 1: bluetoothctl 사용
    if (system("echo -e 'menu advertise\\non\\nexit\\n' | bluetoothctl > /dev/null 2>&1") == 0) {
        Logger::info("Advertising enabled via bluetoothctl");
        success = true;
    }
    
    // 방법 2: hciconfig 사용
    else if (system("sudo hciconfig hci0 leadv 3 > /dev/null 2>&1") == 0) {
        Logger::info("Advertising enabled via hciconfig");
        success = true;
    }
    
    // 방법 3: HCI 레벨 명령
    else if (system("sudo hcitool -i hci0 cmd 0x08 0x000A 01 > /dev/null 2>&1") == 0) {
        Logger::info("Advertising enabled via direct HCI commands");
        success = true;
    }
    
    // 방법 4: D-Bus 속성 설정 (discoverable 사용)
    else {
        try {
            // Powered 속성 설정
            GVariantPtr poweredValue = Utils::gvariantPtrFromBoolean(true);
            BlueZUtils::setAdapterProperty(
                getConnection(),
                "Powered",
                std::move(poweredValue),
                BlueZConstants::ADAPTER_PATH
            );
            
            // Discoverable 속성 설정
            GVariantPtr discValue = Utils::gvariantPtrFromBoolean(true);
            bool discResult = BlueZUtils::setAdapterProperty(
                getConnection(),
                "Discoverable",
                std::move(discValue),
                BlueZConstants::ADAPTER_PATH
            );
            
            if (discResult) {
                Logger::info("Advertising enabled via adapter discoverable property");
                success = true;
            }
        } catch (const std::exception& e) {
            Logger::error("Exception in D-Bus property method: " + std::string(e.what()));
        }
    }
    
    // 9. 방법 확인
    if (success) {
        Logger::info("Advertising activated successfully via fallback methods");
    } else {
        Logger::error("Failed to activate advertising via all fallback methods");
    }
    
    return success;
}

bool GattAdvertisement::cleanupExistingAdvertisements() {
    Logger::info("Cleaning up any existing advertisements");
    
    // 1. 먼저 광고 비활성화
    system("sudo hciconfig hci0 noleadv > /dev/null 2>&1");
    system("echo -e 'menu advertise\\noff\\nexit\\n' | bluetoothctl > /dev/null 2>&1");
    
    // 2. 모든 광고 인스턴스(0-7) 제거 시도
    for (int i = 0; i < 8; i++) {
        std::string cmd = "echo -e 'remove-advertising " + 
                         std::to_string(i) + "\\n' | bluetoothctl > /dev/null 2>&1";
        system(cmd.c_str());
    }
    
    // 3. D-Bus API를 통한 광고 정리
    try {
        // BlueZ의 모든 관리 객체 가져오기
        GVariantPtr result = getConnection().callMethod(
            BlueZConstants::BLUEZ_SERVICE,
            DBusObjectPath(BlueZConstants::ROOT_PATH),
            BlueZConstants::OBJECT_MANAGER_INTERFACE,
            BlueZConstants::GET_MANAGED_OBJECTS,
            makeNullGVariantPtr(),
            "a{oa{sa{sv}}}"  // 예상 반환 형식 (딕셔너리)
        );
        
        if (result) {
            // 결과 타입에 따라 처리
            if (g_variant_is_of_type(result.get(), G_VARIANT_TYPE("a{oa{sa{sv}}}"))) {
                // 모든 광고 객체 찾기
                GVariantIter iter1;
                g_variant_iter_init(&iter1, result.get());
                
                const gchar *objPath;
                GVariant *interfaces;
                std::vector<std::string> advPaths;
                
                while (g_variant_iter_next(&iter1, "{&o@a{sa{sv}}}", &objPath, &interfaces)) {
                    // LEAdvertisement1 인터페이스를 가진 객체 찾기
                    GVariant* adv_iface = g_variant_lookup_value(interfaces, 
                                                               BlueZConstants::LE_ADVERTISEMENT_INTERFACE.c_str(), 
                                                               G_VARIANT_TYPE("a{sv}"));
                    if (adv_iface) {
                        advPaths.push_back(objPath);
                        g_variant_unref(adv_iface);
                    }
                    
                    g_variant_unref(interfaces);
                }
                
                // 찾은 광고 등록 해제
                for (const auto& path : advPaths) {
                    Logger::debug("Unregistering advertisement: " + path);
                    
                    try {
                        GVariant* paramsRaw = g_variant_new("(o)", path.c_str());
                        GVariantPtr params = makeGVariantPtr(paramsRaw, true);
                        
                        getConnection().callMethod(
                            BlueZConstants::BLUEZ_SERVICE,
                            DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                            BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
                            BlueZConstants::UNREGISTER_ADVERTISEMENT,
                            std::move(params)
                        );
                    } catch (...) {
                        // 실패해도 계속 진행
                    }
                }
            } else {
                // 예상하지 못한 반환 형식
                Logger::warn("GetManagedObjects returned unexpected type: " + 
                            std::string(g_variant_get_type_string(result.get())));
            }
        }
    } catch (const std::exception& e) {
        Logger::warn("Failed to clean up advertisements via D-Bus API: " + std::string(e.what()));
        // 계속 진행
    }
    
    // 4. HCI 세션 초기화를 위한 대기
    usleep(500000);  // 500ms
    
    return true;
}

bool GattAdvertisement::unregisterFromBlueZ() {
    try {
        // If not registered, consider it a success
        if (!registered) {
            Logger::debug("Advertisement not registered, nothing to unregister");
            return true;
        }
        
        Logger::info("Unregistering advertisement from BlueZ...");
        bool success = false;
        
        // Method 1: Unregister via D-Bus API
        try {
            // Create parameters using makeGVariantPtr pattern
            GVariant* paramsRaw = g_variant_new("(o)", getPath().c_str());
            GVariantPtr params = makeGVariantPtr(paramsRaw, true);
            
            getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
                BlueZConstants::UNREGISTER_ADVERTISEMENT,
                std::move(params),
                "",
                5000  // 5 second timeout
            );
            
            success = true;
            Logger::info("Successfully unregistered advertisement via BlueZ DBus API");
        } catch (const std::exception& e) {
            // Ignore DoesNotExist errors
            std::string error = e.what();
            if (error.find("DoesNotExist") != std::string::npos || 
                error.find("Does Not Exist") != std::string::npos ||
                error.find("No such") != std::string::npos) {
                Logger::info("Advertisement already unregistered or not found in BlueZ");
                success = true;
            } else {
                Logger::warn("Failed to unregister via BlueZ API: " + error);
            }
        }
        
        // Method 2: Disable via bluetoothctl (if method 1 failed)
        if (!success) {
            Logger::info("Trying to unregister advertisement via bluetoothctl...");
            if (system("echo -e 'menu advertise\\noff\\nexit\\n' | bluetoothctl > /dev/null 2>&1") == 0) {
                success = true;
                Logger::info("Advertisement disabled via bluetoothctl");
            }
        }
        
        // Method 3: Disable via hciconfig (if methods 1 and 2 failed)
        if (!success) {
            Logger::info("Trying to disable advertisement via hciconfig...");
            if (system("sudo hciconfig hci0 noleadv > /dev/null 2>&1") == 0) {
                success = true;
                Logger::info("Advertisement disabled via hciconfig");
            }
        }
        
        // Last resort: Try to remove all advertising instances
        if (!success) {
            Logger::info("Trying to remove all advertising instances...");
            system("echo -e 'remove-advertising 0\\nremove-advertising 1\\nremove-advertising 2\\nremove-advertising 3\\n' | bluetoothctl > /dev/null 2>&1");
            success = true;  // Hard to verify success accurately
        }
        
        // Update local state
        registered = false;
        
        return success;
    } catch (const std::exception& e) {
        Logger::error("Exception in unregisterFromBlueZ: " + std::string(e.what()));
        registered = false;  // Update local state for safety
        return false;
    }
}

bool GattAdvertisement::registerWithDBusApi() {
    // 여러 번 시도, 증가하는 대기 시간
    bool success = false;
    std::string errorMsg;
    
    for (int retryCount = 0; retryCount < MAX_REGISTRATION_RETRIES; retryCount++) {
        try {
            // 1. 빈 옵션 딕셔너리 생성
            GVariantBuilder builder;
            g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
            
            // BlueZ 5.82 호환성을 위한 옵션
            // (필요한 경우 여기에 특별 옵션 추가)
            
            GVariant* options = g_variant_builder_end(&builder);
            
            // 2. 매개변수 튜플 생성
            GVariant* paramsRaw = g_variant_new("(oa{sv})", 
                                        getPath().c_str(), 
                                        options);
            
            // 3. 호출용 매개변수 래핑
            GVariantPtr params = makeGVariantPtr(paramsRaw, true);
            
            // 4. 매개변수 로그 (디버깅용)
            char* debug_str = g_variant_print(params.get(), TRUE);
            Logger::debug("RegisterAdvertisement parameters: " + std::string(debug_str));
            g_free(debug_str);
            
            // 5. BlueZ 메소드 호출
            Logger::info("Calling RegisterAdvertisement with path: " + getPath().toString());
            GVariantPtr result = getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
                BlueZConstants::REGISTER_ADVERTISEMENT,
                std::move(params),
                "",
                5000  // 5초 타임아웃
            );
            
            // 6. 성공!
            registered = true;
            return true;
        }
        catch (const std::exception& e) {
            errorMsg = e.what();
            
            // 7. 특별한 오류 처리
            if (errorMsg.find("Maximum advertisements reached") != std::string::npos) {
                Logger::error("Maximum advertisements reached. Attempting to clean up...");
                
                // 광고 정리 시도
                cleanupExistingAdvertisements();
                
                // 마지막 시도에서 BlueZ 재시작
                if (retryCount == MAX_REGISTRATION_RETRIES - 1) {
                    Logger::info("Trying to restart BlueZ to clear advertisements...");
                    system("sudo systemctl restart bluetooth.service");
                    sleep(2);  // 재시작 대기
                }
            } else {
                Logger::error("D-Bus method failed: " + errorMsg);
            }
            
            // 8. 재시도 전 대기
            int waitTime = BASE_RETRY_WAIT_MS * (1 << retryCount);  // 지수적 백오프
            Logger::info("Retry " + std::to_string(retryCount + 1) + 
                        " of " + std::to_string(MAX_REGISTRATION_RETRIES) + 
                        " in " + std::to_string(waitTime / 1000.0) + " seconds");
            usleep(waitTime * 1000);
        }
    }
    
    Logger::error("Failed to register advertisement after " + 
                 std::to_string(MAX_REGISTRATION_RETRIES) + " attempts. Last error: " + errorMsg);
    return false;
}


GVariant* GattAdvertisement::getTypeProperty() {
    try {
        const char* typeStr;
        
        switch (type) {
            case AdvertisementType::BROADCAST:
                typeStr = "broadcast";
                break;
            case AdvertisementType::PERIPHERAL:
            default:
                typeStr = "peripheral";
                break;
        }
        
        // Use makeGVariantPtr pattern
        GVariantPtr variantPtr = Utils::gvariantPtrFromString(typeStr);
        if (!variantPtr) {
            return nullptr;
        }
        // Transfer ownership
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getTypeProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getServiceUUIDsProperty() {
    try {
        std::vector<std::string> uuidStrings;
        
        for (const auto& uuid : serviceUUIDs) {
            uuidStrings.push_back(uuid.toBlueZFormat());
        }
        
        // Use makeGVariantPtr pattern
        GVariantPtr variantPtr = Utils::gvariantPtrFromStringArray(uuidStrings);
        if (!variantPtr) {
            return nullptr;
        }
        // Transfer ownership
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getServiceUUIDsProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getManufacturerDataProperty() {
    try {
        if (manufacturerData.empty()) {
            // Create valid empty dictionary
            GVariantBuilder builder;
            g_variant_builder_init(&builder, G_VARIANT_TYPE("a{qv}"));
            GVariant* result = g_variant_builder_end(&builder);
            return result;  // No need for ref/unref
        }
        
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{qv}"));
        
        for (const auto& pair : manufacturerData) {
            uint16_t id = pair.first;
            const std::vector<uint8_t>& data = pair.second;
            
            // Create byte array
            GVariant* bytes = g_variant_new_fixed_array(
                G_VARIANT_TYPE_BYTE,
                data.data(),
                data.size(),
                sizeof(uint8_t)
            );
            
            // Wrap in variant
            GVariant* dataVariant = g_variant_new_variant(bytes);
            
            g_variant_builder_add(&builder, "{qv}", id, dataVariant);
        }
        
        GVariant* result = g_variant_builder_end(&builder);
        
        // Debug validation
        char* debug_str = g_variant_print(result, TRUE);
        Logger::debug("ManufacturerData property: " + std::string(debug_str));
        g_free(debug_str);
        
        return result;  // No need for ref/unref
    } catch (const std::exception& e) {
        Logger::error("Exception in getManufacturerDataProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getServiceDataProperty() {
    try {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
        
        for (const auto& pair : serviceData) {
            const GattUuid& uuid = pair.first;
            const std::vector<uint8_t>& data = pair.second;
            
            // Create byte array variant
            GVariantPtr dataVariant = Utils::gvariantPtrFromByteArray(data.data(), data.size());
            
            // Add to builder
            g_variant_builder_add(&builder, "{sv}", 
                                 uuid.toBlueZFormat().c_str(), 
                                 dataVariant.get());
        }
        
        GVariant* result = g_variant_builder_end(&builder);
        return result;  // No need for ref/unref
    } catch (const std::exception& e) {
        Logger::error("Exception in getServiceDataProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getLocalNameProperty() {
    try {
        // Use makeGVariantPtr pattern
        GVariantPtr variantPtr = Utils::gvariantPtrFromString(localName);
        if (!variantPtr) {
            return nullptr;
        }
        // Transfer ownership
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getLocalNameProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getAppearanceProperty() {
    try {
        // Use direct GVariant creation for simple numeric type
        return g_variant_new_uint16(appearance);
    } catch (const std::exception& e) {
        Logger::error("Exception in getAppearanceProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getDurationProperty() {
    try {
        // Use direct GVariant creation for simple numeric type
        return g_variant_new_uint16(duration);
    } catch (const std::exception& e) {
        Logger::error("Exception in getDurationProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getIncludeTxPowerProperty() {
    try {
        // Use makeGVariantPtr pattern
        GVariantPtr variantPtr = Utils::gvariantPtrFromBoolean(includeTxPower);
        if (!variantPtr) {
            return nullptr;
        }
        // Transfer ownership
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getIncludeTxPowerProperty: " + std::string(e.what()));
        return nullptr;
    }
}

std::string GattAdvertisement::getAdvertisementStateString() const {
    std::stringstream oss;
    
    oss << "Advertisement State:\n";
    oss << "  Path: " << getPath().toString() << "\n";
    oss << "  Type: " << (type == AdvertisementType::PERIPHERAL ? "Peripheral" : "Broadcast") << "\n";
    oss << "  Registered: " << (registered ? "Yes" : "No") << "\n";
    oss << "  D-Bus Object Registered: " << (isRegistered() ? "Yes" : "No") << "\n";
    oss << "  Local Name: " << (localName.empty() ? "[None]" : localName) << "\n";
    oss << "  Discoverable: " << (discoverable ? "Yes" : "No") << "\n";
    oss << "  Include TX Power: " << (includeTxPower ? "Yes" : "No") << "\n";
    
    if (!includes.empty()) {
        oss << "  Includes:\n";
        for (const auto& item : includes) {
            oss << "    - " << item << "\n";
        }
    }
    
    if (!serviceUUIDs.empty()) {
        oss << "  Service UUIDs:\n";
        for (const auto& uuid : serviceUUIDs) {
            oss << "    - " << uuid.toString() << "\n";
        }
    }
    
    if (!manufacturerData.empty()) {
        oss << "  Manufacturer Data:\n";
        for (const auto& pair : manufacturerData) {
            oss << "    - ID: 0x" << Utils::hex(pair.first);
            oss << " (" << pair.second.size() << " bytes)\n";
        }
    }
    
    if (appearance != 0) {
        oss << "  Appearance: 0x" << Utils::hex(appearance) << "\n";
    }
    
    if (duration != 0) {
        oss << "  Duration: " << duration << " seconds\n";
    }
    
    return oss.str();
}

// src/GattAdvertisement.cpp (continued)
std::vector<uint8_t> GattAdvertisement::buildRawAdvertisingData() const {
    std::vector<uint8_t> adData;
    
    // 1. Flags - 항상 포함 (BLE 스펙에서 요구됨)
    const uint8_t flags = 0x06;  // LE General Discoverable, BR/EDR not supported
    adData.push_back(2);         // Length (flags + type)
    adData.push_back(0x01);      // Flags type
    adData.push_back(flags);     // Flags value
    
    // 2. Complete Local Name (설정된 경우)
    if (!localName.empty()) {
        adData.push_back(static_cast<uint8_t>(localName.length() + 1));  // Length
        adData.push_back(0x09);                                          // Complete Local Name type
        adData.insert(adData.end(), localName.begin(), localName.end());  // Name
    }
    
    // 3. TX Power (활성화된 경우)
    if (includeTxPower) {
        adData.push_back(2);     // Length
        adData.push_back(0x0A);  // TX Power type
        adData.push_back(0x00);  // 0 dBm (default)
    }
    
    // 4. Service UUIDs (16비트와 128비트 모두 지원)
    if (!serviceUUIDs.empty()) {
        // 16비트 및 128비트 UUID 분류
        std::vector<uint16_t> shortUuids;
        std::vector<std::vector<uint8_t>> fullUuids;

        for (const auto& uuid : serviceUUIDs) {
            std::string blueZFormat = uuid.toBlueZFormat();
            
            // 16비트 UUID 형식 확인 (0000xxxx-0000-1000-8000-00805F9B34FB)
            if (blueZFormat.size() == 32 && 
                blueZFormat.substr(0, 4) == "0000" &&
                blueZFormat.substr(8) == "00001000800000805f9b34fb") {
                
                // 16비트 부분 추출
                uint16_t shortUuid;
                std::istringstream iss(blueZFormat.substr(4, 4));
                iss >> std::hex >> shortUuid;
                
                shortUuids.push_back(shortUuid);
            }
            // 128비트 UUID 처리
            else {
                std::vector<uint8_t> fullUuidBytes;
                
                // 문자열 UUID를 바이트 배열로 변환
                for (size_t i = 0; i < blueZFormat.length(); i += 2) {
                    if (i + 1 < blueZFormat.length()) {
                        std::string byteStr = blueZFormat.substr(i, 2);
                        
                        // 수정된 부분: 올바른 16진수 문자열 파싱
                        int temp;
                        std::istringstream iss(byteStr);
                        iss >> std::hex >> temp;
                        
                        // 값 검증 (파싱 성공 및 범위 확인)
                        if (!iss.fail() && temp >= 0 && temp <= 255) {
                            fullUuidBytes.push_back(static_cast<uint8_t>(temp));
                        } else {
                            Logger::warn("Invalid hex value in UUID: " + byteStr);
                            // 기본값 0 추가
                            fullUuidBytes.push_back(0);
                        }
                    }
                }
                
                // 바이트가 16개인 경우만 추가 (128비트 = 16바이트)
                if (fullUuidBytes.size() == 16) {
                    fullUuids.push_back(fullUuidBytes);
                } else {
                    Logger::warn("Invalid 128-bit UUID byte length: " + std::to_string(fullUuidBytes.size()));
                }
            }
        }
        
        // 16비트 UUID 추가 (있는 경우)
        if (!shortUuids.empty()) {
            size_t dataSize = shortUuids.size() * 2;
            adData.push_back(static_cast<uint8_t>(dataSize + 1));  // Length
            adData.push_back(0x03);  // Complete List of 16-bit Service UUIDs
            
            for (uint16_t uuid : shortUuids) {
                adData.push_back(uuid & 0xFF);
                adData.push_back((uuid >> 8) & 0xFF);
            }
        }
        
        // 128비트 UUID 추가 (있고 공간이 남는 경우)
        // 주의: 광고 패킷은 총 31바이트로 제한됨
        const size_t MAX_AD_SIZE = 31;
        
        for (const auto& fullUuid : fullUuids) {
            // 패킷 크기 계산: 현재 크기 + UUID 길이(16) + 헤더(2)
            if (adData.size() + 18 <= MAX_AD_SIZE) {
                adData.push_back(17);  // Length (16 + type)
                adData.push_back(0x07);  // Complete List of 128-bit Service UUIDs
                adData.insert(adData.end(), fullUuid.begin(), fullUuid.end());
            } else {
                // 공간 부족으로 추가 불가
                Logger::warn("Advertising data too large to include all 128-bit UUIDs");
                break;
            }
        }
    }
    
    // 5. Manufacturer Data (있는 경우)
    for (const auto& pair : manufacturerData) {
        const uint16_t id = pair.first;
        const auto& data = pair.second;
        
        // 데이터가 너무 크면 건너뜀 (최대 AD 크기 31바이트 감안)
        if (data.size() > 25) continue;
        
        // 남은 공간 확인
        if (adData.size() + data.size() + 4 > 31) {
            Logger::warn("Skipping manufacturer data due to size limit");
            break;
        }
        
        adData.push_back(static_cast<uint8_t>(data.size() + 3));  // Length (data + type + 2 bytes ID)
        adData.push_back(0xFF);                                  // Manufacturer Data type
        adData.push_back(id & 0xFF);                             // Manufacturer ID (LSB)
        adData.push_back((id >> 8) & 0xFF);                      // Manufacturer ID (MSB)
        adData.insert(adData.end(), data.begin(), data.end());   // Data
    }
    
    return adData;
}

void GattAdvertisement::addInclude(const std::string& item) {
    // 이미 포함되어 있는지 확인
    for (const auto& existing : includes) {
        if (existing == item) {
            return; // 중복 방지
        }
    }
    
    includes.push_back(item);
    Logger::debug("Added include item: " + item);
    
    // 이전 방식과의 호환성을 위한 특수 처리
    if (item == "tx-power") {
        includeTxPower = true;
    }
}

void GattAdvertisement::setIncludes(const std::vector<std::string>& items) {
    includes.clear();
    for (const auto& item : items) {
        addInclude(item);
    }
    Logger::debug("Set includes array with " + std::to_string(includes.size()) + " items");
}

// 새 속성 게터 구현
GVariant* GattAdvertisement::getDiscoverableProperty() {
    try {
        // makeGVariantPtr 패턴 사용
        GVariantPtr variantPtr = Utils::gvariantPtrFromBoolean(discoverable);
        if (!variantPtr) {
            return nullptr;
        }
        // 소유권 이전
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getDiscoverableProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getIncludesProperty() {
    try {
        // BlueZ 5.82 버전 호환을 위해 이전 방식(IncludeTxPower)과 호환 유지
        std::vector<std::string> allIncludes = includes;
        
        // includeTxPower가 true이고 "tx-power"가 includes에 없으면 추가
        if (includeTxPower) {
            bool hasTxPower = false;
            for (const auto& item : allIncludes) {
                if (item == "tx-power") {
                    hasTxPower = true;
                    break;
                }
            }
            
            if (!hasTxPower) {
                allIncludes.push_back("tx-power");
            }
        }
        
        // appearance가 0이 아니고 "appearance"가 includes에 없으면 추가
        if (appearance != 0) {
            bool hasAppearance = false;
            for (const auto& item : allIncludes) {
                if (item == "appearance") {
                    hasAppearance = true;
                    break;
                }
            }
            
            if (!hasAppearance) {
                allIncludes.push_back("appearance");
            }
        }
        
        // localName이 비어있지 않고 "local-name"이 includes에 없으면 추가
        if (!localName.empty()) {
            bool hasLocalName = false;
            for (const auto& item : allIncludes) {
                if (item == "local-name") {
                    hasLocalName = true;
                    break;
                }
            }
            
            if (!hasLocalName) {
                allIncludes.push_back("local-name");
            }
        }
        
        // makeGVariantPtr 패턴 사용
        GVariantPtr variantPtr = Utils::gvariantPtrFromStringArray(allIncludes);
        if (!variantPtr) {
            return nullptr;
        }
        // 소유권 이전
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getIncludesProperty: " + std::string(e.what()));
        return nullptr;
    }
}

void GattAdvertisement::ensureBlueZ582Compatibility() {
    // 1. Includes 배열 확인
    bool hasTxPower = false;
    bool hasLocalName = false;
    bool hasAppearance = false;
    
    for (const auto& item : includes) {
        if (item == "tx-power") hasTxPower = true;
        else if (item == "local-name") hasLocalName = true;
        else if (item == "appearance") hasAppearance = true;
    }
    
    // 2. TxPower 포함 설정
    if (includeTxPower && !hasTxPower) {
        includes.push_back("tx-power");
        Logger::debug("Added tx-power to Includes array for BlueZ 5.82 compatibility");
    }
    
    // 3. LocalName 포함 설정
    if (!localName.empty() && !hasLocalName) {
        includes.push_back("local-name");
        Logger::debug("Added local-name to Includes array for BlueZ 5.82 compatibility");
    }
    
    // 4. Appearance 포함 설정
    if (appearance != 0 && !hasAppearance) {
        includes.push_back("appearance");
        Logger::debug("Added appearance to Includes array for BlueZ 5.82 compatibility");
    }
    
    // 5. 모든 서비스 UUID가 올바른 형식인지 확인
    std::vector<GattUuid> validUuids;
    for (const auto& uuid : serviceUUIDs) {
        if (!uuid.toString().empty()) {
            validUuids.push_back(uuid);
        } else {
            Logger::warn("Skipping invalid service UUID in advertisement");
        }
    }
    
    if (validUuids.size() != serviceUUIDs.size()) {
        serviceUUIDs = validUuids;
    }
    
    // 6. 광고 상태 로그
    Logger::debug("Advertisement configuration for BlueZ 5.82:");
    Logger::debug("- Type: " + std::string(type == AdvertisementType::PERIPHERAL ? "peripheral" : "broadcast"));
    Logger::debug("- LocalName: " + localName);
    Logger::debug("- ServiceUUIDs: " + std::to_string(serviceUUIDs.size()) + " UUIDs");
    
    std::string includesStr = "- Includes: ";
    for (const auto& item : includes) {
        includesStr += item + ", ";
    }
    if (!includes.empty()) {
        includesStr.pop_back();  // 마지막 콤마 제거
        includesStr.pop_back();
    }
    Logger::debug(includesStr);
}




} // namespace ggk