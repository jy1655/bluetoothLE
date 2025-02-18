#include "GattCharacteristic.h"
#include "GattService.h"
#include "GattDescriptor.h"
#include "Logger.h"
#include "DBusMethod.h"

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
    addDBusProperty("UUID", "s", true, false,
        [](void* userData) -> GVariant* {
            auto* self = static_cast<GattCharacteristic*>(userData);
            return g_variant_new_string(self->uuid.toString128().c_str());
        }, nullptr);

    // Service property
    addDBusProperty("Service", "o", true, false,
        [](void* userData) -> GVariant* {
            auto* self = static_cast<GattCharacteristic*>(userData);
            return g_variant_new_object_path(self->service->getPath().c_str());
        }, nullptr);

    // Flags property
    addDBusProperty("Flags", "as", true, false,
        [](void* userData) -> GVariant* {
            auto* self = static_cast<GattCharacteristic*>(userData);
            return g_variant_new_string(self->getPropertyFlags().c_str());
        }, nullptr);

    // Notifying property
    addDBusProperty("Notifying", "b", true, false,
        [](void* userData) -> GVariant* {
            auto* self = static_cast<GattCharacteristic*>(userData);
            return g_variant_new_boolean(self->notifying);
        }, nullptr);
}


void GattCharacteristic::setupMethods() {
    // ReadValue method
    const char* readValueInArgs[] = { "a{sv}", nullptr };
    addDBusMethod("ReadValue", readValueInArgs, "ay", onReadValue);

    // WriteValue method
    const char* writeValueInArgs[] = { "ay", "a{sv}", nullptr };
    addDBusMethod("WriteValue", writeValueInArgs, "", onWriteValue);

    // StartNotify method
    const char* startNotifyInArgs[] = { nullptr };
    addDBusMethod("StartNotify", startNotifyInArgs, "", onStartNotify);

    // StopNotify method
    const char* stopNotifyInArgs[] = { nullptr };
    addDBusMethod("StopNotify", stopNotifyInArgs, "", onStopNotify);
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

void GattCharacteristic::addFlag(GattProperty::Flags flag) {
    properties.set(flag);
}

bool GattCharacteristic::hasFlag(GattProperty::Flags flag) const {
    return properties.test(flag);
}

std::string GattCharacteristic::getPropertyFlags() const {
    std::vector<std::string> flags;
    
    if (hasFlag(GattProperty::Flags::BROADCAST))
        flags.push_back("broadcast");
    if (hasFlag(GattProperty::Flags::READ))
        flags.push_back("read");
    if (hasFlag(GattProperty::Flags::WRITE_WITHOUT_RESPONSE))
        flags.push_back("write-without-response");
    if (hasFlag(GattProperty::Flags::WRITE))
        flags.push_back("write");
    if (hasFlag(GattProperty::Flags::NOTIFY))
        flags.push_back("notify");
    if (hasFlag(GattProperty::Flags::INDICATE))
        flags.push_back("indicate");
    
    std::string result;
    for (size_t i = 0; i < flags.size(); ++i) {
        if (i > 0) result += ",";
        result += flags[i];
    }
    return result;
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

void GattCharacteristic::addDBusProperty(const char* name,
        const char* type, bool readable, bool writable, GVariant* (*getter)(void*),
        void (*setter)(GVariant*, void*)) {
    auto property = std::make_shared<DBusProperty>(name, type, readable,
                writable, getter, setter, this);
    DBusInterface::addProperty(property);
}

void GattCharacteristic::addDBusMethod(const char* name,
    const char** inArgs,
    const char* outArgs,
    DBusMethod::Callback callback) {
    auto method = std::make_shared<DBusMethod>(this, name, inArgs, outArgs, callback);
    DBusInterface::addMethod(method);
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