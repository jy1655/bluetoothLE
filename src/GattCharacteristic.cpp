#include "GattCharacteristic.h"
#include "GattService.h" // 여기서만 포함 (헤더에서는 전방 선언만)
#include "Logger.h"

namespace ggk {

GattCharacteristic::GattCharacteristic(
    SDBusConnection& connection,
    const std::string& path,
    const GattUuid& uuid,
    GattService* service,
    uint8_t properties,
    uint8_t permissions)
    : connection(connection),
      object(connection, path),
      uuid(uuid),
      parentService(service),
      properties(properties),
      permissions(permissions),
      notifying(false) {
}

void GattCharacteristic::setValue(const std::vector<uint8_t>& newValue) {
    try {
        // 값 설정
        {
            std::lock_guard<std::mutex> lock(valueMutex);
            value = newValue;
        }
        
        // 등록된 경우 PropertyChanged 시그널 발생
        if (object.isRegistered()) {
            // Value 속성 변경 알림
            object.emitPropertyChanged(
                sdbus::InterfaceName{BlueZConstants::GATT_CHARACTERISTIC_INTERFACE}, 
                sdbus::PropertyName{BlueZConstants::PROPERTY_VALUE}
            );
            
            // 알림 중이면 알림 처리
            bool isNotifying = false;
            {
                std::lock_guard<std::mutex> notifyLock(notifyMutex);
                isNotifying = notifying;
            }
            
            if (isNotifying) {
                // 콜백이 등록된 경우 실행
                std::lock_guard<std::mutex> callbackLock(callbackMutex);
                if (notifyCallback) {
                    try {
                        notifyCallback();
                    } catch (const std::exception& e) {
                        Logger::error("알림 콜백에서 예외: " + std::string(e.what()));
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        Logger::error("setValue에서 예외: " + std::string(e.what()));
    }
}

void GattCharacteristic::setValue(std::vector<uint8_t>&& newValue) {
    try {
        // 이동 의미론을 사용하여 값 설정
        {
            std::lock_guard<std::mutex> lock(valueMutex);
            value = std::move(newValue);
        }
        
        // 등록된 경우 PropertyChanged 시그널 발생
        if (object.isRegistered()) {
            // Value 속성 변경 알림
            object.emitPropertyChanged(
                sdbus::InterfaceName{BlueZConstants::GATT_CHARACTERISTIC_INTERFACE}, 
                sdbus::PropertyName{BlueZConstants::PROPERTY_VALUE}
            );
            
            // 알림 상태 확인
            bool isNotifying = false;
            {
                std::lock_guard<std::mutex> notifyLock(notifyMutex);
                isNotifying = notifying;
            }
            
            // 알림 중이면 알림 처리
            if (isNotifying) {
                std::lock_guard<std::mutex> callbackLock(callbackMutex);
                if (notifyCallback) {
                    try {
                        notifyCallback();
                    } catch (const std::exception& e) {
                        Logger::error("알림 콜백에서 예외: " + std::string(e.what()));
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        Logger::error("setValue(이동)에서 예외: " + std::string(e.what()));
    }
}

GattDescriptorPtr GattCharacteristic::createDescriptor(
    const GattUuid& uuid,
    uint8_t permissions)
{
    if (uuid.toString().empty()) {
        Logger::error("빈 UUID로 설명자를 생성할 수 없음");
        return nullptr;
    }
    
    // CCCD UUID (0x2902)
    const std::string CCCD_UUID = "00002902-0000-1000-8000-00805f9b34fb";
    
    // 특성에 알림/표시가 있는 경우 CCCD 설명자 수동 생성 시도 확인
    // BlueZ 5.82에서는 StartNotify/StopNotify 호출 시 CCCD를 자동으로 처리함
    if (uuid.toString() == CCCD_UUID && 
        (properties & GattProperty::PROP_NOTIFY || properties & GattProperty::PROP_INDICATE)) {
        Logger::warn("알림/표시가 있는 특성에 CCCD 설명자를 수동으로 생성하려 시도함. " 
                    "이는 BlueZ 5.82+에서 자동으로 처리됨. 요청 무시.");
        return nullptr;
    }
    
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(descriptorsMutex);
    
    // 이미 존재하는지 확인
    auto it = descriptors.find(uuidStr);
    if (it != descriptors.end()) {
        if (!it->second) {
            Logger::error("UUID에 대한 null 설명자 항목 발견: " + uuidStr);
            descriptors.erase(it);
        } else {
            return it->second;
        }
    }
    
    try {
        // 일관된 객체 경로 생성
        std::string descNum = "desc" + std::to_string(descriptors.size() + 1);
        std::string descriptorPath = getPath() + "/" + descNum;
        
        // 설명자 생성 - this로 자신에 대한 포인터 전달 (약한 참조)
        auto descriptor = std::make_shared<GattDescriptor>(
            connection,
            descriptorPath,
            uuid,
            this,  // 부모 특성 포인터
            permissions
        );
        
        if (!descriptor) {
            Logger::error("UUID에 대한 설명자 생성 실패: " + uuidStr);
            return nullptr;
        }
        
        // 맵에 추가
        descriptors[uuidStr] = descriptor;
        
        Logger::info("설명자 생성됨: " + uuidStr + ", 경로: " + descriptorPath);
        return descriptor;
    } catch (const std::exception& e) {
        Logger::error("설명자 생성 중 예외: " + std::string(e.what()));
        return nullptr;
    }
}

GattDescriptorPtr GattCharacteristic::getDescriptor(const GattUuid& uuid) const {
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(descriptorsMutex);
    
    auto it = descriptors.find(uuidStr);
    if (it != descriptors.end() && it->second) {
        return it->second;
    }
    
    return nullptr;
}

bool GattCharacteristic::startNotify() {
    std::lock_guard<std::mutex> lock(notifyMutex);
    
    if (notifying) {
        return true;  // 이미 알림 중
    }
    
    // 특성이 알림을 지원하는지 확인
    if (!(properties & GattProperty::PROP_NOTIFY) && 
        !(properties & GattProperty::PROP_INDICATE)) {
        Logger::error("특성이 알림을 지원하지 않음: " + uuid.toString());
        return false;
    }
    
    notifying = true;
    
    // Notifying 속성에 대한 PropertyChanged 시그널 발생
    if (object.isRegistered()) {
        object.emitPropertyChanged(
            sdbus::InterfaceName{BlueZConstants::GATT_CHARACTERISTIC_INTERFACE}, 
            sdbus::PropertyName{BlueZConstants::PROPERTY_NOTIFYING}
        );
    }
    
    // 알림 콜백 호출
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex);
        if (notifyCallback) {
            try {
                notifyCallback();
            } catch (const std::exception& e) {
                Logger::error("알림 콜백에서 예외: " + std::string(e.what()));
                // 콜백이 실패해도 알림 상태 유지
            }
        }
    }
    
    Logger::info("특성에 대한 알림 시작됨: " + uuid.toString());
    return true;
}

bool GattCharacteristic::stopNotify() {
    std::lock_guard<std::mutex> lock(notifyMutex);
    
    if (!notifying) {
        return true;  // 이미 중지됨
    }
    
    notifying = false;
    
    // Notifying 속성에 대한 PropertyChanged 시그널 발생
    if (object.isRegistered()) {
        object.emitPropertyChanged(
            sdbus::InterfaceName{BlueZConstants::GATT_CHARACTERISTIC_INTERFACE}, 
            sdbus::PropertyName{BlueZConstants::PROPERTY_NOTIFYING}
        );
    }
    
    Logger::info("특성에 대한 알림 중지됨: " + uuid.toString());
    return true;
}

bool GattCharacteristic::setupInterfaces() {
    // 이미 설정되어 있으면 중복 설정 방지
    if (interfaceSetup) {
        return true;
    }
    
    try {
        Logger::info("특성 인터페이스 설정 시작: " + uuid.toString() + " (경로: " + object.getPath() + ")");
        auto& sdbusObj = object.getSdbusObject();
        
        // V2 API를 사용하여 vtable 생성 및 등록
        sdbus::InterfaceName interfaceName{BlueZConstants::GATT_CHARACTERISTIC_INTERFACE};
        
        // UUID 속성 vtable
        auto uuidVTable = sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_UUID})
                            .withGetter([this](){ return uuid.toBlueZFormat(); });
        
        // Service 속성 vtable (부모 서비스가 있는 경우)
        auto serviceVTable = parentService ? 
            sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_SERVICE})
                .withGetter([this](){ return sdbus::ObjectPath(parentService->getPath()); }) : 
            sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_SERVICE})
                .withGetter([](){ return sdbus::ObjectPath("/"); });
        
        // Value 속성 vtable
        auto valueVTable = sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_VALUE})
                             .withGetter([this]() -> std::vector<uint8_t> {
                                 std::lock_guard<std::mutex> lock(valueMutex);
                                 return value;
                             });
        
