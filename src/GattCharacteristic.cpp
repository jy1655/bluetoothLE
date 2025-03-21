#include "GattCharacteristic.h"
#include "GattService.h"
#include "GattDescriptor.h"
#include "Logger.h"
#include "Utils.h"
#include "DBusMessage.h"

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
        // 값 설정 및 잠금 관리
        {
            std::lock_guard<std::mutex> lock(valueMutex);
            value = newValue;
        }
        
        // 값 변경 시 D-Bus 속성 변경 알림
        if (isRegistered()) {
            // 스마트 포인터 기반 함수 사용
            auto valueVariant = Utils::gvariantPtrFromByteArray(value.data(), value.size());
            
            if (!valueVariant) {
                Logger::error("Failed to create GVariant for value");
                return;
            }
            
            bool isNotifying = false;
            {
                std::lock_guard<std::mutex> notifyLock(notifyMutex);
                isNotifying = notifying;
            }
            
            // Notify 활성화된 경우 시그널 발생
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
            
            // Value 속성 변경 알림 - 속성이 공개되어 있는 경우에만
            emitPropertyChanged(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, "Value", std::move(valueVariant));
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in setValue: " + std::string(e.what()));
    }
}

// 이동 시맨틱스를 사용한 오버로드 추가
void GattCharacteristic::setValue(std::vector<uint8_t>&& newValue) {
    try {
        // 값 설정 및 잠금 관리 (이동 의미론 활용)
        {
            std::lock_guard<std::mutex> lock(valueMutex);
            value = std::move(newValue);
        }
        
        // 값 변경 시 D-Bus 속성 변경 알림
        if (isRegistered()) {
            // 스마트 포인터 기반 함수 사용
            auto valueVariant = Utils::gvariantPtrFromByteArray(value.data(), value.size());
            
            if (!valueVariant) {
                Logger::error("Failed to create GVariant for value");
                return;
            }
            
            bool isNotifying = false;
            {
                std::lock_guard<std::mutex> notifyLock(notifyMutex);
                isNotifying = notifying;
            }
            
            // Notify 활성화된 경우 시그널 발생
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
            
            // Value 속성 변경 알림 - 속성이 공개되어 있는 경우에만
            emitPropertyChanged(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, "Value", std::move(valueVariant));
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in setValue: " + std::string(e.what()));
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
    
    Logger::debug("Creating descriptor UUID: " + uuid.toString() + 
                 ", permissions: " + Utils::hex(permissions));

    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(descriptorsMutex);
    
    // 이미 존재하는 경우 기존 설명자 반환
    auto it = descriptors.find(uuidStr);
    if (it != descriptors.end()) {
        if (!it->second) {
            Logger::error("Found null descriptor entry for UUID: " + uuidStr);
            descriptors.erase(it);  // 잘못된 항목 제거
        } else {
            return it->second;
        }
    }
    
    try {
        // 새 경로 생성
        std::string descNum = "desc" + std::to_string(descriptors.size() + 1);
        DBusObjectPath descriptorPath = getPath() + descNum;
        
        // 설명자 생성
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
        
        // 이 부분 제거 - setupDBusInterfaces 호출하지 않음
        // if (!descriptor->setupDBusInterfaces()) {
        //    Logger::error("Failed to setup descriptor interfaces for: " + uuidStr);
        //    return nullptr;
        // }
        
        // 맵에 추가
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
        return true;  // 이미 알림 중
    }
    
    if (!(properties & GattProperty::PROP_NOTIFY) &&
        !(properties & GattProperty::PROP_INDICATE)) {
        Logger::error("Characteristic does not support notifications: " + uuid.toString());
        return false;
    }
    
    notifying = true;
    
    // 알림 상태 변경 이벤트 발생
    if (isRegistered()) {
        // 스마트 포인터 기반 함수 사용
        auto valueVariant = Utils::gvariantPtrFromBoolean(true);
        
        if (!valueVariant) {
            Logger::error("Failed to create GVariant for notification state");
            notifying = false;
            return false;
        }
        
        emitPropertyChanged(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, "Notifying", std::move(valueVariant));
    }
    
    // 콜백 호출
    std::lock_guard<std::mutex> callbackLock(callbackMutex);
    if (notifyCallback) {
        try {
            notifyCallback();
        } catch (const std::exception& e) {
            Logger::error("Exception in notify callback: " + std::string(e.what()));
            // 콜백에서 예외가 발생해도 알림 상태는 계속 유지
        }
    }
    
    Logger::info("Started notifications for: " + uuid.toString());
    return true;
}

bool GattCharacteristic::stopNotify() {
    std::lock_guard<std::mutex> lock(notifyMutex);
    
    if (!notifying) {
        return true;  // 이미 알림 중지 상태
    }
    
    notifying = false;
    
    // 알림 상태 변경 이벤트 발생
    if (isRegistered()) {
        // 스마트 포인터 기반 함수 사용
        auto valueVariant = Utils::gvariantPtrFromBoolean(false);
        
        if (!valueVariant) {
            Logger::error("Failed to create GVariant for notification state");
            notifying = true;  // 원래 상태로 복원
            return false;
        }
        
        emitPropertyChanged(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, "Notifying", std::move(valueVariant));
    }
    
    Logger::info("Stopped notifications for: " + uuid.toString());
    return true;
}

// GattCharacteristic 클래스에 추가:
void GattCharacteristic::ensureCCCDExists() {
    if (properties & GattProperty::PROP_NOTIFY || properties & GattProperty::PROP_INDICATE) {
        // CCCD가 이미 존재하는지 확인
        bool hasCccd = false;
        std::lock_guard<std::mutex> lock(descriptorsMutex);
        for (const auto& pair : descriptors) {
            if (pair.second && pair.second->getUuid().toBlueZShortFormat() == "00002902") {
                hasCccd = true;
                break;
            }
        }
        
        // CCCD가 없으면 생성
        if (!hasCccd) {
            Logger::info("CCCD 자동 추가 - 특성: " + uuid.toString());
            GattDescriptorPtr cccd = createDescriptor(
                GattUuid::fromShortUuid(0x2902),
                GattPermission::PERM_READ | GattPermission::PERM_WRITE
            );
            
            if (cccd) {
                std::vector<uint8_t> initialValue = {0x00, 0x00};
                cccd->setValue(initialValue);
            }
        }
    }
}

bool GattCharacteristic::setupDBusInterfaces() {
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

    ensureCCCDExists();
    
    // 인터페이스 추가
    if (!addInterface(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, properties)) {
        Logger::error("Failed to add characteristic interface");
        return false;
    }
    
    // 메서드 핸들러 등록
    if (!addMethod(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, "ReadValue", 
                  [this](const DBusMethodCall& call) { handleReadValue(call); })) {
        Logger::error("Failed to add ReadValue method");
        return false;
    }
    
    if (!addMethod(BlueZConstants::GATT_CHARACTERISTIC_INTERFACE, "WriteValue", 
                  [this](const DBusMethodCall& call) { handleWriteValue(call); })) {
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
    
    // 객체 등록
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
                // 콜백이 없으면 저장된 값 반환
                std::lock_guard<std::mutex> valueLock(valueMutex);
                returnValue = value;
            }
        }
        
        // 결과 반환
        auto resultVariant = Utils::gvariantPtrFromByteArray(returnValue.data(), returnValue.size());
        
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
        
        // 메서드 응답 생성 및 전송
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
            // 성공적으로 처리됨
            {
                std::lock_guard<std::mutex> valueLock(valueMutex);
                value = newValue;
            }
            
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
        // 성공 응답
        g_dbus_method_invocation_return_value(call.invocation.get(), nullptr);
    } else {
        // 실패 응답
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
        // 성공 응답
        g_dbus_method_invocation_return_value(call.invocation.get(), nullptr);
    } else {
        // 실패 응답
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
        // 스마트 포인터 기반 함수 사용
        auto variantPtr = Utils::gvariantPtrFromString(uuid.toBlueZFormat());
        // 소유권 이전 - 호출자가 참조 카운트 관리 책임
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getUuidProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattCharacteristic::getServiceProperty() {
    try {
        // 스마트 포인터 기반 함수 사용
        auto variantPtr = Utils::gvariantPtrFromObject(service.getPath());
        // 소유권 이전 - 호출자가 참조 카운트 관리 책임
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getServiceProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattCharacteristic::getPropertiesProperty() {
    try {
        std::vector<std::string> flags;
        
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
        
        // 권한 기반 추가 플래그
        if (permissions & GattPermission::PERM_READ_ENCRYPTED) {
            flags.push_back("encrypt-read");
        }
        if (permissions & GattPermission::PERM_WRITE_ENCRYPTED) {
            flags.push_back("encrypt-write");
        }
        
        // 스마트 포인터 기반 함수 사용
        auto variantPtr = Utils::gvariantPtrFromStringArray(flags);
        // 소유권 이전 - 호출자가 참조 카운트 관리 책임
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
            if (pair.second) {  // nullptr 체크
                paths.push_back(pair.second->getPath().toString());
            }
        }
        
        // 스마트 포인터 기반 함수 사용
        auto variantPtr = Utils::gvariantPtrFromStringArray(paths);
        // 소유권 이전 - 호출자가 참조 카운트 관리 책임
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getDescriptorsProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattCharacteristic::getNotifyingProperty() {
    try {
        std::lock_guard<std::mutex> lock(notifyMutex);
        // 스마트 포인터 기반 함수 사용
        auto variantPtr = Utils::gvariantPtrFromBoolean(notifying);
        // 소유권 이전 - 호출자가 참조 카운트 관리 책임
        return g_variant_ref(variantPtr.get());
    } catch (const std::exception& e) {
        Logger::error("Exception in getNotifyingProperty: " + std::string(e.what()));
        return nullptr;
    }
}

} // namespace ggk