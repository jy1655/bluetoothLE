#include "GattCharacteristic.h"
#include "GattService.h"
#include "GattDescriptor.h"
#include "Logger.h"

namespace ggk {

const char* GattCharacteristic::INTERFACE_NAME = "org.bluez.GattCharacteristic1";

GattCharacteristic::GattCharacteristic(const GattUuid& uuid, GattService* service)
    : DBusInterface(INTERFACE_NAME)
    , uuid(uuid)
    , service(service)
    , notifying(false) {
    
    setupProperties();
    setupMethods();
    Logger::debug("Created GATT characteristic: " + uuid.toString());
}

void GattCharacteristic::setupProperties() {
    // UUID property
    addProperty("UUID", "s", true, false,
                [this](void) -> GVariant* {
                    return g_variant_new_string(uuid.toString128().c_str());
                },
                nullptr);

    // Service property
    addProperty("Service", "o", true, false,
                [this](void) -> GVariant* {
                    return g_variant_new_object_path(service->getPath().c_str());
                },
                nullptr);

    // Properties (flags) property
    addProperty("Flags", "as", true, false,
                [this](void) -> GVariant* {
                    return g_variant_new_string(getPropertyFlags().c_str());
                },
                nullptr);

    // Notifying property
    addProperty("Notifying", "b", true, false,
                [this](void) -> GVariant* {
                    return g_variant_new_boolean(notifying);
                },
                nullptr);
}

void GattCharacteristic::setupMethods() {
    const char* readValueInArgs[] = { "a{sv}", nullptr };  // options dictionary
    const char* readValueOutArgs = "ay";  // byte array
    addMethod("ReadValue", readValueInArgs, readValueOutArgs, onReadValue);

    const char* writeValueInArgs[] = { "ay", "a{sv}", nullptr };  // value + options
    const char* writeValueOutArgs = "";
    addMethod("WriteValue", writeValueInArgs, writeValueOutArgs, onWriteValue);

    const char* startNotifyInArgs[] = { nullptr };
    const char* startNotifyOutArgs = "";
    addMethod("StartNotify", startNotifyInArgs, startNotifyOutArgs, onStartNotify);

    const char* stopNotifyInArgs[] = { nullptr };
    const char* stopNotifyOutArgs = "";
    addMethod("StopNotify", stopNotifyInArgs, stopNotifyOutArgs, onStopNotify);
}

bool GattCharacteristic::setValue(const std::vector<uint8_t>& newValue) {
    if (value != newValue) {
        value = newValue;
        onValueChanged(value);
        
        if (notifying) {
            sendNotification();
        }
        return true;
    }
    return false;
}

void GattCharacteristic::addProperty(GattProperty::Flags flag) {
    properties.set(flag);
}

bool GattCharacteristic::hasProperty(GattProperty::Flags flag) const {
    return properties.test(flag);
}

bool GattCharacteristic::addDescriptor(std::shared_ptr<GattDescriptor> descriptor) {
    if (!descriptor) {
        Logger::error("Attempted to add null descriptor to characteristic: " + uuid.toString());
        return false;
    }

    for (const auto& existing : descriptors) {
        if (existing->getUUID() == descriptor->getUUID()) {
            Logger::error("Descriptor with UUID " + descriptor->getUUID().toString() + 
                         " already exists in characteristic: " + uuid.toString());
            return false;
        }
    }

    descriptors.push_back(descriptor);
    Logger::debug("Added descriptor " + descriptor->getUUID().toString() + 
                 " to characteristic: " + uuid.toString());
    return true;
}

std::shared_ptr<GattDescriptor> GattCharacteristic::getDescriptor(const GattUuid& uuid) {
    for (const auto& descriptor : descriptors) {
        if (descriptor->getUUID() == uuid) {
            return descriptor;
        }
    }
    return nullptr;
}

void GattCharacteristic::onReadValue(const DBusInterface& interface,
                                   GDBusConnection* connection,
                                   const std::string& methodName,
                                   GVariant* parameters,
                                   GDBusMethodInvocation* invocation,
                                   void* userData) {
    auto* characteristic = static_cast<GattCharacteristic*>(userData);
    if (!characteristic) {
        g_dbus_method_invocation_return_error_literal(invocation,
                                                     G_IO_ERROR,
                                                     G_IO_ERROR_FAILED,
                                                     "Invalid characteristic");
        return;
    }

    const std::vector<uint8_t>& value = characteristic->getValue();
    GVariant* result = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                               value.data(),
                                               value.size(),
                                               sizeof(uint8_t));
    g_dbus_method_invocation_return_value(invocation, g_variant_new_tuple(&result, 1));
}

