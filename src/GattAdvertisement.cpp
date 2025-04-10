#include "GattAdvertisement.h"
#include "Logger.h"
#include "Utils.h"

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
      registered(false) {
}

GattAdvertisement::GattAdvertisement(const DBusObjectPath& path)
    : DBusObject(DBusName::getInstance().getConnection(), path),
      type(AdvertisementType::PERIPHERAL),
      appearance(0),
      duration(0),
      includeTxPower(false),
      registered(false) {
}

void GattAdvertisement::addServiceUUID(const GattUuid& uuid) {
    if (!uuid.toString().empty()) {
        for (const auto& existingUuid : serviceUUIDs) {
            if (existingUuid.toString() == uuid.toString()) {
                return; // UUID가 이미 존재함
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
    Logger::debug(SSTR << "Set manufacturer data for ID 0x" << Utils::hex(manufacturerId) << " with " << data.size() << " bytes");
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

void GattAdvertisement::setAppearance(uint16_t value) {
    appearance = value;
    Logger::debug(SSTR << "Set appearance: 0x" << Utils::hex(value));
}

void GattAdvertisement::setDuration(uint16_t value) {
    duration = value;
    Logger::debug(SSTR << "Set advertisement duration: " << value << " seconds");
}

void GattAdvertisement::setIncludeTxPower(bool include) {
    includeTxPower = include;
    Logger::debug(SSTR << "Set include TX power: " << (include ? "true" : "false"));
}

bool GattAdvertisement::setupDBusInterfaces() {
    // 이미 등록된 경우 재등록하지 않음
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
            "IncludeTxPower",
            "b",
            true,
            false,
            false,
            [this]() { return getIncludeTxPowerProperty(); },
            nullptr
        }
    };
    
    // LocalName이 설정되어 있는 경우에만 속성 추가
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
    
    // Appearance가 설정되어 있는 경우에만 속성 추가
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
    
    // Duration이 설정되어 있는 경우에만 속성 추가
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

    // 1. LE Advertisement 인터페이스 추가 - 반드시 registerObject() 호출 전에 수행
    if (!addInterface(BlueZConstants::LE_ADVERTISEMENT_INTERFACE, properties)) {
        Logger::error("Failed to add LEAdvertisement interface");
        return false;
    }
    
    // 2. Release 메서드 추가 - 반드시 registerObject() 호출 전에 수행
    if (!addMethod(BlueZConstants::LE_ADVERTISEMENT_INTERFACE, "Release", 
                  [this](const DBusMethodCall& call) { handleRelease(call); })) {
        Logger::error("Failed to add Release method");
        return false;
    }
    
    // 3. 객체 등록 - 모든 인터페이스와 메서드 추가 후에 마지막으로 수행
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
    
    // Release는 반환값이 없는 메서드
    g_dbus_method_invocation_return_value(call.invocation.get(), nullptr);
    
    // 실제로 해제 작업 수행
    registered = false;
}

bool GattAdvertisement::registerWithBlueZ() {
    try {
        // 1. Check if already registered
        if (registered) {
            Logger::info("Advertisement already registered with BlueZ");
            return true;
        }
        
        // 2. Ensure interfaces are set up with D-Bus
        if (!setupDBusInterfaces()) {
            Logger::error("Failed to setup advertisement D-Bus interfaces");
            return false;
        }
        
        // 3. Clean up any existing advertisements
        cleanupExistingAdvertisements();
        
        // 4. Verify BlueZ service is running
        int bluezStatus = system("systemctl is-active --quiet bluetooth.service");
        if (bluezStatus != 0) {
            Logger::error("BlueZ service is not active. Cannot register advertisement.");
            return false;
        }
        
        // 5. Register with BlueZ using D-Bus API
        if (registerWithDBusApi()) {
            Logger::info("Successfully registered advertisement via BlueZ DBus API");
            registered = true;
            return activateAdvertising();
        }
        
        // 6. Registration failed, try alternative methods
        Logger::warn("Failed to register advertisement via standard D-Bus API, trying alternatives");
        
        // Use direct HCI commands as fallback
        if (activateAdvertisingFallback()) {
            Logger::info("Advertisement activated via fallback methods");
            registered = true;
            return true;
        }
        
        Logger::error("All advertisement registration methods failed");
        return false;
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
    
    // 1. Build raw advertising data
    std::vector<uint8_t> adData = buildRawAdvertisingData();
    
    if (adData.empty()) {
        Logger::error("Failed to build advertising data");
        return false;
    }
    
    // 2. Log the advertising data for debugging
    Logger::debug("Raw advertising data: " + Utils::hex(adData.data(), adData.size()));
    
    // 3. Format HCI command for setting advertising data
    std::string hciCmd = "sudo hcitool -i hci0 cmd 0x08 0x0008";
    hciCmd += " " + std::to_string(adData.size());  // Length byte
    
    // Add each byte as hex
    for (uint8_t byte : adData) {
        char hex[4];
        sprintf(hex, " %02X", byte);
        hciCmd += hex;
    }
    
    // 4. Execute the command to set advertising data
    Logger::debug("Executing HCI command: " + hciCmd);
    if (system(hciCmd.c_str()) != 0) {
        Logger::error("Failed to set advertising data with HCI command");
        return false;
    }
    
    // 5. Enable advertising
    if (system("sudo hcitool -i hci0 cmd 0x08 0x000A 01") != 0) {
        Logger::error("Failed to enable advertising with HCI command");
        return false;
    }
    
    // 6. Set non-connectable advertising parameters for better visibility
    // Parameters: min interval, max interval, type (0=connectable, 3=non-connectable), etc.
    if (system("sudo hcitool -i hci0 cmd 0x08 0x0006 A0 00 A0 00 03 00 00 00 00 00 00 00 00 07 00") != 0) {
        Logger::warn("Failed to set advertising parameters - continuing anyway");
    }
    
    Logger::info("Advertisement activated via direct HCI commands");
    return true;
}

bool GattAdvertisement::cleanupExistingAdvertisements() {
    Logger::info("Cleaning up any existing advertisements");
    
    // 1. First try to disable advertising
    system("sudo hciconfig hci0 noleadv > /dev/null 2>&1");
    system("echo -e 'menu advertise\\noff\\nexit\\n' | bluetoothctl > /dev/null 2>&1");
    
    // 2. Try to remove all advertising instances (0-3)
    for (int i = 0; i < 4; i++) {
        std::string cmd = "echo -e 'remove-advertising " + 
                         std::to_string(i) + "\\n' | bluetoothctl > /dev/null 2>&1";
        system(cmd.c_str());
    }
    
    // 3. More aggressive cleanup via D-Bus API - get all advertisements and remove them
    try {
        // Get all managed objects
        GVariantPtr result = getConnection().callMethod(
            BlueZConstants::BLUEZ_SERVICE,
            DBusObjectPath(BlueZConstants::ROOT_PATH),
            BlueZConstants::OBJECT_MANAGER_INTERFACE,
            BlueZConstants::GET_MANAGED_OBJECTS,
            makeNullGVariantPtr(),
            "a{oa{sa{sv}}}"
        );
        
        if (result) {
            GVariantIter *iter1;
            g_variant_get(result.get(), "a{oa{sa{sv}}}", &iter1);
            
            const gchar *objPath;
            GVariant *interfaces;
            std::vector<std::string> advPaths;
            
            // Find all advertisement objects
            while (g_variant_iter_loop(iter1, "{&o@a{sa{sv}}}", &objPath, &interfaces)) {
                if (g_variant_lookup_value(interfaces, 
                                          BlueZConstants::LE_ADVERTISEMENT_INTERFACE.c_str(), 
                                          NULL)) {
                    advPaths.push_back(objPath);
                }
            }
            
            g_variant_iter_free(iter1);
            
            // Try to unregister each advertisement found
            for (const auto& path : advPaths) {
                Logger::debug("Attempting to unregister advertisement: " + path);
                
                try {
                    // Build parameters
                    GVariantBuilder builder;
                    g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);
                    g_variant_builder_add(&builder, "o", path.c_str());
                    GVariant* params = g_variant_builder_end(&builder);
                    GVariantPtr params_ptr(g_variant_ref_sink(params), &g_variant_unref);
                    
                    // Call UnregisterAdvertisement
                    getConnection().callMethod(
                        BlueZConstants::BLUEZ_SERVICE,
                        DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                        BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
                        BlueZConstants::UNREGISTER_ADVERTISEMENT,
                        std::move(params_ptr)
                    );
                } catch (...) {
                    // Ignore failures, just try to clean up as much as possible
                }
            }
        }
    } catch (...) {
        // Ignore any errors in the cleanup process
    }
    
    // Wait a moment for cleanup to take effect
    usleep(500000);  // 500ms
    
    return true;
}


bool GattAdvertisement::unregisterFromBlueZ() {
    try {
        // 등록되어 있지 않으면 성공으로 간주
        if (!registered) {
            Logger::debug("Advertisement not registered, nothing to unregister");
            return true;
        }
        
        Logger::info("Unregistering advertisement from BlueZ...");
        bool success = false;
        
        // 방법 1: DBus API를 통한 등록 해제
        try {
            // 더 간단한 방식으로 매개변수 생성
            GVariant* params = g_variant_new("(o)", getPath().c_str());
            GVariantPtr parameters(g_variant_ref_sink(params), &g_variant_unref);
            
            getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
                BlueZConstants::UNREGISTER_ADVERTISEMENT,
                std::move(parameters),
                "",
                5000  // 5초 타임아웃
            );
            
            success = true;
            Logger::info("Successfully unregistered advertisement via BlueZ DBus API");
        } catch (const std::exception& e) {
            // DoesNotExist 오류의 경우 이미 BlueZ에서 해제된 것으로 간주
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
        
        // 방법 2: bluetoothctl을 사용한 등록 해제 (방법 1이 실패한 경우)
        if (!success) {
            Logger::info("Trying to unregister advertisement via bluetoothctl...");
            if (system("echo -e 'menu advertise\\noff\\nexit\\n' | bluetoothctl > /dev/null 2>&1") == 0) {
                success = true;
                Logger::info("Advertisement disabled via bluetoothctl");
            }
        }
        
        // 방법 3: hciconfig를 사용한 등록 해제 (방법 1, 2가 실패한 경우)
        if (!success) {
            Logger::info("Trying to disable advertisement via hciconfig...");
            if (system("sudo hciconfig hci0 noleadv > /dev/null 2>&1") == 0) {
                success = true;
                Logger::info("Advertisement disabled via hciconfig");
            }
        }
        
        // 최후의 방법: BlueZ 광고 객체 명시적 삭제 시도
        if (!success) {
            Logger::info("Trying to remove all advertising instances...");
            system("echo -e 'remove-advertising 0\\nremove-advertising 1\\nremove-advertising 2\\nremove-advertising 3\\n' | bluetoothctl > /dev/null 2>&1");
            success = true;  // 성공 여부를 정확히 확인하기 어려움
        }
        
        // 로컬 상태 업데이트
        registered = false;
        
        return success;
    } catch (const std::exception& e) {
        Logger::error("Exception in unregisterFromBlueZ: " + std::string(e.what()));
        registered = false;  // 안전을 위해 로컬 상태 업데이트
        return false;
    } catch (...) {
        Logger::error("Unknown exception in unregisterFromBlueZ");
        registered = false;  // 안전을 위해 로컬 상태 업데이트
        return false;
    }
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
        
        return Utils::gvariantFromString(typeStr);
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
        
        return Utils::gvariantFromStringArray(uuidStrings);
    } catch (const std::exception& e) {
        Logger::error("Exception in getServiceUUIDsProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getManufacturerDataProperty() {
    try {
        if (manufacturerData.empty()) {
            // 유효한 빈 사전 형식 제공
            return g_variant_new_array(G_VARIANT_TYPE("{qv}"), NULL, 0);
        }
        
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{qv}"));
        
        for (const auto& pair : manufacturerData) {
            uint16_t id = pair.first;
            const std::vector<uint8_t>& data = pair.second;
            
            // 바이트 배열을 ay 형식으로 변환
            GVariant* bytes = g_variant_new_fixed_array(
                G_VARIANT_TYPE_BYTE,
                data.data(),
                data.size(),
                sizeof(uint8_t)
            );
            
            // 올바른 variant 래핑
            GVariant* dataVariant = g_variant_new_variant(bytes);
            
            g_variant_builder_add(&builder, "{qv}", id, dataVariant);
        }
        
        GVariant* result = g_variant_builder_end(&builder);
        
        // 디버깅: 형식 검증 
        char* debug_str = g_variant_print(result, TRUE);
        Logger::debug("ManufacturerData property: " + std::string(debug_str));
        g_free(debug_str);
        
        return result;
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
            
            // GVariant를 먼저 생성
            GVariant* dataVariant = Utils::gvariantFromByteArray(data.data(), data.size());
            
            // 그 다음 builder에 추가
            g_variant_builder_add(&builder, "{sv}", 
                                 uuid.toBlueZFormat().c_str(), 
                                 dataVariant);
            
            // dataVariant는 builder가 소유권을 가짐
        }
        
        return g_variant_builder_end(&builder);
    } catch (const std::exception& e) {
        Logger::error("Exception in getServiceDataProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getLocalNameProperty() {
    try {
        return Utils::gvariantFromString(localName);
    } catch (const std::exception& e) {
        Logger::error("Exception in getLocalNameProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getAppearanceProperty() {
    try {
        return g_variant_new_uint16(appearance);
    } catch (const std::exception& e) {
        Logger::error("Exception in getAppearanceProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getDurationProperty() {
    try {
        return g_variant_new_uint16(duration);
    } catch (const std::exception& e) {
        Logger::error("Exception in getDurationProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getIncludeTxPowerProperty() {
    try {
        return Utils::gvariantFromBoolean(includeTxPower);
    } catch (const std::exception& e) {
        Logger::error("Exception in getIncludeTxPowerProperty: " + std::string(e.what()));
        return nullptr;
    }
}

bool GattAdvertisement::cleanupExistingAdvertisements() {
    Logger::info("Cleaning up any existing advertisements...");
    
    // 1. 먼저 bluetoothctl을 통해 광고 중지 시도
    system("echo -e 'menu advertise\\noff\\nexit\\n' | bluetoothctl > /dev/null 2>&1");
    
    // 2. HCI 명령어로 광고 중지 시도
    system("sudo hciconfig hci0 noleadv > /dev/null 2>&1");
    
    // 3. 모든 광고 인스턴스 제거 시도
    bool success = (system("echo -e 'remove-advertising 0\\nremove-advertising 1\\nremove-advertising 2\\nremove-advertising 3\\n' | bluetoothctl > /dev/null 2>&1") == 0);
    
    // 적절한 대기 시간 적용
    usleep(500000);  // 500ms
    
    return success;
}

bool GattAdvertisement::registerWithDBusApi() {
    // 최대 3번의 재시도, 각 시도마다 대기 시간 증가
    bool success = false;
    std::string errorMsg;
    
    for (int retryCount = 0; retryCount < MAX_REGISTRATION_RETRIES; retryCount++) {
        try {
            // 빈 딕셔너리를 Utils 헬퍼 함수를 사용해 생성
            GVariantPtr emptyOptions = Utils::createEmptyDictionaryPtr();
            
            // 튜플 형태로 파라미터 생성
            GVariant* tuple = g_variant_new("(o@a{sv})", 
                                        getPath().c_str(), 
                                        emptyOptions.get());
            
            // 튜플에 대한 참조 관리
            GVariant* owned_tuple = g_variant_ref_sink(tuple);
            GVariantPtr parameters(owned_tuple, &g_variant_unref);
            
            // BlueZ 호출
            GVariantPtr result = getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
                BlueZConstants::REGISTER_ADVERTISEMENT,
                std::move(parameters),
                "",
                5000
            );
            
            // 호출이 성공하면
            registered = true;
            return true;
        }
        catch (const std::exception& e) {
            errorMsg = e.what();
            
            if (errorMsg.find("Maximum advertisements reached") != std::string::npos) {
                Logger::error("Maximum advertisements reached. Attempting to clean up...");
                
                // 광고 정리 시도
                cleanupExistingAdvertisements();
                
                // BlueZ 종료 후 재시작 시도 (마지막 시도에서만)
                if (retryCount == MAX_REGISTRATION_RETRIES - 1) {
                    Logger::info("Trying to restart BlueZ to clear advertisements...");
                    system("sudo systemctl restart bluetooth.service");
                    sleep(2);  // 재시작 대기 시간
                }
            } else {
                Logger::error("DBus method failed: " + errorMsg);
            }
            
            // 재시도 전 대기
            int waitTime = BASE_RETRY_WAIT_MS * (1 << retryCount);  // Exponential backoff
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

std::string GattAdvertisement::getAdvertisementStateString() const {
    std::ostringstream oss;
    
    oss << "Advertisement State:\n";
    oss << "  Path: " << getPath().toString() << "\n";
    oss << "  Type: " << (type == AdvertisementType::PERIPHERAL ? "Peripheral" : "Broadcast") << "\n";
    oss << "  Registered: " << (registered ? "Yes" : "No") << "\n";
    oss << "  D-Bus Object Registered: " << (isRegistered() ? "Yes" : "No") << "\n";
    oss << "  Local Name: " << (localName.empty() ? "[None]" : localName) << "\n";
    oss << "  Include TX Power: " << (includeTxPower ? "Yes" : "No") << "\n";
    
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

std::vector<uint8_t> GattAdvertisement::buildRawAdvertisingData() const {
    std::vector<uint8_t> adData;
    
    // 1. Flags - Always include (required by BLE spec)
    const uint8_t flags = 0x06;  // LE General Discoverable, BR/EDR not supported
    adData.push_back(2);         // Length (flags + type)
    adData.push_back(0x01);      // Flags type
    adData.push_back(flags);     // Flags value
    
    // 2. Complete Local Name (if set)
    if (!localName.empty()) {
        adData.push_back(static_cast<uint8_t>(localName.length() + 1));  // Length
        adData.push_back(0x09);                                          // Complete Local Name type
        adData.insert(adData.end(), localName.begin(), localName.end());  // Name
    }
    
    // 3. TX Power (if enabled)
    if (includeTxPower) {
        adData.push_back(2);     // Length
        adData.push_back(0x0A);  // TX Power type
        adData.push_back(0x00);  // 0 dBm (default)
    }
    
    // 4. Service UUIDs (up to space limit)
    if (!serviceUUIDs.empty()) {
        // For simplicity, we'll use 16-bit UUID format if available
        std::vector<uint8_t> uuidBytes;
        for (const auto& uuid : serviceUUIDs) {
            // Extract 16-bit UUID if possible, otherwise skip
            std::string blueZFormat = uuid.toBlueZFormat();
            if (blueZFormat.size() == 32 && blueZFormat.substr(0, 8) == "0000" && 
                blueZFormat.substr(12) == "00001000800000805f9b34fb") {
                // Extract 16-bit part
                uint16_t shortUuid;
                std::istringstream iss(blueZFormat.substr(8, 4));
                iss >> std::hex >> shortUuid;
                
                uuidBytes.push_back(shortUuid & 0xFF);
                uuidBytes.push_back((shortUuid >> 8) & 0xFF);
            }
        }
        
        if (!uuidBytes.empty()) {
            adData.push_back(static_cast<uint8_t>(uuidBytes.size() + 1));  // Length
            adData.push_back(0x03);  // Complete List of 16-bit Service UUIDs
            adData.insert(adData.end(), uuidBytes.begin(), uuidBytes.end());
        }
    }
    
    // 5. Manufacturer Data (if any)
    for (const auto& pair : manufacturerData) {
        const uint16_t id = pair.first;
        const auto& data = pair.second;
        
        if (data.size() > 25) continue;  // Skip if too large (max AD size minus headers)
        
        adData.push_back(static_cast<uint8_t>(data.size() + 3));  // Length (data + type + 2 bytes ID)
        adData.push_back(0xFF);                                  // Manufacturer Data type
        adData.push_back(id & 0xFF);                             // Manufacturer ID (LSB)
        adData.push_back((id >> 8) & 0xFF);                      // Manufacturer ID (MSB)
        adData.insert(adData.end(), data.begin(), data.end());   // Data
    }
    
    return adData;
}

} // namespace ggk