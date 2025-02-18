#include "GattDescriptor.h"
#include "GattCharacteristic.h"
#include "Logger.h"

namespace ggk {

const char* GattDescriptor::INTERFACE_NAME = "org.bluez.GattDescriptor1";

// 표준 디스크립터 UUID 정의
const GattUuid GattDescriptor::UUID_CHARAC_EXTENDED_PROPERTIES("2900");
const GattUuid GattDescriptor::UUID_CHARAC_USER_DESCRIPTION("2901");
const GattUuid GattDescriptor::UUID_CLIENT_CHARAC_CONFIG("2902");
const GattUuid GattDescriptor::UUID_SERVER_CHARAC_CONFIG("2903");
const GattUuid GattDescriptor::UUID_CHARAC_PRESENTATION_FORMAT("2904");
const GattUuid GattDescriptor::UUID_CHARAC_AGGREGATE_FORMAT("2905");

GattDescriptor::GattDescriptor(const GattUuid& uuid, GattCharacteristic* characteristic)
    : DBusInterface(INTERFACE_NAME)
    , uuid(uuid)
    , characteristic(characteristic) {
    
    setupProperties();
    setupMethods();
    Logger::debug("Created GATT descriptor: " + uuid.toString());
}

void GattDescriptor::setupProperties() {
    // UUID property
    addProperty("UUID", "s", true, false,
                [this](void) -> GVariant* {
                    return g_variant_new_string(uuid.toString128().c_str());
                },
                nullptr);

    // Characteristic property
    addProperty("Characteristic", "o", true, false,
                [this](void) -> GVariant* {
                    return g_variant_new_object_path(characteristic->getPath().c_str());
                },
                nullptr);
}

void GattDescriptor::setupMethods() {
    const char* readValueInArgs[] = { "a{sv}", nullptr };  // options dictionary
    const char* readValueOutArgs = "ay";  // byte array
    addMethod("ReadValue", readValueInArgs, readValueOutArgs, onReadValue);

    const char* writeValueInArgs[] = { "ay", "a{sv}", nullptr };  // value + options
    const char* writeValueOutArgs = "";
    addMethod("WriteValue", writeValueInArgs, writeValueOutArgs, onWriteValue);
}

bool GattDescriptor::setValue(const std::vector<uint8_t>& newValue) {
    if (value != newValue) {
        value = newValue;
        onValueChanged(value);
        return true;
    }
    return false;
}

void GattDescriptor::onReadValue(const DBusInterface& interface,
                               GDBusConnection* connection,
                               const std::string& methodName,
                               GVariant* parameters,
                               GDBusMethodInvocation* invocation,
                               void* userData) {
    auto* descriptor = static_cast<GattDescriptor*>(userData);
    if (!descriptor) {
        g_dbus_method_invocation_return_error_literal(invocation,
                                                     G_IO_ERROR,
                                                     G_IO_ERROR_FAILED,
                                                     "Invalid descriptor");
        return;
    }

    const std::vector<uint8_t>& value = descriptor->getValue();
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
    if (!descriptor) {
        g_dbus_method_invocation_return_error_literal(invocation,
                                                     G_IO_ERROR,
                                                     G_IO_ERROR_FAILED,
                                                     "Invalid descriptor");
        return;
    }

    GVariant* valueVariant;
    GVariant* optionsVariant;
    g_variant_get(parameters, "(@ay@a{sv})", &valueVariant, &optionsVariant);

    gsize n_elements;
    const void* data = g_variant_get_fixed_array(valueVariant, &n_elements, sizeof(guchar));
    
    std::vector<uint8_t> newValue(static_cast<const uint8_t*>(data),
                                 static_cast<const uint8_t*>(data) + n_elements);

    descriptor->setValue(newValue);
    
    g_variant_unref(valueVariant);
    g_variant_unref(optionsVariant);

    g_dbus_method_invocation_return_value(invocation, nullptr);

    // Client Characteristic Configuration Descriptor (CCCD) 특별 처리
    if (descriptor->getUUID() == UUID_CLIENT_CHARAC_CONFIG) {
        bool notify = (newValue.size() >= 2) && (newValue[0] & 0x01);
        bool indicate = (newValue.size() >= 2) && (newValue[0] & 0x02);

        if (notify || indicate) {
            descriptor->getCharacteristic()->onStartNotify(interface, connection, 
                                                         methodName, nullptr, 
                                                         invocation, descriptor->getCharacteristic());
        } else {
            descriptor->getCharacteristic()->onStopNotify(interface, connection,
                                                        methodName, nullptr,
                                                        invocation, descriptor->getCharacteristic());
        }
    }
}

void GattDescriptor::addManagedObjectProperties(GVariantBuilder* builder) {
    if (!builder) return;

    // 디스크립터 인터페이스 프로퍼티 추가
    GVariantBuilder interfaceBuilder;
    g_variant_builder_init(&interfaceBuilder, G_VARIANT_TYPE("a{sa{sv}}"));

    // GattDescriptor1 인터페이스
    GVariantBuilder propsBuilder;
    g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));

    // UUID
    g_variant_builder_add(&propsBuilder, "{sv}", "UUID",
                         g_variant_new_string(uuid.toString128().c_str()));

    // Characteristic
    g_variant_builder_add(&propsBuilder, "{sv}", "Characteristic",
                         g_variant_new_object_path(characteristic->getPath().c_str()));

    g_variant_builder_add(&interfaceBuilder, "{sa{sv}}",
                         INTERFACE_NAME,
                         &propsBuilder);

    g_variant_builder_add(builder, "a{sa{sv}}", &interfaceBuilder);
}

void GattDescriptor::onValueChanged(const std::vector<uint8_t>& newValue) {
    // 기본 구현은 아무것도 하지 않음
    // 하위 클래스에서 필요한 경우 오버라이드
}

} // namespace ggk