void GattCharacteristic::onWriteValue(const DBusInterface& interface,
                                    GDBusConnection* connection,
                                    const std::string& methodName,
                                    GVariant* parameters,
                                    GDBusMethodInvocation* invocation,
                                    void* userData) {
    auto* characteristic = static_cast<GattCharacteristic*>(userData);
    if (!characteristic) {
        g_dbus_method_invocation_return_error_literal(invocation,
                                                     G_IO_ERROR,
                                                     G_IO_ERROR_FAILED,
                                                     "Invalid characteristic");
        return;
    }

    GVariant* valueVariant;
    GVariant* optionsVariant;
    g_variant_get(parameters, "(@ay@a{sv})", &valueVariant, &optionsVariant);

    gsize n_elements;
    const void* data = g_variant_get_fixed_array(valueVariant, &n_elements, sizeof(guchar));
    
    std::vector<uint8_t> newValue(static_cast<const uint8_t*>(data),
                                 static_cast<const uint8_t*>(data) + n_elements);

    characteristic->setValue(newValue);
    
    g_variant_unref(valueVariant);
    g_variant_unref(optionsVariant);

    g_dbus_method_invocation_return_value(invocation, nullptr);
}

void GattCharacteristic::onStartNotify(const DBusInterface& interface,
                                     GDBusConnection* connection,
                                     const std::string& methodName,
                                     GVariant* parameters,
                                     GDBusMethodInvocation* invocation,
                                     void* userData) {
    auto* characteristic = static_cast<GattCharacteristic*>(userData);
    if (!characteristic) {
        g_dbus_method_invocation_return_error_literal(invocation,
                                                     G_IO_ERROR,
                                                     G_IO_ERROR_FAILED,
                                                     "Invalid characteristic");
        return;
    }

    characteristic->notifying = true;
    characteristic->onNotifyingChanged(true);
    g_dbus_method_invocation_return_value(invocation, nullptr);
}

void GattCharacteristic::onStopNotify(const DBusInterface& interface,
                                    GDBusConnection* connection,
                                    const std::string& methodName,
                                    GVariant* parameters,
                                    GDBusMethodInvocation* invocation,
                                    void* userData) {
    auto* characteristic = static_cast<GattCharacteristic*>(userData);
    if (!characteristic) {
        g_dbus_method_invocation_return_error_literal(invocation,
                                                     G_IO_ERROR,
                                                     G_IO_ERROR_FAILED,
                                                     "Invalid characteristic");
        return;
    }

    characteristic->notifying = false;
    characteristic->onNotifyingChanged(false);
    g_dbus_method_invocation_return_value(invocation, nullptr);
}

void GattCharacteristic::sendNotification() {
    if (!notifying) return;

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder, "{sv}", "Value",
                         g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                                value.data(),
                                                value.size(),
                                                sizeof(uint8_t)));

    emitSignal(getConnection(),
               INTERFACE_NAME,
               "PropertiesChanged",
               g_variant_new("(sa{sv}as)",
                           INTERFACE_NAME,
                           &builder,
                           nullptr));
}

void GattCharacteristic::addManagedObjectProperties(GVariantBuilder* builder) {
    if (!builder) return;

    // 특성 인터페이스 프로퍼티 추가
    GVariantBuilder interfaceBuilder;
    g_variant_builder_init(&interfaceBuilder, G_VARIANT_TYPE("a{sa{sv}}"));

    // GattCharacteristic1 인터페이스
    GVariantBuilder propsBuilder;
    g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));

    // UUID
    g_variant_builder_add(&propsBuilder, "{sv}", "UUID",
                         g_variant_new_string(uuid.toString128().c_str()));

    // Service
    g_variant_builder_add(&propsBuilder, "{sv}", "Service",
                         g_variant_new_object_path(service->getPath().c_str()));

    // Flags
    g_variant_builder_add(&propsBuilder, "{sv}", "Flags",
                         g_variant_new_string(getPropertyFlags().c_str()));

    // Notifying
    g_variant_builder_add(&propsBuilder, "{sv}", "Notifying",
                         g_variant_new_boolean(notifying));

    g_variant_builder_add(&interfaceBuilder, "{sa{sv}}",
                         INTERFACE_NAME,
                         &propsBuilder);

    g_variant_builder_add(builder, "a{sa{sv}}", &interfaceBuilder);
}

void GattCharacteristic::onValueChanged(const std::vector<uint8_t>& newValue) {
    // 기본 구현은 아무것도 하지 않음
    // 하위 클래스에서 필요한 경우 오버라이드
}

void GattCharacteristic::onNotifyingChanged(bool isNotifying) {
    // 기본 구현은 아무것도 하지 않음
    // 하위 클래스에서 필요한 경우 오버라이드
}

} // namespace ggk