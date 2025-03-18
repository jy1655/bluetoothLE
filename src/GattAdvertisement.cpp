#include "GattAdvertisement.h"
#include "Logger.h"
#include "Utils.h"

namespace ggk {

GattAdvertisement::GattAdvertisement(
    DBusConnection& connection,
    const DBusObjectPath& path,
    AdvertisementType type)
    : DBusObject(connection, path),
      type(type),
      appearance(0),
      duration(0),
      includeTxPower(false),
      registered(false) {
}

GattAdvertisement::GattAdvertisement(const DBusObjectPath& path)
    : DBusObject(DBusName::getInstance().getConnection(), path),
      type(AdvertisementType::PERIPHERAL),
      appearance(0),
      duration(0),
      includeTxPower(false),
      registered(false) {
}

void GattAdvertisement::addServiceUUID(const GattUuid& uuid) {
    if (!uuid.toString().empty()) {
        for (const auto& existingUuid : serviceUUIDs) {
            if (existingUuid.toString() == uuid.toString()) {
                return; // UUID가 이미 존재함
            }
        }
        serviceUUIDs.push_back(uuid);
        Logger::debug("Added service UUID to advertisement: " + uuid.toString());
    }
}

void GattAdvertisement::addServiceUUIDs(const std::vector<GattUuid>& uuids) {
    for (const auto& uuid : uuids) {
        addServiceUUID(uuid);
    }
}

void GattAdvertisement::setManufacturerData(uint16_t manufacturerId, const std::vector<uint8_t>& data) {
    manufacturerData[manufacturerId] = data;
    Logger::debug(SSTR << "Set manufacturer data for ID 0x" << Utils::hex(manufacturerId) << " with " << data.size() << " bytes");
}

void GattAdvertisement::setServiceData(const GattUuid& serviceUuid, const std::vector<uint8_t>& data) {
    if (!serviceUuid.toString().empty()) {
        serviceData[serviceUuid] = data;
        Logger::debug("Set service data for UUID: " + serviceUuid.toString());
    }
}

void GattAdvertisement::setLocalName(const std::string& name) {
    localName = name;
    Logger::debug("Set local name: " + name);
}

void GattAdvertisement::setAppearance(uint16_t value) {
    appearance = value;
    Logger::debug(SSTR << "Set appearance: 0x" << Utils::hex(value));
}

void GattAdvertisement::setDuration(uint16_t value) {
    duration = value;
    Logger::debug(SSTR << "Set advertisement duration: " << value << " seconds");
}

void GattAdvertisement::setIncludeTxPower(bool include) {
    includeTxPower = include;
    Logger::debug(SSTR << "Set include TX power: " << (include ? "true" : "false"));
}

bool GattAdvertisement::setupDBusInterfaces() {
    // 속성 정의
    std::vector<DBusProperty> properties = {
        {
            "Type",
            "s",
            true,
            false,
            false,
            [this]() { return getTypeProperty(); },
            nullptr
        },
        {
            "ServiceUUIDs",
            "as",
            true,
            false,
            false,
            [this]() { return getServiceUUIDsProperty(); },
            nullptr
        },
        {
            "ManufacturerData",
            "a{qv}",
            true,
            false,
            false,
            [this]() { return getManufacturerDataProperty(); },
            nullptr
        },
        {
            "ServiceData",
            "a{sv}",
            true,
            false,
            false,
            [this]() { return getServiceDataProperty(); },
            nullptr
        },
        {
            "IncludeTxPower",
            "b",
            true,
            false,
            false,
            [this]() { return getIncludeTxPowerProperty(); },
            nullptr
        }
    };
    
    // LocalName이 설정되어 있는 경우에만 속성 추가
    if (!localName.empty()) {
        properties.push_back({
            "LocalName",
            "s",
            true,
            false,
            false,
            [this]() { return getLocalNameProperty(); },
            nullptr
        });
    }
    
    // Appearance가 설정되어 있는 경우에만 속성 추가
    if (appearance != 0) {
        properties.push_back({
            "Appearance",
            "q",
            true,
            false,
            false,
            [this]() { return getAppearanceProperty(); },
            nullptr
        });
    }
    
    // Duration이 설정되어 있는 경우에만 속성 추가
    if (duration != 0) {
        properties.push_back({
            "Duration",
            "q",
            true,
            false,
            false,
            [this]() { return getDurationProperty(); },
            nullptr
        });
    }

    // LE Advertisement 인터페이스 추가
    if (!addInterface(BlueZConstants::LE_ADVERTISEMENT_INTERFACE, properties)) {
        Logger::error("Failed to add LEAdvertisement interface");
        return false;
    }
    
    // Release 메서드 추가
    if (!addMethod(BlueZConstants::LE_ADVERTISEMENT_INTERFACE, "Release", 
                  [this](const DBusMethodCall& call) { handleRelease(call); })) {
        Logger::error("Failed to add Release method");
        return false;
    }
    
    // 객체 등록
    if (!registerObject()) {
        Logger::error("Failed to register advertisement object");
        return false;
    }
    
    Logger::info("Registered LE Advertisement at: " + getPath().toString());
    return true;
}

void GattAdvertisement::handleRelease(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in Release");
        return;
    }
    
