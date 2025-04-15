// src/GattCharacteristic.cpp
#include "GattCharacteristic.h"
#include "GattService.h"
#include "GattDescriptor.h"
#include "Logger.h"
#include "Utils.h"

namespace ggk {

GattCharacteristic::GattCharacteristic(
    DBusConnection& connection,
    const DBusObjectPath& path,
    const GattUuid& uuid,
    GattService& service,
    uint8_t properties,
    uint8_t permissions
) : DBusObject(connection, path),
    uuid(uuid),
    service(service),
    properties(properties),
    permissions(permissions),
    notifying(false) {
}

void GattCharacteristic::setValue(const std::vector<uint8_t>& newValue) {
    try {
        // Set the value
        {
            std::lock_guard<std::mutex> lock(valueMutex);
            value = newValue;
        }
        
        // Emit PropertyChanged signal if registered
        if (isRegistered()) {
            // Create GVariant using makeGVariantPtr pattern
            GVariantPtr valueVariant = Utils::gvariantPtrFromByteArray(newValue.data(), newValue.size());
            
            if (!valueVariant) {
                Logger::error("Failed to create GVariant for characteristic value");
                return;
            }
            
            // Emit ValueChanged signal
            emitPropertyChanged(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, 
                               "Value", 
                               std::move(valueVariant));
            
            // If notifying, handle notification
            bool isNotifying = false;
            {
                std::lock_guard<std::mutex> notifyLock(notifyMutex);
                isNotifying = notifying;
            }
            
            if (isNotifying) {
                // Execute callback if registered
                std::lock_guard<std::mutex> callbackLock(callbackMutex);
                if (notifyCallback) {
                    try {
                        notifyCallback();
                    } catch (const std::exception& e) {
                        Logger::error("Exception in notify callback: " + std::string(e.what()));
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in setValue: " + std::string(e.what()));
    }
}

void GattCharacteristic::setValue(std::vector<uint8_t>&& newValue) {
    try {
        // Set the value using move semantics
        {
            std::lock_guard<std::mutex> lock(valueMutex);
            value = std::move(newValue);
        }
        
        // Emit PropertyChanged signal if registered
        if (isRegistered()) {
            // Create GVariant using makeGVariantPtr pattern
            GVariantPtr valueVariant = Utils::gvariantPtrFromByteArray(value.data(), value.size());
            
            if (!valueVariant) {
                Logger::error("Failed to create GVariant for characteristic value");
                return;
            }
            
            // Check notification state
            bool isNotifying = false;
            {
                std::lock_guard<std::mutex> notifyLock(notifyMutex);
                isNotifying = notifying;
            }
            
            // Handle notification if active
            if (isNotifying) {
                std::lock_guard<std::mutex> callbackLock(callbackMutex);
                if (notifyCallback) {
                    try {
                        notifyCallback();
                    } catch (const std::exception& e) {
                        Logger::error("Exception in notify callback: " + std::string(e.what()));
                    }
                }
            }
            
            // Emit property changed signal
            emitPropertyChanged(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, 
                               "Value", 
                               std::move(valueVariant));
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in setValue (move): " + std::string(e.what()));
    }
}

GattDescriptorPtr GattCharacteristic::createDescriptor(
    const GattUuid& uuid,
    uint8_t permissions
) {
    if (uuid.toString().empty()) {
        Logger::error("Cannot create descriptor with empty UUID");
        return nullptr;
    }
    
    // CCCD UUID (0x2902)
    const std::string CCCD_UUID = "00002902-0000-1000-8000-00805f9b34fb";
    
    // Check if trying to create CCCD descriptor while characteristic has notify/indicate
    if (uuid.toString() == CCCD_UUID && 
        (properties & GattProperty::PROP_NOTIFY || properties & GattProperty::PROP_INDICATE)) {
        Logger::warn("Attempted to manually create CCCD descriptor for characteristic with notify/indicate. " 
                    "This is handled automatically by BlueZ 5.82+. Ignoring request.");
        return nullptr;
    }
    
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(descriptorsMutex);
    
    // Check if already exists
    auto it = descriptors.find(uuidStr);
    if (it != descriptors.end()) {
        if (!it->second) {
            Logger::error("Found null descriptor entry for UUID: " + uuidStr);
            descriptors.erase(it);
        } else {
            return it->second;
        }
    }
    
    try {
        // Create consistent object path
        std::string descNum = "desc" + std::to_string(descriptors.size() + 1);
        DBusObjectPath descriptorPath = getPath() + descNum;
        
        // Create descriptor
        GattDescriptorPtr descriptor = std::make_shared<GattDescriptor>(
            getConnection(),
            descriptorPath,
            uuid,
            *this,
            permissions
        );
        
        if (!descriptor) {
            Logger::error("Failed to create descriptor for UUID: " + uuidStr);
            return nullptr;
        }
        
        // Add to map
        descriptors[uuidStr] = descriptor;
        
        Logger::info("Created descriptor: " + uuidStr + " at path: " + descriptorPath.toString());
        return descriptor;
    } catch (const std::exception& e) {
        Logger::error("Exception during descriptor creation: " + std::string(e.what()));
        return nullptr;
    }
}

GattDescriptorPtr GattCharacteristic::getDescriptor(const GattUuid& uuid) const {
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(descriptorsMutex);
    
    auto it = descriptors.find(uuidStr);
    if (it != descriptors.end() && it->second) {
        return it->second;
    }
    
    return nullptr;
}

bool GattCharacteristic::startNotify() {
    std::lock_guard<std::mutex> lock(notifyMutex);
    
    if (notifying) {
        return true;  // Already notifying
    }
    
    // Check if characteristic supports notifications
    if (!(properties & GattProperty::PROP_NOTIFY) && 
        !(properties & GattProperty::PROP_INDICATE)) {
        Logger::error("Characteristic does not support notifications: " + uuid.toString());
        return false;
    }
    
    notifying = true;
    
    // Emit PropertyChanged signal for Notifying
    if (isRegistered()) {
        // Use makeGVariantPtr pattern
        GVariantPtr valueVariant = Utils::gvariantPtrFromBoolean(true);
        
        if (!valueVariant) {
            Logger::error("Failed to create GVariant for notification state");
            notifying = false;
            return false;
        }
        
        emitPropertyChanged(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, 
                           "Notifying", 
                           std::move(valueVariant));
    }
    
    // Call notify callback
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex);
        if (notifyCallback) {
            try {
                notifyCallback();
            } catch (const std::exception& e) {
                Logger::error("Exception in notify callback: " + std::string(e.what()));
                // Keep notification state enabled even if callback fails
            }
        }
    }
    
    Logger::info("Started notifications for characteristic: " + uuid.toString());
    return true;
}

bool GattCharacteristic::stopNotify() {
    std::lock_guard<std::mutex> lock(notifyMutex);
    
    if (!notifying) {
        return true;  // Already stopped
    }
    
    notifying = false;
    
    // Emit PropertyChanged signal for Notifying
    if (isRegistered()) {
        // Use makeGVariantPtr pattern
        GVariantPtr valueVariant = Utils::gvariantPtrFromBoolean(false);
        
        if (!valueVariant) {
            Logger::error("Failed to create GVariant for notification state");
            notifying = true;  // Restore original state
            return false;
        }
        
        emitPropertyChanged(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, 
                           "Notifying", 
                           std::move(valueVariant));
    }
    
    Logger::info("Stopped notifications for: " + uuid.toString());
    return true;
}

bool GattCharacteristic::setupDBusInterfaces() {
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
            "Service",
            "o",
            true,
            false,
            false,
            [this]() { return getServiceProperty(); },
            nullptr
        },
        {
            "Flags",
            "as",
            true,
            false,
            false,
            [this]() { return getPropertiesProperty(); },
            nullptr
        },
        {
            "Descriptors",
            "ao",
            true,
            false,
            true,
            [this]() { return getDescriptorsProperty(); },
            nullptr
        },
        {
            "Notifying",
            "b",
            true,
            false,
            true,
            [this]() { return getNotifyingProperty(); },
            nullptr
        }
    };
    
    // Add interface
    if (!addInterface(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, properties)) {
        Logger::error("Failed to add characteristic interface");
        return false;
    }
    
    // Add method handlers
    if (!addMethodWithSignature(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, "ReadValue", 
                  [this](const DBusMethodCall& call) { handleReadValue(call); },
                  "a{sv}", "ay")) {  // a{sv} 입력(옵션 딕셔너리), ay 출력(바이트 배열)
        Logger::error("Failed to add ReadValue method");
        return false;
    }
    
    if (!addMethodWithSignature(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, "WriteValue", 
                  [this](const DBusMethodCall& call) { handleWriteValue(call); },
                  "aya{sv}", "")) {  // aya{sv} 입력(값과 옵션 딕셔너리), 빈 출력
        Logger::error("Failed to add WriteValue method");
        return false;
    }
    
    if (!addMethod(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, "StartNotify", 
                  [this](const DBusMethodCall& call) { handleStartNotify(call); })) {
        Logger::error("Failed to add StartNotify method");
        return false;
    }
    
    if (!addMethod(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, "StopNotify", 
                  [this](const DBusMethodCall& call) { handleStopNotify(call); })) {
        Logger::error("Failed to add StopNotify method");
        return false;
    }
    
    // Register object
    if (!registerObject()) {
        Logger::error("Failed to register characteristic object");
        return false;
    }
    
    Logger::info("Registered GATT characteristic: " + uuid.toString());
    return true;
}

void GattCharacteristic::handleReadValue(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in ReadValue");
        return;
    }
    
    Logger::debug("ReadValue called for characteristic: " + uuid.toString());
    
    try {
        // 옵션 파라미터 처리 (offset 등)
        // BlueZ 5.82에서는 a{sv} 형태로 옵션이 전달됨
        uint16_t offset = 0;
        if (call.parameters) {
            Logger::debug("ReadValue parameters received: " + std::string(g_variant_get_type_string(call.parameters.get())));
            // 옵션에서 offset 추출 예시
            GVariant* offsetVar = g_variant_lookup_value(call.parameters.get(), "offset", G_VARIANT_TYPE_UINT16);
            if (offsetVar) {
                offset = g_variant_get_uint16(offsetVar);
                g_variant_unref(offsetVar);
                Logger::debug("Read offset: " + std::to_string(offset));
            }
        }
        
        std::vector<uint8_t> returnValue;
        
        // 콜백 사용
        {
            std::lock_guard<std::mutex> callbackLock(callbackMutex);
            if (readCallback) {
                try {
                    returnValue = readCallback();
                } catch (const std::exception& e) {
                    Logger::error("Exception in read callback: " + std::string(e.what()));
                    g_dbus_method_invocation_return_error_literal(
                        call.invocation.get(),
                        G_DBUS_ERROR,
                        G_DBUS_ERROR_FAILED,
                        e.what()
                    );
                    return;
                }
            } else {
                // 저장된 값 사용
                std::lock_guard<std::mutex> valueLock(valueMutex);
                returnValue = value;
            }
        }
        
        // offset 적용 (있는 경우)
        if (offset > 0 && offset < returnValue.size()) {
            returnValue.erase(returnValue.begin(), returnValue.begin() + offset);
        }
        
        // 응답 생성
        GVariantPtr resultVariant = Utils::gvariantPtrFromByteArray(returnValue.data(), returnValue.size());
        
        if (!resultVariant) {
            Logger::error("Failed to create GVariant for read response");
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(),
                G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED,
                "Failed to create response"
            );
            return;
        }
        
        // 응답 반환
        g_dbus_method_invocation_return_value(call.invocation.get(), resultVariant.get());
        
    } catch (const std::exception& e) {
        Logger::error("Exception in ReadValue: " + std::string(e.what()));
        g_dbus_method_invocation_return_error_literal(
            call.invocation.get(),
            G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED,
            e.what()
        );
    }
}