        // Flags 속성 vtable (특성 속성)
        auto flagsVTable = sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_FLAGS})
                              .withGetter([this]() -> std::vector<std::string> {
                                  std::vector<std::string> flags;
                                  
                                  if (properties & GattProperty::PROP_BROADCAST)
                                      flags.push_back(BlueZConstants::FLAG_BROADCAST);
                                  if (properties & GattProperty::PROP_READ)
                                      flags.push_back(BlueZConstants::FLAG_READ);
                                  if (properties & GattProperty::PROP_WRITE_WITHOUT_RESPONSE)
                                      flags.push_back(BlueZConstants::FLAG_WRITE_WITHOUT_RESPONSE);
                                  if (properties & GattProperty::PROP_WRITE)
                                      flags.push_back(BlueZConstants::FLAG_WRITE);
                                  if (properties & GattProperty::PROP_NOTIFY)
                                      flags.push_back(BlueZConstants::FLAG_NOTIFY);
                                  if (properties & GattProperty::PROP_INDICATE)
                                      flags.push_back(BlueZConstants::FLAG_INDICATE);
                                  if (properties & GattProperty::PROP_AUTHENTICATED_SIGNED_WRITES)
                                      flags.push_back(BlueZConstants::FLAG_AUTHENTICATED_SIGNED_WRITES);
                                  if (properties & GattProperty::PROP_EXTENDED_PROPERTIES)
                                      flags.push_back(BlueZConstants::FLAG_EXTENDED_PROPERTIES);
                                  if (permissions & GattPermission::PERM_READ_ENCRYPTED)
                                      flags.push_back(BlueZConstants::FLAG_ENCRYPT_READ);
                                  if (permissions & GattPermission::PERM_WRITE_ENCRYPTED)
                                      flags.push_back(BlueZConstants::FLAG_ENCRYPT_WRITE);
                                  if (permissions & GattPermission::PERM_READ_AUTHENTICATED)
                                      flags.push_back(BlueZConstants::FLAG_ENCRYPT_AUTHENTICATED_READ);
                                  if (permissions & GattPermission::PERM_WRITE_AUTHENTICATED)
                                      flags.push_back(BlueZConstants::FLAG_ENCRYPT_AUTHENTICATED_WRITE);
                                  
                                  return flags;
                              });
        
        // Notifying 속성 vtable
        auto notifyingVTable = sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_NOTIFYING})
                                 .withGetter([this]() -> bool {
                                     std::lock_guard<std::mutex> lock(notifyMutex);
                                     return notifying;
                                 });
        
        // Descriptors 속성 vtable
        auto descriptorsVTable = sdbus::registerProperty(sdbus::PropertyName{"Descriptors"})
                                   .withGetter([this]() -> std::vector<sdbus::ObjectPath> {
                                       std::vector<sdbus::ObjectPath> paths;
                                       std::lock_guard<std::mutex> lock(descriptorsMutex);
                                       
                                       for (const auto& pair : descriptors) {
                                           if (pair.second) {  // null 체크
                                               paths.push_back(sdbus::ObjectPath(pair.second->getPath()));
                                           }
                                       }
                                       
                                       return paths;
                                   });
        
        // ReadValue 메서드 vtable
        auto readValueVTable = sdbus::registerMethod(sdbus::MethodName{BlueZConstants::READ_VALUE})
                                 .implementedAs([this](const std::map<std::string, sdbus::Variant>& options) -> std::vector<uint8_t> {
                                     return handleReadValue(options);
                                 });
        
        // WriteValue 메서드 vtable
        auto writeValueVTable = sdbus::registerMethod(sdbus::MethodName{BlueZConstants::WRITE_VALUE})
                                  .implementedAs([this](const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options) {
                                      handleWriteValue(value, options);
                                  });
        
        // StartNotify/StopNotify 메서드 vtable
        auto startNotifyVTable = sdbus::registerMethod(sdbus::MethodName{BlueZConstants::START_NOTIFY})
                                   .implementedAs([this]() { handleStartNotify(); });
        
        auto stopNotifyVTable = sdbus::registerMethod(sdbus::MethodName{BlueZConstants::STOP_NOTIFY})
                                  .implementedAs([this]() { handleStopNotify(); });
        
        // vtable 등록 - 이 호출에서 D-Bus 객체가 자동으로 등록됨
        sdbusObj.addVTable(
            uuidVTable, serviceVTable, valueVTable, flagsVTable, 
            notifyingVTable, descriptorsVTable,
            readValueVTable, writeValueVTable, 
            startNotifyVTable, stopNotifyVTable
        ).forInterface(interfaceName);
        
        // 모든 설명자에 대해 인터페이스 설정 (계층적 등록)
        std::lock_guard<std::mutex> lock(descriptorsMutex);
        for (const auto& [uuid, descriptor] : descriptors) {
            if (descriptor && !descriptor->isInterfaceSetup()) {
                Logger::debug("설명자 인터페이스 설정 시작: " + uuid);
                if (!descriptor->setupInterfaces()) {
                    Logger::error("설명자 인터페이스 설정 실패: " + uuid);
                    return false;
                }
            }
        }
        
        // BlueZ 5.82에서는 CCCD 설명자가 알림/표시 지원 특성에 대해 자동으로 생성됨
        // 따라서 명시적으로 생성할 필요가 없음
        
        // 설정 완료 표시
        interfaceSetup = true;
        Logger::info("특성 인터페이스 설정 완료: " + uuid.toString());
        return true;
    } catch (const std::exception& e) {
        Logger::error("특성 인터페이스 설정 실패: " + std::string(e.what()));
        return false;
    }
}

