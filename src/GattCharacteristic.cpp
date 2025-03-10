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
    value = newValue;
    
    // 값 변경 시 D-Bus 속성 변경 알림
    if (isRegistered()) {
        GVariantPtr valueVariant = GVariantPtr(
            Utils::gvariantFromByteArray(value.data(), value.size()),
            &g_variant_unref
        );
        
        // Notify 활성화된 경우 시그널 발생
        if (notifying && notifyCallback) {
            notifyCallback();
        }
        
        // Value 속성 변경 알림 - 속성이 공개되어 있는 경우에만
        emitPropertyChanged(CHARACTERISTIC_INTERFACE, "Value", std::move(valueVariant));
    }
}

GattDescriptorPtr GattCharacteristic::createDescriptor(
    const GattUuid& uuid,
    uint8_t permissions
) {
    Logger::debug("Creating descriptor UUID: " + uuid.toString() + 
                 ", permissions: " + Utils::hex(permissions));

    std::string uuidStr = uuid.toString();
    
    // 이미 존재하는 경우 기존 설명자 반환
    auto it = descriptors.find(uuidStr);
    if (it != descriptors.end()) {
        return it->second;
    }
    
    // 새 경로 생성
    DBusObjectPath descriptorPath = getPath() + "/desc" + std::to_string(descriptors.size() + 1);
    
    // 설명자 생성
    GattDescriptorPtr descriptor = std::make_shared<GattDescriptor>(
        getConnection(),
        descriptorPath,
        uuid,
        *this,
        permissions
    );
    
    Logger::debug("About to setup descriptor DBus interfaces");
    // 설명자 등록
    if (!descriptor->setupDBusInterfaces()) {
        Logger::error("Failed to setup descriptor interfaces for: " + uuidStr);
        return nullptr;
    }
    
    // 맵에 추가
    descriptors[uuidStr] = descriptor;
    
    Logger::info("Created descriptor: " + uuidStr + " at path: " + descriptorPath.toString());
    return descriptor;
}

GattDescriptorPtr GattCharacteristic::getDescriptor(const GattUuid& uuid) const {
    std::string uuidStr = uuid.toString();
    auto it = descriptors.find(uuidStr);
    
    if (it != descriptors.end()) {
        return it->second;
    }
    
    return nullptr;
}

bool GattCharacteristic::startNotify() {
    if (notifying) {
        return true;  // 이미 알림 중
    }
    
    if (!(properties & static_cast<uint8_t>(GattProperty::NOTIFY)) &&
        !(properties & static_cast<uint8_t>(GattProperty::INDICATE))) {
        Logger::error("Characteristic does not support notifications: " + uuid.toString());
        return false;
    }
    
    notifying = true;
    
    // 알림 상태 변경 이벤트 발생
    if (isRegistered()) {
        GVariantPtr valueVariant = GVariantPtr(
            Utils::gvariantFromBoolean(true),
            &g_variant_unref
        );
        emitPropertyChanged(CHARACTERISTIC_INTERFACE, "Notifying", std::move(valueVariant));
    }
    
    // 콜백 호출
    if (notifyCallback) {
        notifyCallback();
    }
    
    Logger::info("Started notifications for: " + uuid.toString());
    return true;
}

