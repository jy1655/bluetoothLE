// src/GattAdvertisement.cpp
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
    // If already registered, don't re-register
    if (isRegistered()) {
        Logger::warn("Advertisement already registered with D-Bus");
        return true;
    }
    
    Logger::info("Setting up D-Bus interfaces for advertisement: " + getPath().toString());
    
    // Define properties
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
    
    // Add optional properties
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

    // 1. Add LEAdvertisement interface
    if (!addInterface(BlueZConstants::LE_ADVERTISEMENT_INTERFACE, properties)) {
        Logger::error("Failed to add LEAdvertisement interface");
        return false;
    }
    
    // 2. Add Release method
    if (!addMethod(BlueZConstants::LE_ADVERTISEMENT_INTERFACE, "Release", 
                  [this](const DBusMethodCall& call) { handleRelease(call); })) {
        Logger::error("Failed to add Release method");
        return false;
    }
    
    // 3. Register object
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
            ""
        );
        
        if (result) {
            // BlueZ 5.82 형식 확인 (튜플로 감싸진 딕셔너리)
            if (g_variant_is_of_type(result.get(), G_VARIANT_TYPE("(a{oa{sa{sv}}})"))) {
                Logger::debug("BlueZ 5.82 형식 감지: 튜플에서 딕셔너리 추출");
                // 튜플에서 딕셔너리 추출
                GVariant* extracted = g_variant_get_child_value(result.get(), 0);
                GVariantPtr extractedPtr = makeGVariantPtr(extracted, true);
                
                GVariantIter *iter1;
                g_variant_get(extractedPtr.get(), "a{oa{sa{sv}}}", &iter1);
                
                const gchar *objPath;
                GVariant *interfaces;
                std::vector<std::string> advPaths;
                
                // 모든 광고 객체 찾기
                while (g_variant_iter_loop(iter1, "{&o@a{sa{sv}}}", &objPath, &interfaces)) {
                    if (g_variant_lookup_value(interfaces, 
                                               BlueZConstants::LE_ADVERTISEMENT_INTERFACE.c_str(), 
                                               NULL)) {
                        advPaths.push_back(objPath);
                    }
                }
                
                g_variant_iter_free(iter1);
                
                // 찾은 각 광고 등록 해제 시도
                for (const auto& path : advPaths) {
                    Logger::debug("Attempting to unregister advertisement: " + path);
                    
                    try {
                        // 파라미터 빌드
                        GVariant* paramsRaw = g_variant_new("(o)", path.c_str());
                        GVariantPtr params = makeGVariantPtr(paramsRaw, true);
                        
                        // UnregisterAdvertisement 호출
                        getConnection().callMethod(
                            BlueZConstants::BLUEZ_SERVICE,
                            DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                            BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
                            BlueZConstants::UNREGISTER_ADVERTISEMENT,
                            std::move(params)
                        );
                    } catch (...) {
                        // 실패 무시, 가능한 한 많이 정리
                    }
                }
            } 
            // 기존 BlueZ 형식 (직접 딕셔너리)
            else if (g_variant_is_of_type(result.get(), G_VARIANT_TYPE("a{oa{sa{sv}}}"))) {
                GVariantIter *iter1;
                g_variant_get(result.get(), "a{oa{sa{sv}}}", &iter1);
                
                const gchar *objPath;
                GVariant *interfaces;
                std::vector<std::string> advPaths;
                
                // 모든 광고 객체 찾기
                while (g_variant_iter_loop(iter1, "{&o@a{sa{sv}}}", &objPath, &interfaces)) {
                    if (g_variant_lookup_value(interfaces,
                                              BlueZConstants::LE_ADVERTISEMENT_INTERFACE.c_str(),
                                              NULL)) {
                        advPaths.push_back(objPath);
                    }
                }
                
                g_variant_iter_free(iter1);
                
                // 찾은 각 광고 등록 해제 시도
                for (const auto& path : advPaths) {
                    Logger::debug("Attempting to unregister advertisement: " + path);
                    
                    try {
                        // 파라미터 빌드
                        GVariant* paramsRaw = g_variant_new("(o)", path.c_str());
                        GVariantPtr params = makeGVariantPtr(paramsRaw, true);
                        
                        // UnregisterAdvertisement 호출
                        getConnection().callMethod(
                            BlueZConstants::BLUEZ_SERVICE,
                            DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                            BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
                            BlueZConstants::UNREGISTER_ADVERTISEMENT,
                            std::move(params)
                        );
                    } catch (...) {
                        // 실패 무시, 가능한 한 많이 정리
                    }
                }
            }
            else {
                Logger::warn("GetManagedObjects 결과가 예상치 못한 형식: " + 
                            std::string(g_variant_get_type_string(result.get())));
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
    // Try up to 3 times, with increasing wait time
    bool success = false;
    std::string errorMsg;
    
    for (int retryCount = 0; retryCount < MAX_REGISTRATION_RETRIES; retryCount++) {
        try {
            // Create empty options dictionary using Utils helper function
            GVariantPtr emptyOptions = Utils::createEmptyDictionaryPtr();
            
            // Create tuple parameter
            GVariant* paramsRaw = g_variant_new("(o@a{sv})", 
                                        getPath().c_str(), 
                                        emptyOptions.get());
            
            // Manage parameter with makeGVariantPtr pattern
            GVariantPtr params = makeGVariantPtr(paramsRaw, true);
            
            // Call BlueZ
            GVariantPtr result = getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
                BlueZConstants::REGISTER_ADVERTISEMENT,
                std::move(params),
                "",
                5000
            );
            
            // If call succeeds
            registered = true;
            return true;
        }
        catch (const std::exception& e) {
            errorMsg = e.what();
            
            if (errorMsg.find("Maximum advertisements reached") != std::string::npos) {
                Logger::error("Maximum advertisements reached. Attempting to clean up...");
                
                // Try to clean up advertisements
                cleanupExistingAdvertisements();
                
                // Try restarting BlueZ on last attempt
                if (retryCount == MAX_REGISTRATION_RETRIES - 1) {
                    Logger::info("Trying to restart BlueZ to clear advertisements...");
                    system("sudo systemctl restart bluetooth.service");
                    sleep(2);  // Wait for restart
                }
            } else {
                Logger::error("DBus method failed: " + errorMsg);
            }
            
            // Wait before retry
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

// src/GattAdvertisement.cpp (continued)
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