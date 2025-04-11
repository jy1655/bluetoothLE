// src/GattDescriptor.cpp
#include "GattDescriptor.h"
#include "GattCharacteristic.h"
#include "Logger.h"
#include "Utils.h"

namespace ggk {

GattDescriptor::GattDescriptor(
    DBusConnection& connection,
    const DBusObjectPath& path,
    const GattUuid& uuid,
    GattCharacteristic& characteristic,
    uint8_t permissions
) : DBusObject(connection, path),
    uuid(uuid),
    characteristic(characteristic),
    permissions(permissions) {
}

void GattDescriptor::setValue(const std::vector<uint8_t>& newValue) {
    try {
        {
            std::lock_guard<std::mutex> lock(valueMutex);
            value = newValue;
        }
        
        // Handle CCCD descriptor (UUID 2902)
        if (uuid.toBlueZShortFormat() == "00002902") {
            if (newValue.size() >= 2) {
                // First byte's first bit is for notifications, second bit for indications
                bool enableNotify = (newValue[0] & 0x01) != 0;
                bool enableIndicate = (newValue[0] & 0x02) != 0;
                
                try {
                    // Update characteristic's notification state
                    if (enableNotify || enableIndicate) {
                        characteristic.startNotify();
                    } else {
                        characteristic.stopNotify();
                    }
                } catch (const std::exception& e) {
                    Logger::error("Failed to change notification state: " + std::string(e.what()));
                }
            }
        }
        
        // Emit property changed if registered
        if (isRegistered()) {
            // Create a GVariant for the new value - use makeGVariantPtr pattern
            GVariantPtr valueVariant = Utils::gvariantPtrFromByteArray(value.data(), value.size());
            
            if (valueVariant) {
                // Emit property changed signal
                emitPropertyChanged(BlueZConstants::GATT_DESCRIPTOR_INTERFACE, "Value", std::move(valueVariant));
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in descriptor setValue: " + std::string(e.what()));
    }
}

bool GattDescriptor::setupDBusInterfaces() {
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
            "Characteristic",
            "o",
            true,
            false,
            false,
            [this]() { return getCharacteristicProperty(); },
            nullptr
        },
        {
            "Flags",
            "as",
            true,
            false, 
            false,
            [this]() { return getPermissionsProperty(); },
            nullptr
        }
    };
    
    // Add interface
    if (!addInterface(BlueZConstants::GATT_DESCRIPTOR_INTERFACE, properties)) {
        Logger::error("Failed to add descriptor interface");
        return false;
    }
    
    // Add method handlers
    if (!addMethod(BlueZConstants::GATT_DESCRIPTOR_INTERFACE, "ReadValue", 
                  [this](const DBusMethodCall& call) { handleReadValue(call); })) {
        Logger::error("Failed to add ReadValue method");
        return false;
    }
    
    if (!addMethod(BlueZConstants::GATT_DESCRIPTOR_INTERFACE, "WriteValue", 
                  [this](const DBusMethodCall& call) { handleWriteValue(call); })) {
        Logger::error("Failed to add WriteValue method");
        return false;
    }
    
    // Register object
    if (!registerObject()) {
        Logger::error("Failed to register descriptor object");
        return false;
    }
    
    Logger::info("Registered GATT descriptor: " + uuid.toString());
    return true;
}

void GattDescriptor::handleReadValue(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in descriptor ReadValue");
        return;
    }
    
    Logger::debug("ReadValue called for descriptor: " + uuid.toString());
    
    try {
        // Handle options parameter (e.g., offset)
        // In a real implementation, this section would be extended
        
        std::vector<uint8_t> returnValue;
        
        // Use callback if available
        {
            std::lock_guard<std::mutex> callbackLock(callbackMutex);
            if (readCallback) {
                try {
                    returnValue = readCallback();
                } catch (const std::exception& e) {
                    Logger::error("Exception in descriptor read callback: " + std::string(e.what()));
                    g_dbus_method_invocation_return_error_literal(
                        call.invocation.get(),
                        G_DBUS_ERROR,
                        G_DBUS_ERROR_FAILED,
                        e.what()
                    );
                    return;
                }
            } else {
                // Use stored value if no callback
                std::lock_guard<std::mutex> valueLock(valueMutex);
                returnValue = value;
            }
        }
        
        // Create response
        GVariantPtr resultVariant = Utils::gvariantPtrFromByteArray(returnValue.data(), returnValue.size());
        
        if (!resultVariant) {
            Logger::error("Failed to create GVariant for descriptor read response");
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(),
                G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED,
                "Failed to create response"
            );
            return;
        }
        
