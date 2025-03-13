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
        
        // 클라이언트 특성 설정 설명자(CCCD)인 경우, 이 값이 알림 활성화/비활성화를 제어
        if (uuid.toBlueZShortFormat() == "00002902") {
            if (newValue.size() >= 2) {
                // 첫 번째 바이트의 첫 번째 비트는 알림 활성화, 두 번째 비트는 표시(Indication) 활성화
                bool enableNotify = (newValue[0] & 0x01) != 0;
                bool enableIndicate = (newValue[0] & 0x02) != 0;
                
                try {
                    // 별도의 락 없이 특성의 알림 상태를 직접 변경
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
        
        // 값 변경 시 D-Bus 속성 변경 알림
        if (isRegistered()) {
            GVariantPtr valueVariant(
                Utils::gvariantFromByteArray(value.data(), value.size()),
                &g_variant_unref
            );
            
            if (valueVariant) {
                // 속성 변경 알림
                emitPropertyChanged(BlueZConstants::GATT_DESCRIPTOR_INTERFACE, "Value", std::move(valueVariant));
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in descriptor setValue: " + std::string(e.what()));
    }
}

bool GattDescriptor::setupDBusInterfaces() {
    // 속성 정의
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
    
    // 인터페이스 추가
    if (!addInterface(BlueZConstants::GATT_DESCRIPTOR_INTERFACE, properties)) {
        Logger::error("Failed to add descriptor interface");
        return false;
    }
    
    // 메서드 핸들러 등록
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
    
    // 객체 등록
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
        // 옵션 파라미터 처리 (예: offset)
        // 실제 구현에서는 이 부분을 확장해야 함
        
        std::vector<uint8_t> returnValue;
        
        // 콜백이 있으면 호출
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
                // 콜백이 없으면 저장된 값 반환
                std::lock_guard<std::mutex> valueLock(valueMutex);
                returnValue = value;
            }
        }
        
        // 결과 반환
        GVariantPtr resultVariant(
            Utils::gvariantFromByteArray(returnValue.data(), returnValue.size()),
            &g_variant_unref
        );
        
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
        
        // 메서드 응답 생성 및 전송
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
    
    // 파라미터 확인
    if (!call.parameters) {
        Logger::error("Missing parameters for descriptor WriteValue");
        
        // 오류 응답
        g_dbus_method_invocation_return_error_literal(
            call.invocation.get(),
            G_DBUS_ERROR,
            G_DBUS_ERROR_INVALID_ARGS,
            "Missing parameters"
        );
        return;
    }
    
    // 바이트 배열 파라미터 추출
    try {
        std::string byteString = Utils::stringFromGVariantByteArray(call.parameters.get());
        std::vector<uint8_t> newValue(byteString.begin(), byteString.end());
        
        // 콜백이 있으면 호출
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
            // 성공적으로 처리됨
            setValue(newValue); // setValue를 통해 특성의 알림 상태 업데이트
            
            // 빈 응답 생성 및 전송
            g_dbus_method_invocation_return_value(call.invocation.get(), nullptr);
        } else {
            // 콜백에서 실패 반환
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
        return Utils::gvariantFromString(uuid.toBlueZFormat());
    } catch (const std::exception& e) {
        Logger::error("Exception in descriptor getUuidProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattDescriptor::getCharacteristicProperty() {
    try {
        return Utils::gvariantFromObject(characteristic.getPath());
    } catch (const std::exception& e) {
        Logger::error("Exception in getCharacteristicProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattDescriptor::getPermissionsProperty() {
    try {
        std::vector<std::string> flags;

        Logger::debug("Descriptor permissions flags count: " + std::to_string(flags.size()));
        
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
            flags.push_back("read");  // 기본값 추가
            Logger::warn("Descriptor permissions empty, defaulting to 'read'");
        }
        
        return Utils::gvariantFromStringArray(flags);
    } catch (const std::exception& e) {
        Logger::error("Exception in getPermissionsProperty: " + std::string(e.what()));
        return nullptr;
    }
}

} // namespace ggk