bool GattCharacteristic::stopNotify() {
    if (!notifying) {
        return true;  // 이미 알림 중지 상태
    }
    
    notifying = false;
    
    // 알림 상태 변경 이벤트 발생
    if (isRegistered()) {
        GVariantPtr valueVariant = GVariantPtr(
            Utils::gvariantFromBoolean(false),
            &g_variant_unref
        );
        emitPropertyChanged(CHARACTERISTIC_INTERFACE, "Notifying", std::move(valueVariant));
    }
    
    Logger::info("Stopped notifications for: " + uuid.toString());
    return true;
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
    
    // 인터페이스 추가
    if (!addInterface(CHARACTERISTIC_INTERFACE, properties)) {
        Logger::error("Failed to add characteristic interface");
        return false;
    }
    
    // 메서드 핸들러 등록 - const 참조로 수정
    if (!addMethod(CHARACTERISTIC_INTERFACE, "ReadValue", 
                  [this](const DBusMethodCall& call) { handleReadValue(call); })) {
        Logger::error("Failed to add ReadValue method");
        return false;
    }
    
    if (!addMethod(CHARACTERISTIC_INTERFACE, "WriteValue", 
                  [this](const DBusMethodCall& call) { handleWriteValue(call); })) {
        Logger::error("Failed to add WriteValue method");
        return false;
    }
    
    if (!addMethod(CHARACTERISTIC_INTERFACE, "StartNotify", 
                  [this](const DBusMethodCall& call) { handleStartNotify(call); })) {
        Logger::error("Failed to add StartNotify method");
        return false;
    }
    
    if (!addMethod(CHARACTERISTIC_INTERFACE, "StopNotify", 
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
    Logger::debug("ReadValue called for characteristic: " + uuid.toString());
    
    // 옵션 파라미터 처리 (예: offset)
    // 실제 구현에서는 이 부분을 확장해야 함
    
    std::vector<uint8_t> returnValue;
    
    // 콜백이 있으면 호출
    if (readCallback) {
        returnValue = readCallback();
    } else {
        // 콜백이 없으면 저장된 값 반환
        returnValue = value;
    }
    
    // 결과 반환
    GVariantPtr resultVariant = GVariantPtr(
        Utils::gvariantFromByteArray(returnValue.data(), returnValue.size()),
        &g_variant_unref
    );
    
    // callMethod를 통해 메서드 리턴 (실제 연결에 따라 수정 필요)
    // 대안 1: DBusMessage 사용하여 메서드 응답 생성
    try {
        // 메서드 응답 생성 및 전송
        // 여기서 call.invocation은 GDBusMethodInvocationPtr이어야 함
        if (call.invocation) {
            // DBusConnection에 직접 전송 - 헤더 파일을 참조하여 적절한 메서드 사용
            // 예를 들어, GDBusMethodInvocation_return_value와 같은 기본 함수 사용
            g_dbus_method_invocation_return_value(call.invocation.get(), resultVariant.get());
        } else {
            Logger::error("Invalid method invocation in ReadValue");
        }
    } catch (const std::exception& e) {
        Logger::error(std::string("Exception in ReadValue: ") + e.what());
        if (call.invocation) {
            // 오류 응답
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(), 
                G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED,
                e.what()
            );
        }
    }
}

void GattCharacteristic::handleWriteValue(const DBusMethodCall& call) {
    Logger::debug("WriteValue called for characteristic: " + uuid.toString());
    
    // 파라미터 확인
    if (!call.parameters) {
        Logger::error("Missing parameters for WriteValue");
        
        // 오류 응답
        if (call.invocation) {
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(), 
                G_DBUS_ERROR,
                G_DBUS_ERROR_INVALID_ARGS,
                "Missing parameters"
            );
        }
        return;
    }
    
    // 바이트 배열 파라미터 추출
    try {
        std::string byteString = Utils::stringFromGVariantByteArray(call.parameters.get());
        std::vector<uint8_t> newValue(byteString.begin(), byteString.end());
        
        // 콜백이 있으면 호출
        bool success = true;
        if (writeCallback) {
            success = writeCallback(newValue);
        }
        
        if (success) {
            // 성공적으로 처리됨
            value = newValue;
            
            // 빈 응답 생성 및 전송
            if (call.invocation) {
                g_dbus_method_invocation_return_value(call.invocation.get(), nullptr);
            }
        } else {
            // 콜백에서 실패 반환
            if (call.invocation) {
                g_dbus_method_invocation_return_error_literal(
                    call.invocation.get(), 
                    G_DBUS_ERROR,
                    G_DBUS_ERROR_FAILED,
                    "Write operation failed"
                );
            }
        }
    } catch (const std::exception& e) {
        Logger::error(std::string("Failed to parse WriteValue parameters: ") + e.what());
        
        if (call.invocation) {
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(), 
                G_DBUS_ERROR,
                G_DBUS_ERROR_INVALID_ARGS,
                "Invalid parameters"
            );
        }
    }
}

void GattCharacteristic::handleStartNotify(const DBusMethodCall& call) {
    Logger::debug("StartNotify called for characteristic: " + uuid.toString());
    
    if (startNotify()) {
        // 성공 응답
        if (call.invocation) {
            g_dbus_method_invocation_return_value(call.invocation.get(), nullptr);
        }
    } else {
        // 실패 응답
        if (call.invocation) {
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(), 
                G_DBUS_ERROR,
                G_DBUS_ERROR_NOT_SUPPORTED,
                "Notifications not supported"
            );
        }
    }
}

void GattCharacteristic::handleStopNotify(const DBusMethodCall& call) {
    Logger::debug("StopNotify called for characteristic: " + uuid.toString());
    
    if (stopNotify()) {
        // 성공 응답
        if (call.invocation) {
            g_dbus_method_invocation_return_value(call.invocation.get(), nullptr);
        }
    } else {
        // 실패 응답
        if (call.invocation) {
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(), 
                G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED,
                "Failed to stop notifications"
            );
        }
    }
}

GVariant* GattCharacteristic::getUuidProperty() {
    return Utils::gvariantFromString(uuid.toBlueZFormat());
}

GVariant* GattCharacteristic::getServiceProperty() {
    return Utils::gvariantFromObject(service.getPath());
}

GVariant* GattCharacteristic::getPropertiesProperty() {
    std::vector<std::string> flags;
    
    if (properties & static_cast<uint8_t>(GattProperty::BROADCAST)) {
        flags.push_back("broadcast");
    }
    if (properties & static_cast<uint8_t>(GattProperty::READ)) {
        flags.push_back("read");
    }
    if (properties & static_cast<uint8_t>(GattProperty::WRITE_WITHOUT_RESPONSE)) {
        flags.push_back("write-without-response");
    }
    if (properties & static_cast<uint8_t>(GattProperty::WRITE)) {
        flags.push_back("write");
    }
    if (properties & static_cast<uint8_t>(GattProperty::NOTIFY)) {
        flags.push_back("notify");
    }
    if (properties & static_cast<uint8_t>(GattProperty::INDICATE)) {
        flags.push_back("indicate");
    }
    if (properties & static_cast<uint8_t>(GattProperty::AUTHENTICATED_SIGNED_WRITES)) {
        flags.push_back("authenticated-signed-writes");
    }
    
    // 권한 기반 추가 플래그
    if (permissions & static_cast<uint8_t>(GattPermission::READ_ENCRYPTED)) {
        flags.push_back("encrypt-read");
    }
    if (permissions & static_cast<uint8_t>(GattPermission::WRITE_ENCRYPTED)) {
        flags.push_back("encrypt-write");
    }
    
    return Utils::gvariantFromStringArray(flags);
}

GVariant* GattCharacteristic::getDescriptorsProperty() {
    std::vector<std::string> paths;
    for (const auto& pair : descriptors) {
        paths.push_back(pair.second->getPath().toString());
    }
    
    return Utils::gvariantFromStringArray(paths);
}

GVariant* GattCharacteristic::getNotifyingProperty() {
    return Utils::gvariantFromBoolean(notifying);
}

} // namespace ggk