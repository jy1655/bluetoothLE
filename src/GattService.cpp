// src/GattService.cpp
#include "GattService.h"
#include "GattCharacteristic.h"
#include "Logger.h"
#include "Utils.h"

namespace ggk {

GattService::GattService(
    DBusConnection& connection,
    const DBusObjectPath& path,
    const GattUuid& uuid,
    bool isPrimary
) : DBusObject(connection, path),
    uuid(uuid),
    primary(isPrimary) {
}

GattCharacteristicPtr GattService::createCharacteristic(
    const GattUuid& uuid,
    uint8_t properties,
    uint8_t permissions
) {
    if (uuid.toString().empty()) {
        Logger::error("Cannot create characteristic with empty UUID");
        return nullptr;
    }
    
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(characteristicsMutex);
    
    // Check if already exists
    auto it = characteristics.find(uuidStr);
    if (it != characteristics.end()) {
        if (!it->second) {
            Logger::error("Found null characteristic entry for UUID: " + uuidStr);
            characteristics.erase(it);
        } else {
            return it->second;
        }
    }
    
    try {
        // Create standardized object path
        // Format: <service_path>/char<uuid_short>
        std::string uuidShort = "/char" + uuid.toBlueZShortFormat().substr(0, 8);
        DBusObjectPath charPath = getPath() + uuidShort;
        
        // Create characteristic
        GattCharacteristicPtr characteristic = std::make_shared<GattCharacteristic>(
            getConnection(),
            charPath,
            uuid,
            *this,
            properties,
            permissions
        );
        
        if (!characteristic) {
            Logger::error("Failed to create characteristic for UUID: " + uuidStr);
            return nullptr;
        }
        
        // Add to map
        characteristics[uuidStr] = characteristic;
        
        // Ensure CCCD exists for notify/indicate characteristics
        if ((properties & GattProperty::PROP_NOTIFY) || 
            (properties & GattProperty::PROP_INDICATE)) {
            characteristic->ensureCCCDExists();
        }
        
        Logger::info("Created characteristic: " + uuidStr + " at path: " + charPath.toString());
        return characteristic;
    } catch (const std::exception& e) {
        Logger::error("Exception during characteristic creation: " + std::string(e.what()));
        return nullptr;
    }
}

GattCharacteristicPtr GattService::getCharacteristic(const GattUuid& uuid) const {
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(characteristicsMutex);
    
    auto it = characteristics.find(uuidStr);
    if (it != characteristics.end() && it->second) {
        return it->second;
    }
    
    return nullptr;
}

bool GattService::setupDBusInterfaces() {
    // Define properties
    std::vector<DBusProperty> properties = {
        {
            "UUID",
            "s",
            true,
            false,
            false,
            [this]() { return getUuidProperty(); },
            nullptr
        },
        {
            "Primary",
            "b",
            true,
            false,
            false,
            [this]() { return getPrimaryProperty(); },
            nullptr
        },
        {
            "Characteristics",
            "ao",
            true,
            false,
            true,
            [this]() { return getCharacteristicsProperty(); },
            nullptr
        }
    };
    
    // Add interface
    if (!addInterface(BlueZConstants::GATT_SERVICE_INTERFACE, properties)) {
        Logger::error("Failed to add service interface");
        return false;
    }
    
    // Register object
    if (!registerObject()) {
        Logger::error("Failed to register service object");
        return false;
    }
    
    Logger::info("Registered GATT service: " + uuid.toString());
    return true;
}

GVariant* GattService::getUuidProperty() {
    try {
        // Use makeGVariantPtr pattern
        GVariantPtr variantPtr = Utils::gvariantPtrFromString(uuid.toBlueZFormat());
        if (!variantPtr) {
            return nullptr;
        }
        // Transfer ownership
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getUuidProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattService::getPrimaryProperty() {
    try {
        // Use makeGVariantPtr pattern
        GVariantPtr variantPtr = Utils::gvariantPtrFromBoolean(primary);
        if (!variantPtr) {
            return nullptr;
        }
        // Transfer ownership
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getPrimaryProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattService::getCharacteristicsProperty() {
    try {
        std::vector<std::string> paths;
        
        std::lock_guard<std::mutex> lock(characteristicsMutex);
        
        for (const auto& pair : characteristics) {
            if (pair.second) {  // Check for nullptr
                paths.push_back(pair.second->getPath().toString());
            }
        }
        
        // Use makeGVariantPtr pattern
        GVariantPtr variantPtr = Utils::gvariantPtrFromStringArray(paths);
        if (!variantPtr) {
            return nullptr;
        }
        // Transfer ownership
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getCharacteristicsProperty: " + std::string(e.what()));
        return nullptr;
    }
}

} // namespace ggk