#include "GattCharacteristic.h"
#include "Logger.h"

namespace ggk {

const char* GattCharacteristic::INTERFACE_NAME = "org.bluez.GattCharacteristic1";

GattCharacteristic::GattCharacteristic(const GattUuid& uuid, const DBusObjectPath& path)
    : DBusInterface(INTERFACE_NAME)
    , uuid(uuid)
    , objectPath(path) {
    
    setupProperties();
    setupMethods();
    Logger::debug("Created GATT characteristic: " + uuid.toString());
}

void GattCharacteristic::setupProperties() {
    // UUID property
    addDBusProperty("UUID", "s", true, false,
        std::function<GVariant*(void*)>([this](void*) -> GVariant* {
            return g_variant_new_string(uuid.toString128().c_str());
        }));

    // Flags property
    addDBusProperty("Flags", "as", true, false,
        std::function<GVariant*(void*)>([this](void*) -> GVariant* {
            return g_variant_new_string(getPropertyFlags().c_str());
        }));

    // Notifying property
    addDBusProperty("Notifying", "b", true, false,
        std::function<GVariant*(void*)>([this](void*) -> GVariant* {
            return g_variant_new_boolean(notifying);
        }));
}

void GattCharacteristic::setupMethods() {
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

    // StartNotify method
    addMethod(std::make_shared<DBusMethod>(
        this,
        "StartNotify",
        nullptr,
        "",
        onStartNotify
    ));

    // StopNotify method
    addMethod(std::make_shared<DBusMethod>(
        this,
        "StopNotify",
        nullptr,
        "",
        onStopNotify
    ));
}

void GattCharacteristic::addProperty(Property prop) {
    properties.set(static_cast<size_t>(prop));
    
    // Notify/Indicate가 추가되면 CCCD 생성
    if ((prop == Property::NOTIFY || prop == Property::INDICATE) && 
        !getDescriptor(GattDescriptor::TYPE_TO_UUID.at(GattDescriptor::Type::CLIENT_CHAR_CONFIG))) {
        auto cccd = createCCCD();
        if (cccd) {
            addDescriptor(cccd);
        }
    }
}

bool GattCharacteristic::hasProperty(Property prop) const {
    return properties.test(static_cast<size_t>(prop));
}

std::string GattCharacteristic::getPropertyFlags() const {
    std::vector<std::string> flags;
    
    if (hasProperty(Property::BROADCAST))
        flags.push_back("broadcast");
    if (hasProperty(Property::READ))
        flags.push_back("read");
    if (hasProperty(Property::WRITE_WITHOUT_RESPONSE))
        flags.push_back("write-without-response");
    if (hasProperty(Property::WRITE))
        flags.push_back("write");
    if (hasProperty(Property::NOTIFY))
        flags.push_back("notify");
    if (hasProperty(Property::INDICATE))
        flags.push_back("indicate");
    if (hasProperty(Property::SIGNED_WRITE))
        flags.push_back("authenticated-signed-writes");
    if (hasProperty(Property::EXTENDED_PROPERTIES))
        flags.push_back("extended-properties");

    std::string result;
    for (size_t i = 0; i < flags.size(); ++i) {
        if (i > 0) result += ",";
        result += flags[i];
    }
    return result;
}

bool GattCharacteristic::setValue(const std::vector<uint8_t>& newValue) {
    if (value != newValue) {
        value = newValue;
        onValueChanged(value);
        return true;
    }
    return false;
}

bool GattCharacteristic::addDescriptor(std::shared_ptr<GattDescriptor> descriptor) {
    if (!descriptor) {
        Logger::error("Attempted to add null descriptor");
        return false;
    }

    // 이미 존재하는 UUID인지 확인
    for (const auto& existing : descriptors) {
        if (existing->getUUID() == descriptor->getUUID()) {
            Logger::error("Descriptor with UUID " + descriptor->getUUID().toString() + " already exists");
            return false;
        }
    }

    descriptors.push_back(descriptor);
    Logger::debug("Added descriptor: " + descriptor->getUUID().toString());
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

void GattCharacteristic::startNotify() {
    if (!hasProperty(Property::NOTIFY) && !hasProperty(Property::INDICATE)) {
        Logger::error("Characteristic does not support notifications or indications");
        return;
    }

    if (!notifying) {
        notifying = true;
        onNotifyingChanged(true);
    }
}

void GattCharacteristic::stopNotify() {
    if (notifying) {
        notifying = false;
        onNotifyingChanged(false);
    }
}

void GattCharacteristic::onCCCDChanged(bool notificationEnabled, bool indicationEnabled) {
    if (notificationEnabled || indicationEnabled) {
        startNotify();
    } else {
        stopNotify();
    }
}

std::shared_ptr<GattDescriptor> GattCharacteristic::createCCCD() {
    auto cccd = std::make_shared<GattDescriptor>(
        GattDescriptor::Type::CLIENT_CHAR_CONFIG,
        objectPath + "/desc0"
    );
    cccd->setCCCDCallback([this](bool notify, bool indicate) {
        onCCCDChanged(notify, indicate);
    });
    return cccd;
}

// D-Bus 메서드 핸들러 구현
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

    if (!characteristic->hasProperty(Property::READ)) {
        g_dbus_method_invocation_return_error_literal(invocation,
                                                     G_IO_ERROR,
                                                     G_IO_ERROR_NOT_SUPPORTED,
                                                     "Read not permitted");
        return;
    }

    const auto& value = characteristic->getValue();
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

    if (!characteristic->hasProperty(Property::WRITE) &&
        !characteristic->hasProperty(Property::WRITE_WITHOUT_RESPONSE)) {
        g_dbus_method_invocation_return_error_literal(invocation,
                                                     G_IO_ERROR,
                                                     G_IO_ERROR_NOT_SUPPORTED,
                                                     "Write not permitted");
        return;
    }

    GVariant* valueVariant;
    GVariant* optionsVariant;
    g_variant_get(parameters, "(@ay@a{sv})", &valueVariant, &optionsVariant);

    gsize n_elements;
    const void* data = g_variant_get_fixed_array(valueVariant, &n_elements, sizeof(guchar));
    
    std::vector<uint8_t> newValue(static_cast<const uint8_t*>(data),
                                 static_cast<const uint8_t*>(data) + n_elements);

    if (characteristic->setValue(newValue)) {
        g_dbus_method_invocation_return_value(invocation, nullptr);
    } else {
        g_dbus_method_invocation_return_error_literal(invocation,
                                                     G_IO_ERROR,
                                                     G_IO_ERROR_FAILED,
                                                     "Failed to set value");
    }
    
    g_variant_unref(valueVariant);
    g_variant_unref(optionsVariant);
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

    characteristic->startNotify();
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

    characteristic->stopNotify();
    g_dbus_method_invocation_return_value(invocation, nullptr);
}

void GattCharacteristic::onValueChanged(const std::vector<uint8_t>& newValue) {
    // 기본 구현은 비어있음 - 하위 클래스에서 필요에 따라 구현
}

void GattCharacteristic::onNotifyingChanged(bool isNotifying) {
    // 기본 구현은 비어있음 - 하위 클래스에서 필요에 따라 구현
}

void GattCharacteristic::addDBusProperty(const char* name,
    const char* type,
    bool readable,
    bool writable,
    std::function<GVariant*(void*)> getter,
    std::function<void(GVariant*, void*)> setter) {
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

    // DBusInterface::addProperty를 직접 호출
    DBusInterface::addProperty(name, type, readable, writable, getterPtr, setterPtr);
}

} // namespace ggk