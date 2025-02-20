#pragma once

#include <gio/gio.h>
#include <memory>
#include <vector>
#include <optional>
#include <functional>
#include "DBusInterface.h"
#include "GattUuid.h"
#include "GattProperty.h"

namespace ggk {

class GattDescriptor : public DBusInterface {
public:
    // BlueZ GATT Descriptor Interface
    static const char* INTERFACE_NAME;    // "org.bluez.GattDescriptor1"

    // 필수/선택적 디스크립터 타입 구분
    enum class Type {
        EXTENDED_PROPERTIES,      // Optional: 0x2900
        USER_DESCRIPTION,        // Optional: 0x2901
        CLIENT_CHAR_CONFIG,      // Conditional: 0x2902 (필요한 경우에만)
        SERVER_CHAR_CONFIG,      // Optional: 0x2903
        PRESENTATION_FORMAT,     // Optional: 0x2904
        AGGREGATE_FORMAT,        // Optional: 0x2905
        CUSTOM                   // Custom descriptor (항상 Optional)
    };

    enum class Requirement {
        MANDATORY,       // 반드시 필요
        OPTIONAL,        // 선택적
        CONDITIONAL      // 조건부 (예: CCCD는 notify/indicate가 지원될 때만)
    };

    // 콜백 타입 정의
    using ValueChangedCallback = std::function<void(const std::vector<uint8_t>&)>;
    using CCCDCallback = std::function<void(bool notification, bool indication)>;

    // 생성자 - path는 선택적 (동적 생성 가능)
    explicit GattDescriptor(Type type, const DBusObjectPath& path = DBusObjectPath());
    GattDescriptor(const GattUuid& uuid, const DBusObjectPath& path = DBusObjectPath());
    virtual ~GattDescriptor() = default;

    // DBusInterface 구현
    virtual DBusObjectPath getPath() const override { return objectPath; }

    // 디스크립터 속성
    const GattUuid& getUUID() const { return uuid; }
    Type getType() const { return type; }
    
    // 필수/선택 여부 확인
    Requirement getRequirement() const;
    bool isRequired() const { return getRequirement() == Requirement::MANDATORY; }
    bool isOptional() const { return getRequirement() == Requirement::OPTIONAL; }
    bool isConditional() const { return getRequirement() == Requirement::CONDITIONAL; }

    // 조건부 디스크립터 검증
    bool isConditionallyRequired(bool hasNotify, bool hasIndicate) const;

    // 값 관리 (모든 디스크립터는 값이 optional)
    std::optional<std::vector<uint8_t>> getValue() const { return value; }
    bool setValue(const std::vector<uint8_t>& newValue);
    bool hasValue() const { return value.has_value(); }
    void clearValue() { value.reset(); }

    // 콜백 설정
    void setValueChangedCallback(ValueChangedCallback callback) {
        onValueChangedCallback = std::move(callback);
    }
    
    void setCCCDCallback(CCCDCallback callback) {
        onCCCDCallback = std::move(callback);
    }

    // CCCD 특화 기능
    bool isNotificationEnabled() const;
    bool isIndicationEnabled() const;

    // DBus 객체 등록 여부
    bool isRegistered() const { return registered; }
    void setRegistered(bool reg) { registered = reg; }

    // 표준 UUID 매핑
    static const std::map<Type, GattUuid> TYPE_TO_UUID;
    static const std::map<Type, Requirement> TYPE_TO_REQUIREMENT;

    static const std::map<Type, GattUuid>& getTypeToUuidMap() {
        return TYPE_TO_UUID;
    }

    void addDBusProperty(const char* name,
        const char* type,
        bool readable,
        bool writable,
        std::function<GVariant*(void*)> getter = nullptr,
        std::function<void(GVariant*, void*)> setter = nullptr);

private:
    GattUuid uuid;
    DBusObjectPath objectPath;
    std::optional<std::vector<uint8_t>> value;
    Type type;
    bool registered{false};  // DBus에 등록되었는지 여부

    // 콜백
    ValueChangedCallback onValueChangedCallback;
    CCCDCallback onCCCDCallback;

    void setupProperties();
    void setupMethods();
    bool validateValue(const std::vector<uint8_t>& newValue) const;

    // D-Bus 메서드 핸들러
    static void onReadValue(const DBusInterface& interface,
                          GDBusConnection* connection,
                          const std::string& methodName,
                          GVariant* parameters,
                          GDBusMethodInvocation* invocation,
                          void* userData);

    static void onWriteValue(const DBusInterface& interface,
                           GDBusConnection* connection,
                           const std::string& methodName,
                           GVariant* parameters,
                           GDBusMethodInvocation* invocation,
                           void* userData);
};

} // namespace ggk