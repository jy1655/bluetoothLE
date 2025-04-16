// src/GattDescriptor.cpp - 순환 참조 개선
#include "GattDescriptor.h"
#include "GattCharacteristic.h" // 여기서만 포함 (헤더에서는 전방 선언만)
#include "Logger.h"

namespace ggk {

GattDescriptor::GattDescriptor(
    SDBusConnection& connection,
    const std::string& path,
    const GattUuid& uuid,
    GattCharacteristic* characteristic,
    uint8_t permissions)
    : connection(connection),
      object(connection, path),
      uuid(uuid),
      parentCharacteristic(characteristic),
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
                
                // 부모 특성이 유효한지 확인
                if (parentCharacteristic) {
                    try {
                        // 특성의 알림 상태 업데이트
                        if (enableNotify || enableIndicate) {
                            parentCharacteristic->startNotify();
                        } else {
                            parentCharacteristic->stopNotify();
                        }
                    } catch (const std::exception& e) {
                        Logger::error("알림 상태 변경 실패: " + std::string(e.what()));
                    }
                }
            }
        }
        
        // 등록된 경우 속성 변경 시그널 발생
        if (object.isRegistered()) {
            // Value 속성 변경 알림
            object.emitPropertyChanged(BlueZConstants::GATT_DESCRIPTOR_INTERFACE, BlueZConstants::PROPERTY_VALUE);
        }
    } catch (const std::exception& e) {
        Logger::error("설명자 setValue에서 예외: " + std::string(e.what()));
    }
}

