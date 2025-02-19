#include "GattService.h"
#include "GattCharacteristic.h"
#include "Logger.h"

namespace ggk {

const char* GattService::INTERFACE_NAME = "org.bluez.GattService1";

GattService::GattService(const GattUuid& uuid, Type type)
    : DBusInterface(INTERFACE_NAME)
    , uuid(uuid)
    , type(type) {
    
    // 필수 프로퍼티 설정
    addProperty("UUID", "s", true, false, 
                [this](void) -> GVariant* {
                    return g_variant_new_string(uuid.toString128().c_str());  // 128비트 UUID 사용
                },
                nullptr);

    addProperty("Primary", "b", true, false,
                [this](void) -> GVariant* {
                    return g_variant_new_boolean(type == Type::PRIMARY);
                },
                nullptr);

    Logger::debug("Created GATT service: " + uuid.value);
}

bool GattService::addCharacteristic(std::shared_ptr<GattCharacteristic> characteristic) {
    if (!characteristic) {
        Logger::error("Attempted to add null characteristic to service: " + uuid.value);
        return false;
    }

    // UUID 중복 검사
    for (const auto& existing : characteristics) {
        if (existing->getUUID() == characteristic->getUUID()) {
            Logger::error("Characteristic with UUID " + characteristic->getUUID() + 
                         " already exists in service: " + uuid.value);
            return false;
        }
    }

    characteristics.push_back(characteristic);
    Logger::debug("Added characteristic " + characteristic->getUUID() + 
                 " to service: " + uuid.value);
    return true;
}

std::shared_ptr<GattCharacteristic> GattService::getCharacteristic(const std::string& uuid) {
    for (const auto& characteristic : characteristics) {
        if (characteristic->getUUID() == uuid) {
            return characteristic;
        }
    }
    return nullptr;
}

void GattService::addManagedObjectProperties(GVariantBuilder* builder) {
    if (!builder) return;

    // 서비스 인터페이스 프로퍼티 추가
    GVariantBuilder interfaceBuilder;
    g_variant_builder_init(&interfaceBuilder, G_VARIANT_TYPE("a{sa{sv}}"));

    // GattService1 인터페이스
    GVariantBuilder propsBuilder;
    g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));

    // UUID 프로퍼티
    g_variant_builder_add(&propsBuilder, "{sv}", "UUID",
                         g_variant_new_string(uuid.value.c_str()));

    // Primary 프로퍼티
    g_variant_builder_add(&propsBuilder, "{sv}", "Primary",
                         g_variant_new_boolean(type == Type::PRIMARY));

    g_variant_builder_add(&interfaceBuilder, "{sa{sv}}",
                         INTERFACE_NAME,
                         &propsBuilder);

    g_variant_builder_add(builder, "a{sa{sv}}", &interfaceBuilder);
}

} // namespace ggk