void GattCharacteristic::handleWriteValue(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in WriteValue");
        return;
    }
    
    Logger::debug("WriteValue called for characteristic: " + uuid.toString());
    
    // 파라미터 확인
    if (!call.parameters) {
        Logger::error("Missing parameters for WriteValue");
        g_dbus_method_invocation_return_error_literal(
            call.invocation.get(),
            G_DBUS_ERROR,
            G_DBUS_ERROR_INVALID_ARGS,
            "Missing parameters"
        );
        return;
    }
    
    // BlueZ 5.82에서는 WriteValue(ay options) 형태로 호출됨
    // 첫 번째는 바이트 배열, 두 번째는 옵션 딕셔너리
    try {
        Logger::debug("WriteValue parameters type: " + std::string(g_variant_get_type_string(call.parameters.get())));
        
        // 바이트 배열과 옵션 추출
        GVariant* value_variant = nullptr;
        GVariant* options_variant = nullptr;
        
        // 튜플에서 값과 옵션 추출
        if (g_variant_is_of_type(call.parameters.get(), G_VARIANT_TYPE("(aya{sv})"))) {
            value_variant = g_variant_get_child_value(call.parameters.get(), 0);
            options_variant = g_variant_get_child_value(call.parameters.get(), 1);
        }
        // 단일 값만 전달된 경우 (이전 버전 호환)
        else if (g_variant_is_of_type(call.parameters.get(), G_VARIANT_TYPE("ay"))) {
            value_variant = g_variant_ref(call.parameters.get());
        }
        
        if (!value_variant) {
            Logger::error("Cannot extract value from WriteValue parameters");
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(),
                G_DBUS_ERROR,
                G_DBUS_ERROR_INVALID_ARGS,
                "Cannot extract value"
            );
            return;
        }
        
        // 옵션 처리 (예: offset)
        uint16_t offset = 0;
        if (options_variant) {
            GVariant* offsetVar = g_variant_lookup_value(options_variant, "offset", G_VARIANT_TYPE_UINT16);
            if (offsetVar) {
                offset = g_variant_get_uint16(offsetVar);
                g_variant_unref(offsetVar);
                Logger::debug("Write offset: " + std::to_string(offset));
            }
            g_variant_unref(options_variant);
        }
        
        // 바이트 배열로 변환
        std::vector<uint8_t> newValue = Utils::variantToByteArray(value_variant);
        g_variant_unref(value_variant);
        
        // 콜백 사용
        bool success = true;
        {
            std::lock_guard<std::mutex> callbackLock(callbackMutex);
            if (writeCallback) {
                try {
                    success = writeCallback(newValue);
                } catch (const std::exception& e) {
                    Logger::error("Exception in write callback: " + std::string(e.what()));
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
            // 처리 성공 - offset 적용해서 값 설정
            if (offset > 0) {
                std::lock_guard<std::mutex> valueLock(valueMutex);
                // 필요시 기존 값 확장
                if (offset >= value.size()) {
                    value.resize(offset + newValue.size(), 0);
                }
                // offset 위치에 새 값 복사
                std::copy(newValue.begin(), newValue.end(), value.begin() + offset);
            } else {
                // 그냥 전체 값 설정
                setValue(newValue);
            }
            
            // 빈 응답
            g_dbus_method_invocation_return_value(call.invocation.get(), nullptr);
        } else {
            // 콜백 실패
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(),
                G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED,
                "Write operation failed"
            );
        }
    } catch (const std::exception& e) {
        Logger::error("Failed to parse WriteValue parameters: " + std::string(e.what()));
        g_dbus_method_invocation_return_error_literal(
            call.invocation.get(),
            G_DBUS_ERROR,
            G_DBUS_ERROR_INVALID_ARGS,
            "Invalid parameters"
        );
    }
}