    Logger::info("BlueZ called Release on advertisement: " + getPath().toString());
    
    // Release는 반환값이 없는 메서드
    g_dbus_method_invocation_return_value(call.invocation.get(), nullptr);
    
    // 실제로 해제 작업 수행
    registered = false;
}

bool GattAdvertisement::registerWithBlueZ() {
    try {
        if (registered) {
            Logger::info("Advertisement already registered with BlueZ");
            return true;
        }
        
        // isRegistered() 검사 제거 - setupDBusInterfaces()가 이미 성공했으므로
        // D-Bus 등록은 이미 되어 있다고 가정
        
        // BlueZ에 등록 요청 전송
        Logger::info("Sending RegisterAdvertisement request to BlueZ");
        
        // 옵션 딕셔너리 생성
        GVariantBuilder options_builder;
        g_variant_builder_init(&options_builder, G_VARIANT_TYPE("a{sv}"));
        
        // 파라미터 생성
        GVariant* params = g_variant_new("(o@a{sv})", 
                                        getPath().c_str(),
                                        g_variant_builder_end(&options_builder));
        
        // 참조 카운트 관리
        GVariantPtr parameters(g_variant_ref_sink(params), &g_variant_unref);
        
        // BlueZ에 등록 요청 전송
        GVariantPtr result = getConnection().callMethod(
            BlueZConstants::BLUEZ_SERVICE,
            DBusObjectPath(BlueZConstants::ADAPTER_PATH),
            BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
            BlueZConstants::REGISTER_ADVERTISEMENT,
            std::move(parameters),
            "",
            10000
        );
        
        registered = true;
        Logger::info("Successfully registered advertisement with BlueZ");
        return true;
    } catch (const std::exception& e) {
        // 타임아웃 예외 처리 등 기존 코드 유지
        std::string error = e.what();
        if (error.find("Timeout") != std::string::npos) {
            Logger::warn("BlueZ advertisement registration timed out but continuing operation");
            registered = true;
            return true;
        }
        
        Logger::error("Exception in registerWithBlueZ: " + error);
        return false;
    }
}

bool GattAdvertisement::unregisterFromBlueZ() {
    try {
        // 등록되어 있지 않으면 성공으로 간주
        if (!registered) {
            return true;
        }
        
        // 더 간단한 방식으로 매개변수 생성
        GVariant* params = g_variant_new("(o)", getPath().c_str());
        GVariantPtr parameters(g_variant_ref_sink(params), &g_variant_unref);
        
        GVariantPtr result = getConnection().callMethod(
            BlueZConstants::BLUEZ_SERVICE,
            DBusObjectPath(BlueZConstants::ADAPTER_PATH),
            BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
            BlueZConstants::UNREGISTER_ADVERTISEMENT,
            std::move(parameters)
        );
        
        // 결과가 없어도 오류로 간주하지 않음 
        registered = false;
        Logger::info("Successfully unregistered advertisement from BlueZ");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Exception in unregisterFromBlueZ: " + std::string(e.what()));
        return false;
    }
}

