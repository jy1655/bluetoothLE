// src/GattDescriptor.cpp
#include "GattDescriptor.h"
#include "GattCharacteristic.h"
#include "Logger.h"

namespace ggk {

GattDescriptor::GattDescriptor(
    SDBusConnection& connection,
    const std::string& path,
    const GattUuid& uuid,
    GattCharacteristic& characteristic,
    uint8_t permissions)
    : connection(connection),
      object(connection, path),
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
        
        // CCCD 설명자 처리 (UUID 2902)
        if (uuid.toBlueZShortFormat() == "00002902") {
            if (newValue.size() >= 2) {
                // 첫 번째 바이트의 첫 번째 비트는 알림용, 두 번째 비트는 표시용
                bool enableNotify = (newValue[0] & 0x01) != 0;
                bool enableIndicate = (newValue[0] & 0x02) != 0;
                
                try {
                    // 특성의 알림 상태 업데이트
                    if (enableNotify || enableIndicate) {
                        characteristic.startNotify();
                    } else {
                        characteristic.stopNotify();
                    }
                } catch (const std::exception& e) {
                    Logger::error("알림 상태 변경 실패: " + std::string(e.what()));
                }
            }
        }
        
        // 등록된 경우 속성 변경 시그널 발생
        if (object.isRegistered()) {
            // Value 속성 변경 알림
            object.emitPropertyChanged(BlueZConstants::GATT_DESCRIPTOR_INTERFACE, "Value");
        }
    } catch (const std::exception& e) {
        Logger::error("설명자 setValue에서 예외: " + std::string(e.what()));
    }
}

bool GattDescriptor::setupDBusInterfaces() {
    // 속성 등록
    object.registerProperty(
        BlueZConstants::GATT_DESCRIPTOR_INTERFACE,
        "UUID",
        "s",
        [this]() -> std::string { return uuid.toBlueZFormat(); }
    );
    
    object.registerProperty(
        BlueZConstants::GATT_DESCRIPTOR_INTERFACE,
        "Characteristic",
        "o",
        [this]() -> sdbus::ObjectPath { return sdbus::ObjectPath(characteristic.getPath()); }
    );
    
    object.registerProperty(
        BlueZConstants::GATT_DESCRIPTOR_INTERFACE,
        "Flags",
        "as",
        [this]() -> std::vector<std::string> {
            std::vector<std::string> flags;
            
            // 권한에 따라 플래그 추가
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
                flags.push_back("read");  // 기본 권한 추가
                Logger::warn("설명자 권한이 비어 있어 기본값 'read'로 설정");
            }
            
            return flags;
        }
    );
    
    // ReadValue 메서드 등록
    object.registerReadValueMethod(
        BlueZConstants::GATT_DESCRIPTOR_INTERFACE,
        [this](const std::map<std::string, sdbus::Variant>& options) -> std::vector<uint8_t> {
            return handleReadValue(options);
        }
    );
    
    // WriteValue 메서드 등록
    object.registerWriteValueMethod(
        BlueZConstants::GATT_DESCRIPTOR_INTERFACE,
        [this](const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options) {
            handleWriteValue(value, options);
        }
    );
    
    // 객체 등록
    return object.registerObject();
}

std::vector<uint8_t> GattDescriptor::handleReadValue(const std::map<std::string, sdbus::Variant>& options) {
    Logger::debug("설명자에 대해 ReadValue 호출됨: " + uuid.toString());
    
    std::vector<uint8_t> returnValue;
    
    // 콜백 사용
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex);
        if (readCallback) {
            try {
                returnValue = readCallback();
            } catch (const std::exception& e) {
                Logger::error("설명자 읽기 콜백에서 예외: " + std::string(e.what()));
                throw sdbus::Error("org.bluez.Error.Failed", e.what());
            }
        } else {
            // 저장된 값 사용
            std::lock_guard<std::mutex> valueLock(valueMutex);
            returnValue = value;
        }
    }
    
    return returnValue;
}

void GattDescriptor::handleWriteValue(const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options) {
    Logger::debug("설명자에 대해 WriteValue 호출됨: " + uuid.toString());
    
    // 콜백 사용
    bool success = true;
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex);
        if (writeCallback) {
            try {
                success = writeCallback(value);
            } catch (const std::exception& e) {
                Logger::error("설명자 쓰기 콜백에서 예외: " + std::string(e.what()));
                throw sdbus::Error("org.bluez.Error.Failed", e.what());
            }
        }
    }
    
    if (success) {
        // 성공적으로 처리됨
        setValue(value); // 알림 상태를 업데이트하기 위해 setValue 사용
    } else {
        // 콜백이 실패 반환
        throw sdbus::Error("org.bluez.Error.Failed", "Write operation failed");
    }
}

} // namespace ggk