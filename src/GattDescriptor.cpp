#include <map>
#include "GattDescriptor.h"
#include "Logger.h"

namespace ggk {

const char* GattDescriptor::INTERFACE_NAME = "org.bluez.GattDescriptor1";

// 표준 UUID 매핑 정의
const std::map<GattDescriptor::Type, GattUuid> GattDescriptor::TYPE_TO_UUID = {
   {Type::EXTENDED_PROPERTIES,    GattUuid("2900")},
   {Type::USER_DESCRIPTION,       GattUuid("2901")},
   {Type::CLIENT_CHAR_CONFIG,     GattUuid("2902")},
   {Type::SERVER_CHAR_CONFIG,     GattUuid("2903")},
   {Type::PRESENTATION_FORMAT,    GattUuid("2904")},
   {Type::AGGREGATE_FORMAT,       GattUuid("2905")}
};

// 디스크립터 타입별 요구사항 매핑
const std::map<GattDescriptor::Type, GattDescriptor::Requirement> GattDescriptor::TYPE_TO_REQUIREMENT = {
   {Type::EXTENDED_PROPERTIES,    Requirement::OPTIONAL},
   {Type::USER_DESCRIPTION,       Requirement::OPTIONAL},
   {Type::CLIENT_CHAR_CONFIG,     Requirement::CONDITIONAL},
   {Type::SERVER_CHAR_CONFIG,     Requirement::OPTIONAL},
   {Type::PRESENTATION_FORMAT,    Requirement::OPTIONAL},
   {Type::AGGREGATE_FORMAT,       Requirement::OPTIONAL},
   {Type::CUSTOM,                 Requirement::OPTIONAL}
};

GattDescriptor::GattDescriptor(Type type, const DBusObjectPath& path)
   : DBusInterface(INTERFACE_NAME)
   , objectPath(path)
   , uuid(TYPE_TO_UUID.at(type))
   , type(type) {
   
   auto it = TYPE_TO_UUID.find(type);
   if (it != TYPE_TO_UUID.end()) {
       uuid = it->second;
       setupProperties();
       setupMethods();
       Logger::debug("Created standard GATT descriptor: " + uuid.toString());
   } else {
       Logger::error("Invalid descriptor type");
   }
}

GattDescriptor::GattDescriptor(const GattUuid& uuid, const DBusObjectPath& path)
   : DBusInterface(INTERFACE_NAME)
   , uuid(uuid)
   , objectPath(path)
   , type(Type::CUSTOM) {
   
   setupProperties();
   setupMethods();
   Logger::debug("Created custom GATT descriptor: " + uuid.toString());
}

GattDescriptor::Requirement GattDescriptor::getRequirement() const {
   auto it = TYPE_TO_REQUIREMENT.find(type);
   return (it != TYPE_TO_REQUIREMENT.end()) ? it->second : Requirement::OPTIONAL;
}

bool GattDescriptor::isConditionallyRequired(bool hasNotify, bool hasIndicate) const {
   if (type == Type::CLIENT_CHAR_CONFIG) {
       return hasNotify || hasIndicate;  // CCCD는 notify나 indicate가 있을 때만 필요
   }
   return false;
}

void GattDescriptor::setupProperties() {
   // UUID property
   addDBusProperty("UUID", "s", true, false,
    std::function<GVariant*(void*)>([this](void*) -> GVariant* {
        return g_variant_new_string(uuid.toString128().c_str());
    }));
}

void GattDescriptor::setupMethods() {
   // ReadValue method
   const char* readValueInArgs[] = { "a{sv}", nullptr };
   addMethod(std::make_shared<DBusMethod>(
       this,
       "ReadValue",
       readValueInArgs,
       "ay",
       onReadValue
   ));

   // WriteValue method
   const char* writeValueInArgs[] = { "ay", "a{sv}", nullptr };
   addMethod(std::make_shared<DBusMethod>(
       this,
       "WriteValue",
       writeValueInArgs,
       "",
       onWriteValue
   ));
}

bool GattDescriptor::setValue(const std::vector<uint8_t>& newValue) {
   if (!validateValue(newValue)) {
       Logger::error("Invalid value for descriptor: " + uuid.toString());
       return false;
   }

   bool changed = !value.has_value() || value.value() != newValue;
   if (changed) {
       value = newValue;
       
       if (onValueChangedCallback) {
           onValueChangedCallback(newValue);
       }

       if (type == Type::CLIENT_CHAR_CONFIG && onCCCDCallback) {
           onCCCDCallback(isNotificationEnabled(), isIndicationEnabled());
       }
   }
   return changed;
}

bool GattDescriptor::validateValue(const std::vector<uint8_t>& newValue) const {
   switch (type) {
       case Type::CLIENT_CHAR_CONFIG:
           if (newValue.size() != 2) return false;
           // CCCD 값 검증: 알려진 비트만 사용
           return (newValue[0] & 0xFC) == 0 && newValue[1] == 0;
           
       case Type::PRESENTATION_FORMAT:
           return newValue.size() == 7;  // Format + Exponent + Unit + Namespace + Description
           
       case Type::EXTENDED_PROPERTIES:
           return newValue.size() == 2;  // 16-bit flags
           
       default:
           return true;  // 다른 타입은 크기 제한 없음
   }
}

void GattDescriptor::onReadValue(const DBusInterface& interface,
                              GDBusConnection* connection,
                              const std::string& methodName,
                              GVariant* parameters,
                              GDBusMethodInvocation* invocation,
                              void* userData) {
   auto* descriptor = static_cast<GattDescriptor*>(userData);
   if (!descriptor || !descriptor->isRegistered()) {
       g_dbus_method_invocation_return_error_literal(invocation,
                                                    G_IO_ERROR,
                                                    G_IO_ERROR_FAILED,
                                                    "Invalid descriptor or not registered");
       return;
   }

   if (!descriptor->hasValue()) {
       g_dbus_method_invocation_return_error_literal(invocation,
                                                    G_IO_ERROR,
                                                    G_IO_ERROR_NOT_FOUND,
                                                    "No value available");
       return;
   }

   const auto& value = descriptor->getValue().value();
   GVariant* result = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                              value.data(),
                                              value.size(),
                                              sizeof(uint8_t));
   g_dbus_method_invocation_return_value(invocation, g_variant_new_tuple(&result, 1));
}