std::vector<uint8_t> GattCharacteristic::handleReadValue(const std::map<std::string, sdbus::Variant>& options) {
    Logger::debug("특성에 대해 ReadValue 호출됨: " + uuid.toString());
    
    // 옵션 처리 (예: offset)
    uint16_t offset = 0;
    if (options.count("offset") > 0) {
        try {
            offset = options.at("offset").get<uint16_t>();
            Logger::debug("읽기 오프셋: " + std::to_string(offset));
        } catch (...) {
            // 타입 변환 오류 무시
        }
    }
    
    // BlueZ 5.82에서 추가된 새 옵션 확인 (예: MTU)
    if (options.count("mtu") > 0) {
        try {
            uint16_t mtu = options.at("mtu").get<uint16_t>();
            Logger::debug("읽기 MTU: " + std::to_string(mtu));
        } catch (...) {
            // 타입 변환 오류 무시
        }
    }
    
    std::vector<uint8_t> returnValue;
    
    // 콜백 사용
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex);
        if (readCallback) {
            try {
                returnValue = readCallback();
            } catch (const std::exception& e) {
                Logger::error("읽기 콜백에서 예외: " + std::string(e.what()));
                throw sdbus::Error(sdbus::Error::Name("org.bluez.Error.Failed"), e.what());
            }
        } else {
            // 저장된 값 사용
            std::lock_guard<std::mutex> valueLock(valueMutex);
            returnValue = value;
        }
    }
    
    // offset 적용 (있는 경우)
    if (offset > 0 && offset < returnValue.size()) {
        returnValue.erase(returnValue.begin(), returnValue.begin() + offset);
    }
    
    return returnValue;
}