bool GattDescriptor::setupDBusInterfaces() {
    // UUID 속성 등록
    object.registerProperty(
        BlueZConstants::GATT_DESCRIPTOR_INTERFACE,
        BlueZConstants::PROPERTY_UUID,
        "s",
        [this]() -> std::string { return uuid.toBlueZFormat(); }
    );
    
    // Characteristic 속성 등록 (부모 특성이 있는 경우)
    if (parentCharacteristic) {
        object.registerProperty(
            BlueZConstants::GATT_DESCRIPTOR_INTERFACE,
            BlueZConstants::PROPERTY_CHARACTERISTIC,
            "o",
            [this]() -> sdbus::ObjectPath { 
                return sdbus::ObjectPath(parentCharacteristic->getPath()); 
            }
        );
    }
    
    // Value 속성 등록 - BlueZ 5.82+ 호환
    object.registerProperty(
        BlueZConstants::GATT_DESCRIPTOR_INTERFACE,
        BlueZConstants::PROPERTY_VALUE,
        "ay",
        [this]() -> std::vector<uint8_t> {
            std::lock_guard<std::mutex> lock(valueMutex);
            return value;
        }
    );
    
    // Flags 속성 등록 - BlueZ 5.82에서 중요한 필수 속성
    object.registerProperty(
        BlueZConstants::GATT_DESCRIPTOR_INTERFACE,
        BlueZConstants::PROPERTY_FLAGS,
        "as",
        [this]() -> std::vector<std::string> {
            std::vector<std::string> flags;
            
            // 권한에 따라 플래그 추가
            if (permissions & GattPermission::PERM_READ) {
                flags.push_back(BlueZConstants::FLAG_READ);
            }
            if (permissions & GattPermission::PERM_WRITE) {
                flags.push_back(BlueZConstants::FLAG_WRITE);
            }
            if (permissions & GattPermission::PERM_READ_ENCRYPTED) {
                flags.push_back(BlueZConstants::FLAG_ENCRYPT_READ);
            }
            if (permissions & GattPermission::PERM_WRITE_ENCRYPTED){
                flags.push_back(BlueZConstants::FLAG_ENCRYPT_WRITE);
            }
            if (permissions & GattPermission::PERM_READ_AUTHENTICATED) {
                flags.push_back(BlueZConstants::FLAG_ENCRYPT_AUTHENTICATED_READ);
            }
            if (permissions & GattPermission::PERM_WRITE_AUTHENTICATED) {
                flags.push_back(BlueZConstants::FLAG_ENCRYPT_AUTHENTICATED_WRITE);
            }

            if (flags.empty()) {
                flags.push_back(BlueZConstants::FLAG_READ);  // 기본 권한 추가
                Logger::warn("설명자 권한이 비어 있어 기본값 'read'로 설정");
            }
            
            return flags;
        }
    );
    
    // ReadValue 메서드 등록 - BlueZ 5.82 표준 메서드
    object.registerReadValueMethod(
        BlueZConstants::GATT_DESCRIPTOR_INTERFACE,
        [this](const std::map<std::string, sdbus::Variant>& options) -> std::vector<uint8_t> {
            return handleReadValue(options);
        }
    );
    
    // WriteValue 메서드 등록 - BlueZ 5.82 표준 메서드
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
    
    // 옵션 처리 (BlueZ 5.82 호환)
    uint16_t offset = 0;
    if (options.count("offset") > 0) {
        try {
            offset = options.at("offset").get<uint16_t>();
            Logger::debug("읽기 오프셋: " + std::to_string(offset));
        } catch (...) {
            // 타입 변환 오류 무시
        }
    }
    
    // BlueZ 5.82의 추가 옵션 처리
    if (options.count("device") > 0) {
        try {
            std::string device = options.at("device").get<std::string>();
            Logger::debug("읽기 요청 장치: " + device);
        } catch (...) {
            // 타입 변환 오류 무시
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
                Logger::error("설명자 읽기 콜백에서 예외: " + std::string(e.what()));
                throw sdbus::Error("org.bluez.Error.Failed", e.what());
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
    
    return returnValue;
}

void GattDescriptor::handleWriteValue(const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options) {
    Logger::debug("설명자에 대해 WriteValue 호출됨: " + uuid.toString());
    
    // 옵션 처리 (BlueZ 5.82 호환)
    uint16_t offset = 0;
    if (options.count("offset") > 0) {
        try {
            offset = options.at("offset").get<uint16_t>();
            Logger::debug("쓰기 오프셋: " + std::to_string(offset));
        } catch (...) {
            // 타입 변환 오류 무시
        }
    }
    
    // BlueZ 5.82의 추가 옵션 처리
    if (options.count("device") > 0) {
        try {
            std::string device = options.at("device").get<std::string>();
            Logger::debug("쓰기 요청 장치: " + device);
        } catch (...) {
            // 타입 변환 오류 무시
        }
    }
    
    // CCCD (UUID 0x2902) 특별 처리 - BlueZ 5.82에서 중요
    if (uuid.toBlueZShortFormat() == "00002902") {
        Logger::debug("CCCD 설명자 쓰기 처리");
        
        // BlueZ 5.82에서는 StartNotify/StopNotify 호출 시 이 설명자를 자동으로 처리하지만
        // 클라이언트가 직접 이 설명자에 쓰기를 할 수도 있음
    }
    
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
        if (offset > 0) {
            // offset 위치에 쓰기
            std::lock_guard<std::mutex> valueLock(valueMutex);
            
            // 필요시 기존 값 확장
            if (offset >= this->value.size()) {
                this->value.resize(offset + value.size(), 0);
            }
            
            // offset 위치에 새 값 복사
            std::copy(value.begin(), value.end(), this->value.begin() + offset);
            
            // 값 변경 알림 발생
            if (object.isRegistered()) {
                object.emitPropertyChanged(BlueZConstants::GATT_DESCRIPTOR_INTERFACE, BlueZConstants::PROPERTY_VALUE);
            }
        } else {
            // 전체 값 설정 (알림 및 CCCD 처리 포함)
            setValue(value);
        }
    } else {
        // 콜백이 실패 반환
        throw sdbus::Error("org.bluez.Error.Failed", "Write operation failed");
    }
}

} // namespace ggk