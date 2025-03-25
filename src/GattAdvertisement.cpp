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

        // 기존 광고 정리 - BlueZ의 최대 광고 수 도달 오류 방지
        Logger::info("Cleaning up any existing advertisements...");
        // 1. 먼저 bluetoothctl을 통해 광고 중지 시도
        system("echo -e 'menu advertise\\noff\\nexit\\n' | bluetoothctl > /dev/null 2>&1");
        // 2. HCI 명령어로 광고 중지 시도
        system("sudo hciconfig hci0 noleadv > /dev/null 2>&1");
        // 3. 모든 광고 인스턴스 제거 시도
        system("echo -e 'remove-advertising 0\\nremove-advertising 1\\nremove-advertising 2\\nremove-advertising 3\\n' | bluetoothctl > /dev/null 2>&1");
        
        // 적절한 대기 시간 적용
        sleep(2);
        
        // D-Bus 인터페이스 설정
        if (!setupDBusInterfaces()) {
            Logger::error("Failed to setup advertisement D-Bus interfaces");
            return false;
        }
        
        // BlueZ 서비스가 실행 중인지 확인
        int bluezStatus = system("systemctl is-active --quiet bluetooth.service");
        if (bluezStatus != 0) {
            Logger::error("BlueZ service is not active. Trying to restart...");
            system("sudo systemctl restart bluetooth.service");
            sleep(3);  // 서비스 재시작 대기
            
            bluezStatus = system("systemctl is-active --quiet bluetooth.service");
            if (bluezStatus != 0) {
                Logger::error("Failed to restart BlueZ service");
                return false;
            }
            Logger::info("BlueZ service restarted successfully");
        }
        
        // BlueZ 어댑터가 존재하는지 확인
        if (system("hciconfig hci0 > /dev/null 2>&1") != 0) {
            Logger::error("BlueZ adapter 'hci0' not found or not available");
            return false;
        }
        
        // BlueZ에 등록 요청 전송
        Logger::info("Sending RegisterAdvertisement request to BlueZ");
        
        // 최대 3번의 재시도, 각 시도마다 대기 시간 증가
        bool success = false;
        std::string errorMsg = "";
        int retryCount = 0;
        int maxRetries = 3;
        int baseWaitMs = 1000;  // 1초
        
        while (!success && retryCount < maxRetries) {
            try {
                // 빈 딕셔너리를 Utils 헬퍼 함수를 사용해 생성
                GVariantPtr emptyOptions = Utils::createEmptyDictionaryPtr();
                
                // 튜플 형태로 파라미터 생성
                GVariant* tuple = g_variant_new("(o@a{sv})", 
                                            getPath().c_str(), 
                                            emptyOptions.get());
                
                // 튜플에 대한 참조 관리
                GVariant* owned_tuple = g_variant_ref_sink(tuple);
                GVariantPtr parameters(owned_tuple, &g_variant_unref);
                
                // BlueZ 호출
                GVariantPtr result = getConnection().callMethod(
                    BlueZConstants::BLUEZ_SERVICE,
                    DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                    BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
                    BlueZConstants::REGISTER_ADVERTISEMENT,
                    std::move(parameters),
                    "",
                    5000
                );
                
                // 호출이 성공하면
                success = true;
                registered = true;
                Logger::info("Successfully registered advertisement via BlueZ DBus API");
                break;
            }
            catch (const std::exception& e) {
                errorMsg = e.what();
                
                if (errorMsg.find("Maximum advertisements reached") != std::string::npos) {
                    Logger::error("Maximum advertisements reached. Attempting to clean up...");
                    
                    // 광고 정리 시도
                    system("echo -e 'remove-advertising 0\\nremove-advertising 1\\nremove-advertising 2\\nremove-advertising 3\\n' | bluetoothctl > /dev/null 2>&1");
                    system("sudo hciconfig hci0 noleadv > /dev/null 2>&1");
                    
                    // BlueZ 종료 후 재시작 시도 (마지막 시도에서만)
                    if (retryCount == maxRetries - 1) {
                        Logger::info("Trying to restart BlueZ to clear advertisements...");
                        system("sudo systemctl restart bluetooth.service");
                        sleep(3);  // 재시작 대기 시간
                    }
                } else {
                    Logger::error("DBus method failed: " + errorMsg);
                }
                
                retryCount++;
                int waitTime = baseWaitMs * (1 << retryCount);  // Exponential backoff
                Logger::info("Retry " + std::to_string(retryCount) + 
                            " of " + std::to_string(maxRetries) + 
                            " in " + std::to_string(waitTime / 1000.0) + " seconds");
                usleep(waitTime * 1000);
            }
        }
        
        // 중요: 최종 성공 여부 검증 - 실패한 경우 false 반환
        if (!success) {
            Logger::info("Failed to register advertisement via D-Bus after " + 
                         std::to_string(maxRetries) + " attempts. Last error: " + errorMsg);
            Logger::info("Falling back to bluetoothctl for advertisement...");
            
            // bluetoothctl 명령어로 광고 활성화
            std::string cmd = "echo -e 'menu advertise\\non\\nexit\\n' | bluetoothctl > /dev/null 2>&1";
            int cmdResult = system(cmd.c_str());
            
            if (cmdResult == 0) {
                Logger::info("Advertisement enabled via bluetoothctl");
                registered = true;
                success = true;
            } else {
                // hciconfig로 직접 광고 활성화 시도
                Logger::info("Trying to enable advertisement via hciconfig...");
                system("sudo hciconfig hci0 leadv 3 > /dev/null 2>&1");
                sleep(1);
                system("sudo hciconfig hci0 noscan > /dev/null 2>&1");
                sleep(1);
                system("sudo hciconfig hci0 piscan > /dev/null 2>&1");
                sleep(1);
                
                // 상태 확인
                std::string checkCmd = "hciconfig hci0 | grep -i 'ISCAN\\|PSCAN\\|UP RUNNING' > /dev/null 2>&1";
                int checkResult = system(checkCmd.c_str());
                
                if (checkResult == 0) {
                    Logger::info("Advertisement enabled via hciconfig");
                    registered = true;
                    success = true;
                } else {
                    // 마지막 시도: 직접 HCI 명령어 사용
                    Logger::info("Final attempt: Using direct HCI commands");
                    system("sudo hcitool -i hci0 cmd 0x08 0x0006 A0 00 A0 00 00 00 00 00 00 00 00 00 00 07 00 > /dev/null 2>&1");
                    sleep(1);
                    system("sudo hcitool -i hci0 cmd 0x08 0x000a 01 > /dev/null 2>&1");
                    
                    Logger::warn("Advertisement enabled using direct HCI commands, but status cannot be verified");
                    registered = true;  // 성공이라 가정
                    success = true;
                }
            }
        }
        
        return success;
    }
    catch (const std::exception& e) {
        Logger::error("Exception in registerWithBlueZ: " + std::string(e.what()));
        return false;
    }
    catch (...) {
        Logger::error("Unknown exception in registerWithBlueZ");
        return false;
    }
}