GVariant* GattAdvertisement::getTypeProperty() {
    try {
        const char* typeStr;
        
        switch (type) {
            case AdvertisementType::BROADCAST:
                typeStr = "broadcast";
                break;
            case AdvertisementType::PERIPHERAL:
            default:
                typeStr = "peripheral";
                break;
        }
        
        return Utils::gvariantFromString(typeStr);
    } catch (const std::exception& e) {
        Logger::error("Exception in getTypeProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getServiceUUIDsProperty() {
    try {
        std::vector<std::string> uuidStrings;
        
        for (const auto& uuid : serviceUUIDs) {
            uuidStrings.push_back(uuid.toBlueZFormat());
        }
        
        return Utils::gvariantFromStringArray(uuidStrings);
    } catch (const std::exception& e) {
        Logger::error("Exception in getServiceUUIDsProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getManufacturerDataProperty() {
    try {
        // 제조사 데이터가 없는 경우 빈 맵 반환
        if (manufacturerData.empty()) {
            Logger::debug("No manufacturer data, returning empty map");
            return g_variant_new("a{qv}", NULL);
        }
        
        // 빌더 초기화
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{qv}"));
        
        for (const auto& pair : manufacturerData) {
            uint16_t id = pair.first;
            const std::vector<uint8_t>& data = pair.second;
            
            Logger::debug("Adding manufacturer data for ID: " + Utils::hex(id) + 
                         " with " + std::to_string(data.size()) + " bytes");
            
            // 바이트 배열 생성 (여기서는 스마트 포인터 사용하지 않음)
            // Utils::gvariantFromByteArray는 이미 floating reference 반환
            GVariant* dataVariant = Utils::gvariantFromByteArray(data.data(), data.size());
            
            // 빌더에 추가 (소유권이 빌더로 이전됨)
            g_variant_builder_add(&builder, "{qv}", id, dataVariant);
            
            // dataVariant는 여기서 추가 해제하지 않음 - 빌더가 소유
        }
        
        // 빌더에서 GVariant 생성하고 즉시 반환
        // g_variant_builder_end()는 빌더의 내용을 소비하고 builder를 재설정함
        GVariant* result = g_variant_builder_end(&builder);
        
        // 타입 확인용 디버그 로그
        const GVariantType* type = g_variant_get_type(result);
        Logger::debug("ManufacturerData property type: " + 
                     std::string(g_variant_type_peek_string(type)));
        
        return result;
    } catch (const std::exception& e) {
        Logger::error("Exception in getManufacturerDataProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getServiceDataProperty() {
    try {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
        
        for (const auto& pair : serviceData) {
            const GattUuid& uuid = pair.first;
            const std::vector<uint8_t>& data = pair.second;
            
            // GVariant를 먼저 생성
            GVariant* dataVariant = Utils::gvariantFromByteArray(data.data(), data.size());
            
            // 그 다음 builder에 추가
            g_variant_builder_add(&builder, "{sv}", 
                                 uuid.toBlueZFormat().c_str(), 
                                 dataVariant);
            
            // dataVariant는 builder가 소유권을 가짐
        }
        
        return g_variant_builder_end(&builder);
    } catch (const std::exception& e) {
        Logger::error("Exception in getServiceDataProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getLocalNameProperty() {
    try {
        return Utils::gvariantFromString(localName);
    } catch (const std::exception& e) {
        Logger::error("Exception in getLocalNameProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getAppearanceProperty() {
    try {
        return g_variant_new_uint16(appearance);
    } catch (const std::exception& e) {
        Logger::error("Exception in getAppearanceProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getDurationProperty() {
    try {
        return g_variant_new_uint16(duration);
    } catch (const std::exception& e) {
        Logger::error("Exception in getDurationProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattAdvertisement::getIncludeTxPowerProperty() {
    try {
        return Utils::gvariantFromBoolean(includeTxPower);
    } catch (const std::exception& e) {
        Logger::error("Exception in getIncludeTxPowerProperty: " + std::string(e.what()));
        return nullptr;
    }
}

} // namespace ggk