void GattCharacteristic::handleStartNotify(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in StartNotify");
        return;
    }
    
    Logger::debug("StartNotify called for characteristic: " + uuid.toString());
    
    if (startNotify()) {
        // Success response
        g_dbus_method_invocation_return_value(call.invocation.get(), nullptr);
    } else {
        // Error response
        g_dbus_method_invocation_return_error_literal(
            call.invocation.get(),
            G_DBUS_ERROR,
            G_DBUS_ERROR_NOT_SUPPORTED,
            "Notifications not supported"
        );
    }
}

void GattCharacteristic::handleStopNotify(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in StopNotify");
        return;
    }
    
    Logger::debug("StopNotify called for characteristic: " + uuid.toString());
    
    if (stopNotify()) {
        // Success response
        g_dbus_method_invocation_return_value(call.invocation.get(), nullptr);
    } else {
        // Error response
        g_dbus_method_invocation_return_error_literal(
            call.invocation.get(),
            G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED,
            "Failed to stop notifications"
        );
    }
}

GVariant* GattCharacteristic::getUuidProperty() {
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

GVariant* GattCharacteristic::getServiceProperty() {
    try {
        // Use makeGVariantPtr pattern
        GVariantPtr variantPtr = Utils::gvariantPtrFromObject(service.getPath());
        if (!variantPtr) {
            return nullptr;
        }
        // Transfer ownership
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getServiceProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattCharacteristic::getPropertiesProperty() {
    try {
        std::vector<std::string> flags;
        
        // Add flags based on properties
        if (properties & GattProperty::PROP_BROADCAST) {
            flags.push_back("broadcast");
        }
        if (properties & GattProperty::PROP_READ) {
            flags.push_back("read");
        }
        if (properties & GattProperty::PROP_WRITE_WITHOUT_RESPONSE) {
            flags.push_back("write-without-response");
        }
        if (properties & GattProperty::PROP_WRITE) {
            flags.push_back("write");
        }
        if (properties & GattProperty::PROP_NOTIFY) {
            flags.push_back("notify");
        }
        if (properties & GattProperty::PROP_INDICATE) {
            flags.push_back("indicate");
        }
        if (properties & GattProperty::PROP_AUTHENTICATED_SIGNED_WRITES) {
            flags.push_back("authenticated-signed-writes");
        }
        
        // Add permission flags
        if (permissions & GattPermission::PERM_READ_ENCRYPTED) {
            flags.push_back("encrypt-read");
        }
        if (permissions & GattPermission::PERM_WRITE_ENCRYPTED) {
            flags.push_back("encrypt-write");
        }
        if (permissions & GattPermission::PERM_READ_AUTHENTICATED) {
            flags.push_back("auth-read");
        }
        if (permissions & GattPermission::PERM_WRITE_AUTHENTICATED) {
            flags.push_back("auth-write");
        }
        
        // Use makeGVariantPtr pattern
        GVariantPtr variantPtr = Utils::gvariantPtrFromStringArray(flags);
        if (!variantPtr) {
            return nullptr;
        }
        // Transfer ownership
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getPropertiesProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattCharacteristic::getDescriptorsProperty() {
    try {
        std::vector<std::string> paths;
        
        std::lock_guard<std::mutex> lock(descriptorsMutex);
        
        for (const auto& pair : descriptors) {
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
        Logger::error("Exception in getDescriptorsProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattCharacteristic::getNotifyingProperty() {
    try {
        std::lock_guard<std::mutex> lock(notifyMutex);
        // Use makeGVariantPtr pattern
        GVariantPtr variantPtr = Utils::gvariantPtrFromBoolean(notifying);
        if (!variantPtr) {
            return nullptr;
        }
        // Transfer ownership
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getNotifyingProperty: " + std::string(e.what()));
        return nullptr;
    }
}

} // namespace ggk