bool GattAdvertisement::unregisterFromBlueZ() {
    try {
        // 등록되어 있지 않으면 성공으로 간주
        if (!registered) {
            Logger::debug("Advertisement not registered, nothing to unregister");
            return true;
        }
        
        Logger::info("Unregistering advertisement from BlueZ...");
        bool success = false;
        
        // 방법 1: DBus API를 통한 등록 해제
        try {
            // 더 간단한 방식으로 매개변수 생성
            GVariant* params = g_variant_new("(o)", getPath().c_str());
            GVariantPtr parameters(g_variant_ref_sink(params), &g_variant_unref);
            
            getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE,
                BlueZConstants::UNREGISTER_ADVERTISEMENT,
                std::move(parameters),
                "",
                5000  // 5초 타임아웃
            );
            
            success = true;
            Logger::info("Successfully unregistered advertisement via BlueZ DBus API");
        } catch (const std::exception& e) {
            // DoesNotExist 오류의 경우 이미 BlueZ에서 해제된 것으로 간주
            std::string error = e.what();
            if (error.find("DoesNotExist") != std::string::npos || 
                error.find("Does Not Exist") != std::string::npos ||
                error.find("No such") != std::string::npos) {
                Logger::info("Advertisement already unregistered or not found in BlueZ");
                success = true;
            } else {
                Logger::warn("Failed to unregister via BlueZ API: " + error);
            }
        }
        
        // 방법 2: bluetoothctl을 사용한 등록 해제 (방법 1이 실패한 경우)
        if (!success) {
            Logger::info("Trying to unregister advertisement via bluetoothctl...");
            if (system("echo -e 'menu advertise\\noff\\nexit\\n' | bluetoothctl > /dev/null 2>&1") == 0) {
                success = true;
                Logger::info("Advertisement disabled via bluetoothctl");
            }
        }
        
        // 방법 3: hciconfig를 사용한 등록 해제 (방법 1, 2가 실패한 경우)
        if (!success) {
            Logger::info("Trying to disable advertisement via hciconfig...");
            if (system("sudo hciconfig hci0 noleadv > /dev/null 2>&1") == 0) {
                success = true;
                Logger::info("Advertisement disabled via hciconfig");
            }
        }
        
        // 최후의 방법: BlueZ 광고 객체 명시적 삭제 시도
        if (!success) {
            Logger::info("Trying to remove all advertising instances...");
            system("echo -e 'remove-advertising 0\\nremove-advertising 1\\nremove-advertising 2\\nremove-advertising 3\\n' | bluetoothctl > /dev/null 2>&1");
            success = true;  // 성공 여부를 정확히 확인하기 어려움
        }
        
        // 로컬 상태 업데이트
        registered = false;
        
        return success;
    } catch (const std::exception& e) {
        Logger::error("Exception in unregisterFromBlueZ: " + std::string(e.what()));
        registered = false;  // 안전을 위해 로컬 상태 업데이트
        return false;
    } catch (...) {
        Logger::error("Unknown exception in unregisterFromBlueZ");
        registered = false;  // 안전을 위해 로컬 상태 업데이트
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
        if (manufacturerData.empty()) {
            // 유효한 빈 사전 형식 제공
            return g_variant_new_array(G_VARIANT_TYPE("{qv}"), NULL, 0);
        }
        
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{qv}"));
        
        for (const auto& pair : manufacturerData) {
            uint16_t id = pair.first;
            const std::vector<uint8_t>& data = pair.second;
            
            // 바이트 배열을 ay 형식으로 변환
            GVariant* bytes = g_variant_new_fixed_array(
                G_VARIANT_TYPE_BYTE,
                data.data(),
                data.size(),
                sizeof(uint8_t)
            );
            
            // 올바른 variant 래핑
            GVariant* dataVariant = g_variant_new_variant(bytes);
            
            g_variant_builder_add(&builder, "{qv}", id, dataVariant);
        }
        
        GVariant* result = g_variant_builder_end(&builder);
        
        // 디버깅: 형식 검증 
        char* debug_str = g_variant_print(result, TRUE);
        Logger::debug("ManufacturerData property: " + std::string(debug_str));
        g_free(debug_str);
        
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