        // Return the response
        g_dbus_method_invocation_return_value(call.invocation.get(), resultVariant.get());
        
    } catch (const std::exception& e) {
        Logger::error("Exception in descriptor ReadValue: " + std::string(e.what()));
        g_dbus_method_invocation_return_error_literal(
            call.invocation.get(),
            G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED,
            e.what()
        );
    }
}

void GattDescriptor::handleWriteValue(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in descriptor WriteValue");
        return;
    }
    
    Logger::debug("WriteValue called for descriptor: " + uuid.toString());
    
    // Check parameters
    if (!call.parameters) {
        Logger::error("Missing parameters for descriptor WriteValue");
        
        // Return error
        g_dbus_method_invocation_return_error_literal(
            call.invocation.get(),
            G_DBUS_ERROR,
            G_DBUS_ERROR_INVALID_ARGS,
            "Missing parameters"
        );
        return;
    }
    
    // Extract byte array parameter
    try {
        std::string byteString = Utils::stringFromGVariantByteArray(call.parameters.get());
        std::vector<uint8_t> newValue(byteString.begin(), byteString.end());
        
        // Use callback if available
        bool success = true;
        {
            std::lock_guard<std::mutex> callbackLock(callbackMutex);
            if (writeCallback) {
                try {
                    success = writeCallback(newValue);
                } catch (const std::exception& e) {
                    Logger::error("Exception in descriptor write callback: " + std::string(e.what()));
                    g_dbus_method_invocation_return_error_literal(
                        call.invocation.get(),
                        G_DBUS_ERROR,
                        G_DBUS_ERROR_FAILED,
                        e.what()
                    );
                    return;
                }
            }
        }
        
        if (success) {
            // Successfully processed
            setValue(newValue); // Use setValue to update notification state
            
            // Return empty response
            g_dbus_method_invocation_return_value(call.invocation.get(), nullptr);
        } else {
            // Callback returned failure
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(),
                G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED,
                "Write operation failed"
            );
        }
    } catch (const std::exception& e) {
        Logger::error("Failed to parse descriptor WriteValue parameters: " + std::string(e.what()));
        
        g_dbus_method_invocation_return_error_literal(
            call.invocation.get(),
            G_DBUS_ERROR,
            G_DBUS_ERROR_INVALID_ARGS,
            "Invalid parameters"
        );
    }
}

GVariant* GattDescriptor::getUuidProperty() {
    try {
        // Use makeGVariantPtr pattern
        GVariantPtr variantPtr = Utils::gvariantPtrFromString(uuid.toBlueZFormat());
        if (!variantPtr) {
            return nullptr;
        }
        // Transfer ownership
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in descriptor getUuidProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattDescriptor::getCharacteristicProperty() {
    try {
        // Use makeGVariantPtr pattern
        GVariantPtr variantPtr = Utils::gvariantPtrFromObject(characteristic.getPath());
        if (!variantPtr) {
            return nullptr;
        }
        // Transfer ownership
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getCharacteristicProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattDescriptor::getPermissionsProperty() {
    try {
        std::vector<std::string> flags;
        
        // Add flags based on permissions
        if (permissions & GattPermission::PERM_READ) {
            flags.push_back("read");
        }
        if (permissions & GattPermission::PERM_WRITE) {
            flags.push_back("write");
        }
        if (permissions & GattPermission::PERM_READ_ENCRYPTED) {
            flags.push_back("encrypt-read");
        }
        if (permissions & GattPermission::PERM_WRITE_ENCRYPTED){
            flags.push_back("encrypt-write");
        }
        if (permissions & GattPermission::PERM_READ_AUTHENTICATED) {
            flags.push_back("auth-read");
        }
        if (permissions & GattPermission::PERM_WRITE_AUTHENTICATED) {
            flags.push_back("auth-write");
        }

        if (flags.empty()) {
            flags.push_back("read");  // Add default permission
            Logger::warn("Descriptor permissions empty, defaulting to 'read'");
        }
        
        // Use makeGVariantPtr pattern
        GVariantPtr variantPtr = Utils::gvariantPtrFromStringArray(flags);
        if (!variantPtr) {
            return nullptr;
        }
        // Transfer ownership
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getPermissionsProperty: " + std::string(e.what()));
        return nullptr;
    }
}

} // namespace ggk