// src/GattAdvertisement.cpp
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
      discoverable(true), 
      registered(false) {
}

GattAdvertisement::GattAdvertisement(const DBusObjectPath& path)
    : DBusObject(DBusName::getInstance().getConnection(), path),
      type(AdvertisementType::PERIPHERAL),
      appearance(0),
      duration(0),
      includeTxPower(false),
      discoverable(true),
      registered(false) {
}

void GattAdvertisement::addServiceUUID(const GattUuid& uuid) {
    if (!uuid.toString().empty()) {
        for (const auto& existingUuid : serviceUUIDs) {
            if (existingUuid.toString() == uuid.toString()) {
                return; // UUID already exists
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
    Logger::debug("Set manufacturer data for ID 0x" + Utils::hex(manufacturerId));
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

void GattAdvertisement::setDiscoverable(bool value) {
    discoverable = value;
    Logger::debug("Set discoverable: " + std::string(value ? "true" : "false"));
}

void GattAdvertisement::setAppearance(uint16_t value) {
    appearance = value;
    Logger::debug("Set appearance: 0x" + Utils::hex(value));
}

void GattAdvertisement::setDuration(uint16_t value) {
    duration = value;
    Logger::debug("Set advertisement duration: " + std::to_string(value) + " seconds");
}

void GattAdvertisement::setIncludeTxPower(bool include) {
    includeTxPower = include;
    Logger::debug("Set include TX power: " + std::string(include ? "true" : "false"));
}

void GattAdvertisement::addInclude(const std::string& item) {
    // 이미 포함되어 있는지 확인
    for (const auto& existing : includes) {
        if (existing == item) {
            return; // 중복 방지
        }
    }
    
    includes.push_back(item);
    Logger::debug("Added include item: " + item);
    
    // 이전 방식과의 호환성을 위한 특수 처리
    if (item == "tx-power") {
        includeTxPower = true;
    }
}

void GattAdvertisement::setIncludes(const std::vector<std::string>& items) {
    includes.clear();
    for (const auto& item : items) {
        addInclude(item);
    }
    Logger::debug("Set includes array with " + std::to_string(includes.size()) + " items");
}

bool GattAdvertisement::setupDBusInterfaces() {
    // 이미 등록된 경우 성공 반환
    if (isRegistered()) {
        Logger::debug("Advertisement already registered with D-Bus");
        return true;
    }
    
    Logger::info("Setting up D-Bus interfaces for advertisement: " + getPath().toString());
    
    // 모든 속성 한 번에 정의
    std::vector<DBusProperty> properties;
    
    // Type 속성 (필수)
    properties.push_back({
        "Type",
        "s",
        true,
        false,
        false,
        [this]() { return getTypeProperty(); },
        nullptr
    });
    
    // ServiceUUIDs 속성 (필수)
    properties.push_back({
        "ServiceUUIDs",
        "as",
        true,
        false,
        false,
        [this]() { return getServiceUUIDsProperty(); },
        nullptr
    });
    
    // ManufacturerData 속성
    properties.push_back({
        "ManufacturerData",
        "a{qv}",
        true,
        false,
        false,
        [this]() { return getManufacturerDataProperty(); },
        nullptr
    });
    
    // ServiceData 속성
    properties.push_back({
        "ServiceData",
        "a{sv}",
        true,
        false,
        false,
        [this]() { return getServiceDataProperty(); },
        nullptr
    });
    
    // Discoverable 속성 (BlueZ 5.82+)
    properties.push_back({
        "Discoverable",
        "b",
        true,
        false,
        false,
        [this]() { return getDiscoverableProperty(); },
        nullptr
    });
    
    // Includes 속성 (BlueZ 5.82+)
    properties.push_back({
        "Includes",
        "as",
        true,
        false,
        false,
        [this]() { return getIncludesProperty(); },
        nullptr
    });
    
    // TX Power 속성 (이전 버전 호환용)
    properties.push_back({
        "IncludeTxPower",
        "b",
        true,
        false,
        false,
        [this]() { return getIncludeTxPowerProperty(); },
        nullptr
    });
    
    // 조건부 속성 추가
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
    
    // LEAdvertisement 인터페이스 추가
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
    
    // D-Bus에 객체 등록
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
    
    // 응답 반환 (응답 값 없음)
    g_dbus_method_invocation_return_value(call.invocation.get(), nullptr);
    
    // 등록 상태 업데이트
    registered = false;
}

bool GattAdvertisement::registerWithBlueZ() {
    try {
        Logger::info("Registering GATT advertisement with BlueZ");
        
        // 이미 등록된 경우 성공 반환
        if (registered) {
            Logger::info("Advertisement already registered with BlueZ");
            return true;
        }
        
        // D-Bus 인터페이스 설정
        if (!isRegistered()) {
            if (!setupDBusInterfaces()) {
                Logger::error("Failed to setup advertisement D-Bus interfaces");
                return false;
            }
        }
        
        // BlueZ 5.82 호환성 확보
        ensureBlueZ582Compatibility();
        
        // 옵션 생성 - 수정된 부분
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
        
        // 빌더 종료하여 GVariant 먼저 생성
        GVariant* options = g_variant_builder_end(&builder);
        
        // 매개변수 생성 - 안전한 방식으로 수정
        GVariant* paramsRaw = g_variant_new("(o@a{sv})", 
                                getPath().c_str(), 
                                options);
        
        GVariantPtr params = makeGVariantPtr(paramsRaw, true);
        
        try {
            // BlueZ 호출
            getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
                BlueZConstants::REGISTER_ADVERTISEMENT,
                std::move(params),
                "",
                5000  // 5초 타임아웃
            );
            
            registered = true;
            Logger::info("Successfully registered advertisement with BlueZ");
            return true;
        }
        catch (const std::exception& e) {
            std::string errorMsg = e.what();
            
            // AlreadyExists 오류 처리
            if (errorMsg.find("AlreadyExists") != std::string::npos) {
                Logger::info("Advertisement already registered with BlueZ");
                registered = true;
                return true;
            }
            
            Logger::error("Failed to register advertisement with BlueZ: " + errorMsg);
            return false;
        }
    }
    catch (const std::exception& e) {
        Logger::error("Exception in registerWithBlueZ: " + std::string(e.what()));
        return false;
    }
}

bool GattAdvertisement::unregisterFromBlueZ() {
    // 등록되지 않은 경우 성공 반환
    if (!registered) {
        Logger::debug("Advertisement not registered with BlueZ, nothing to unregister");
        return true;
    }
    
    try {
        Logger::info("Unregistering advertisement from BlueZ...");
        
        // 매개변수 생성
        GVariant* paramsRaw = g_variant_new("(o)", getPath().c_str());
        GVariantPtr params = makeGVariantPtr(paramsRaw, true);
        
        try {
            // BlueZ 호출
            getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
                BlueZConstants::UNREGISTER_ADVERTISEMENT,
                std::move(params),
                "",
                5000  // 5초 타임아웃
            );
            
            Logger::info("Successfully unregistered advertisement via BlueZ DBus API");
        }
        catch (const std::exception& e) {
            // 특정 오류는 무시
            std::string error = e.what();
            if (error.find("DoesNotExist") != std::string::npos || 
                error.find("Does Not Exist") != std::string::npos ||
                error.find("No such") != std::string::npos) {
                Logger::info("Advertisement already unregistered or not found in BlueZ");
            } else {
                Logger::warn("Failed to unregister via BlueZ API: " + error);
            }
        }
        
        // 백업 방법: bluetoothctl 사용
        system("echo -e 'menu advertise\\noff\\nexit\\n' | bluetoothctl > /dev/null 2>&1");
        
        // 추가 백업: hciconfig 사용
        system("sudo hciconfig hci0 noleadv > /dev/null 2>&1");
        
        // 등록 상태 업데이트
        registered = false;
        return true;
    }
    catch (const std::exception& e) {
        Logger::error("Exception in unregisterFromBlueZ: " + std::string(e.what()));
        registered = false;  // 안전을 위해 상태 업데이트
        return false;
    }
}

void GattAdvertisement::ensureBlueZ582Compatibility() {
    // 필수 Includes 항목 확인
    std::vector<std::string> requiredIncludes;
    
    // TX Power 확인
    if (includeTxPower && std::find(includes.begin(), includes.end(), "tx-power") == includes.end()) {
        requiredIncludes.push_back("tx-power");
    }
    
    // 로컬 이름 확인
    if (!localName.empty() && std::find(includes.begin(), includes.end(), "local-name") == includes.end()) {
        requiredIncludes.push_back("local-name");
    }
    
    // 외관 확인
    if (appearance != 0 && std::find(includes.begin(), includes.end(), "appearance") == includes.end()) {
        requiredIncludes.push_back("appearance");
    }
    
    // 필요한 항목 추가
    for (const auto& item : requiredIncludes) {
        includes.push_back(item);
    }
    
    // Discoverable 속성 확인
    discoverable = true;
    
    // 널 UUID 제거
    serviceUUIDs.erase(
        std::remove_if(serviceUUIDs.begin(), serviceUUIDs.end(), 
                      [](const GattUuid& uuid) { return uuid.toString().empty(); }),
        serviceUUIDs.end()
    );
}

// 속성 취득 함수들
GVariant* GattAdvertisement::getTypeProperty() {
    const char* typeStr = (type == AdvertisementType::BROADCAST) ? "broadcast" : "peripheral";
    GVariantPtr variantPtr = Utils::gvariantPtrFromString(typeStr);
    if (!variantPtr) return nullptr;
    return g_variant_ref(variantPtr.get());
}

GVariant* GattAdvertisement::getServiceUUIDsProperty() {
    std::vector<std::string> uuidStrings;
    for (const auto& uuid : serviceUUIDs) {
        uuidStrings.push_back(uuid.toBlueZFormat());
    }
    
    GVariantPtr variantPtr = Utils::gvariantPtrFromStringArray(uuidStrings);
    if (!variantPtr) return nullptr;
    return g_variant_ref(variantPtr.get());
}

GVariant* GattAdvertisement::getManufacturerDataProperty() {
    if (manufacturerData.empty()) {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{qv}"));
        return g_variant_builder_end(&builder);
    }
    
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{qv}"));
    
    for (const auto& pair : manufacturerData) {
        uint16_t id = pair.first;
        const std::vector<uint8_t>& data = pair.second;
        
        GVariant* bytes = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, data.data(), data.size(), sizeof(uint8_t));
        GVariant* dataVariant = g_variant_new_variant(bytes);
        
        g_variant_builder_add(&builder, "{qv}", id, dataVariant);
    }
    
    return g_variant_builder_end(&builder);
}

GVariant* GattAdvertisement::getServiceDataProperty() {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    
    for (const auto& pair : serviceData) {
        const GattUuid& uuid = pair.first;
        const std::vector<uint8_t>& data = pair.second;
        
        GVariantPtr dataVariant = Utils::gvariantPtrFromByteArray(data.data(), data.size());
        g_variant_builder_add(&builder, "{sv}", uuid.toBlueZFormat().c_str(), dataVariant.get());
    }
    
    return g_variant_builder_end(&builder);
}

GVariant* GattAdvertisement::getLocalNameProperty() {
    GVariantPtr variantPtr = Utils::gvariantPtrFromString(localName);
    if (!variantPtr) return nullptr;
    return g_variant_ref(variantPtr.get());
}

GVariant* GattAdvertisement::getAppearanceProperty() {
    return g_variant_new_uint16(appearance);
}

GVariant* GattAdvertisement::getDurationProperty() {
    return g_variant_new_uint16(duration);
}

GVariant* GattAdvertisement::getIncludeTxPowerProperty() {
    GVariantPtr variantPtr = Utils::gvariantPtrFromBoolean(includeTxPower);
    if (!variantPtr) return nullptr;
    return g_variant_ref(variantPtr.get());
}

GVariant* GattAdvertisement::getDiscoverableProperty() {
    GVariantPtr variantPtr = Utils::gvariantPtrFromBoolean(discoverable);
    if (!variantPtr) return nullptr;
    return g_variant_ref(variantPtr.get());
}

GVariant* GattAdvertisement::getIncludesProperty() {
    std::vector<std::string> allIncludes = includes;
    
    // TX Power이 켜져있지만 includes에 없으면 추가
    if (includeTxPower && std::find(allIncludes.begin(), allIncludes.end(), "tx-power") == allIncludes.end()) {
        allIncludes.push_back("tx-power");
    }
    
    // Appearance가 설정되어 있지만 includes에 없으면 추가
    if (appearance != 0 && std::find(allIncludes.begin(), allIncludes.end(), "appearance") == allIncludes.end()) {
        allIncludes.push_back("appearance");
    }
    
    // LocalName이 설정되어 있지만 includes에 없으면 추가
    if (!localName.empty() && std::find(allIncludes.begin(), allIncludes.end(), "local-name") == allIncludes.end()) {
        allIncludes.push_back("local-name");
    }
    
    GVariantPtr variantPtr = Utils::gvariantPtrFromStringArray(allIncludes);
    if (!variantPtr) return nullptr;
    return g_variant_ref(variantPtr.get());
}

} // namespace ggk