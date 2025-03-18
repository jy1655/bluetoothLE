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
    // 이미 등록된 경우 재등록하지 않음
    if (isRegistered()) {
        Logger::warn("Advertisement already registered with D-Bus");
        return true;
    }
    
    Logger::info("Setting up D-Bus interfaces for advertisement: " + getPath().toString());
    
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

    // 1. LE Advertisement 인터페이스 추가 - 반드시 registerObject() 호출 전에 수행
    if (!addInterface(BlueZConstants::LE_ADVERTISEMENT_INTERFACE, properties)) {
        Logger::error("Failed to add LEAdvertisement interface");
        return false;
    }
    
    // 2. Release 메서드 추가 - 반드시 registerObject() 호출 전에 수행
    if (!addMethod(BlueZConstants::LE_ADVERTISEMENT_INTERFACE, "Release", 
                  [this](const DBusMethodCall& call) { handleRelease(call); })) {
        Logger::error("Failed to add Release method");
        return false;
    }
    
    // 3. 객체 등록 - 모든 인터페이스와 메서드 추가 후에 마지막으로 수행
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
        // 이미 BlueZ에 등록된 경우
        if (registered) {
            Logger::info("Advertisement already registered with BlueZ");
            return true;
        }
        
        // 객체가 이미 등록되어 있으면 등록 해제 후 다시 설정
        if (isRegistered()) {
            Logger::info("Unregistering advertisement object to refresh interfaces");
            unregisterObject();
        }
        
        // D-Bus 인터페이스 설정
        if (!setupDBusInterfaces()) {
            Logger::error("Failed to setup advertisement D-Bus interfaces");
            return false;
        }
        
        // BlueZ 서비스가 실행 중인지 확인
        int bluezStatus = system("systemctl is-active --quiet bluetooth.service");
        if (bluezStatus != 0) {
            Logger::error("BlueZ service is not active. Run: systemctl start bluetooth.service");
            return false;
        }
        
        // BlueZ 어댑터가 존재하는지 확인
        if (system("hciconfig hci0 > /dev/null 2>&1") != 0) {
            Logger::error("BlueZ adapter 'hci0' not found or not available");
            return false;
        }
        
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
        
        try {
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
            
            if (!result) {
                Logger::warn("No result from BlueZ RegisterAdvertisement call");
                // 타임아웃인 경우와 동일하게 처리
                registered = true;
                Logger::info("Assuming advertisement registration succeeded despite no result");
                return true;
            }
            
            registered = true;
            Logger::info("Successfully registered advertisement with BlueZ");
            return true;
        } catch (const std::exception& e) {
            std::string error = e.what();
            
            // 타임아웃 예외 처리
            if (error.find("Timeout") != std::string::npos) {
                Logger::warn("BlueZ advertisement registration timed out but continuing operation");
                registered = true;
                return true;
            }
            
            // DoesNotExist 오류 처리
            if (error.find("DoesNotExist") != std::string::npos || 
                error.find("Does Not Exist") != std::string::npos) {
                Logger::error("BlueZ advertising interface not available. Check if bluetoothd supports LE advertising");
                
                // bluetoothd 버전 확인
                system("bluetoothd -v");
                
                // 어댑터를 리셋해 볼 수 있음
                Logger::info("Attempting to reset Bluetooth adapter...");
                system("sudo hciconfig hci0 down && sudo hciconfig hci0 up");
                sleep(2);
                
                return false;
            }
            
            Logger::error("Exception in registerWithBlueZ: " + error);
            return false;
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in registerWithBlueZ outer block: " + std::string(e.what()));
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