void GattCharacteristic::handleWriteValue(const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options) {
    Logger::debug("특성에 대해 WriteValue 호출됨: " + uuid.toString());
    
    // 옵션 처리 (예: offset)
    uint16_t offset = 0;
    if (options.count("offset") > 0) {
        try {
            offset = options.at("offset").get<uint16_t>();
            Logger::debug("쓰기 오프셋: " + std::to_string(offset));
        } catch (...) {
            // 타입 변환 오류 무시
        }
    }
    
    // BlueZ 5.82의 추가 옵션 처리
    if (options.count("type") > 0) {
        try {
            std::string type = options.at("type").get<std::string>();
            Logger::debug("쓰기 타입: " + type);
            // "command", "request", "reliable" 등 가능
        } catch (...) {
            // 타입 변환 오류 무시
        }
    }
    
    // 콜백 사용
    bool success = true;
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex);
        if (writeCallback) {
            try {
                success = writeCallback(value);
            } catch (const std::exception& e) {
                Logger::error("쓰기 콜백에서 예외: " + std::string(e.what()));
                throw sdbus::Error(sdbus::Error::Name("org.bluez.Error.Failed"), e.what());
            }
        }
    }
    
    if (success) {
        // offset 적용하여 값 설정
        if (offset > 0) {
            std::lock_guard<std::mutex> valueLock(valueMutex);
            // 필요시 기존 값 확장
            if (offset >= this->value.size()) {
                this->value.resize(offset + value.size(), 0);
            }
            // offset 위치에 새 값 복사
            std::copy(value.begin(), value.end(), this->value.begin() + offset);
            
            // 값 변경 알림 발생
            if (object.isRegistered()) {
                object.emitPropertyChanged(
                    sdbus::InterfaceName{BlueZConstants::GATT_CHARACTERISTIC_INTERFACE}, 
                    sdbus::PropertyName{BlueZConstants::PROPERTY_VALUE}
                );
            }
        } else {
            // 그냥 전체 값 설정
            setValue(value);
        }
    } else {
        // 콜백 실패 시 예외 발생
        throw sdbus::Error(sdbus::Error::Name("org.bluez.Error.Failed"), "Write operation failed");
    }
}

void GattCharacteristic::handleStartNotify() {
    Logger::debug("특성에 대해 StartNotify 호출됨: " + uuid.toString());
    
    // BlueZ 5.82에서는 StartNotify 호출 시 자동으로 CCCD 설정을 처리함
    
    if (!startNotify()) {
        throw sdbus::Error(sdbus::Error::Name("org.bluez.Error.Failed"), "Cannot start notifications");
    }
}

void GattCharacteristic::handleStopNotify() {
    Logger::debug("특성에 대해 StopNotify 호출됨: " + uuid.toString());
    
    // BlueZ 5.82에서는 StopNotify 호출 시 자동으로 CCCD 설정을 처리함
    
    if (!stopNotify()) {
        throw sdbus::Error(sdbus::Error::Name("org.bluez.Error.Failed"), "Cannot stop notifications");
    }
}

} // namespace ggk