void GattDescriptor::onWriteValue(const DBusInterface& interface,
                               GDBusConnection* connection,
                               const std::string& methodName,
                               GVariant* parameters,
                               GDBusMethodInvocation* invocation,
                               void* userData) {
   auto* descriptor = static_cast<GattDescriptor*>(userData);
   if (!descriptor || !descriptor->isRegistered()) {
       g_dbus_method_invocation_return_error_literal(invocation,
                                                    G_IO_ERROR,
                                                    G_IO_ERROR_FAILED,
                                                    "Invalid descriptor or not registered");
       return;
   }

   GVariant* valueVariant;
   GVariant* optionsVariant;
   g_variant_get(parameters, "(@ay@a{sv})", &valueVariant, &optionsVariant);

   gsize n_elements;
   const void* data = g_variant_get_fixed_array(valueVariant, &n_elements, sizeof(guchar));
   
   std::vector<uint8_t> newValue(static_cast<const uint8_t*>(data),
                                static_cast<const uint8_t*>(data) + n_elements);

   if (descriptor->setValue(newValue)) {
       g_dbus_method_invocation_return_value(invocation, nullptr);
   } else {
       g_dbus_method_invocation_return_error_literal(invocation,
                                                    G_IO_ERROR,
                                                    G_IO_ERROR_INVALID_DATA,
                                                    "Invalid value for descriptor");
   }
   
   g_variant_unref(valueVariant);
   g_variant_unref(optionsVariant);
}

bool GattDescriptor::isNotificationEnabled() const {
   if (type != Type::CLIENT_CHAR_CONFIG || !value.has_value()) {
       return false;
   }
   const auto& cccd = value.value();
   return cccd.size() >= 2 && (cccd[0] & 0x01);
}

bool GattDescriptor::isIndicationEnabled() const {
   if (type != Type::CLIENT_CHAR_CONFIG || !value.has_value()) {
       return false;
   }
   const auto& cccd = value.value();
   return cccd.size() >= 2 && (cccd[0] & 0x02);
}

void GattDescriptor::addDBusProperty(const char* name,
    const char* type,
    bool readable,
    bool writable,
    std::function<GVariant*(void*)> getter,
    std::function<void(GVariant*, void*)> setter) {
    // 정적 함수로 변환
    GVariant* (*getterPtr)() = nullptr;
    void (*setterPtr)(GVariant*) = nullptr;

    if (getter) {
        static std::function<GVariant*(void*)> staticGetter = getter;
        getterPtr = []() -> GVariant* {
            return staticGetter(nullptr);
    };
    }

    if (setter) {
        static std::function<void(GVariant*, void*)> staticSetter = setter;
        setterPtr = [](GVariant* value) {
            staticSetter(value, nullptr);
        };
    }

    addProperty(name, type, readable, writable, getterPtr, setterPtr);
}

} // namespace ggk