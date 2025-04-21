// src/GattCharacteristic.cpp
#include "GattCharacteristic.h"
#include <iostream>

namespace ggk {

GattCharacteristic::GattCharacteristic(sdbus::IConnection& connection,
                                      const std::string& path,
                                      const GattUuid& uuid,
                                      uint8_t properties,
                                      const std::string& servicePath)
    : AdaptorInterfaces(connection, sdbus::ObjectPath(path)),
      m_objectPath(path),
      m_uuid(uuid),
      m_properties(properties),
      m_servicePath(servicePath) {
    
    // 초기값 설정
    m_value = {0};
    
    // 인터페이스 등록
    registerAdaptor();
    std::cout << "GattCharacteristic 생성됨: " << m_objectPath << " (UUID: " << uuid.toString() << ")" << std::endl;
}

GattCharacteristic::~GattCharacteristic() {
    // 어댑터 등록 해제
    unregisterAdaptor();
    std::cout << "GattCharacteristic 소멸됨: " << m_objectPath << std::endl;
}

std::vector<uint8_t> GattCharacteristic::ReadValue(const std::map<std::string, sdbus::Variant>& options) {
    std::cout << "ReadValue 호출됨: " << m_objectPath << std::endl;
    
    // 옵션 처리 (예: offset)
    uint16_t offset = 0;
    if (options.count("offset") > 0) {
        try {
            offset = options.at("offset").get<uint16_t>();
        } catch (...) {
            // 변환 실패 시 offset = 0 유지
        }
    }
    
    // 읽기 콜백이 있으면 사용
    if (m_readCallback) {
        return m_readCallback();
    }
    
    // 콜백이 없으면 저장된 값 반환
    if (offset < m_value.size()) {
        return std::vector<uint8_t>(m_value.begin() + offset, m_value.end());
    }
    
    return {};
}

void GattCharacteristic::WriteValue(const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options) {
    std::cout << "WriteValue 호출됨: " << m_objectPath << std::endl;
    
    // 옵션 처리 (예: offset)
    uint16_t offset = 0;
    if (options.count("offset") > 0) {
        try {
            offset = options.at("offset").get<uint16_t>();
        } catch (...) {
            // 변환 실패 시 offset = 0 유지
        }
    }
    
    // 쓰기 콜백이 있으면 사용
    if (m_writeCallback) {
        if (!m_writeCallback(value)) {
            throw sdbus::Error(sdbus::Error::Name("org.bluez.Error.Failed"), "Write operation rejected by callback");
        }
    }
    
    // 값 업데이트
    if (offset == 0) {
        m_value = value;
    } else {
        // offset이 있는 경우 부분 업데이트
        if (offset >= m_value.size()) {
            m_value.resize(offset + value.size(), 0);
        }
        
        std::copy(value.begin(), value.end(), m_value.begin() + offset);
    }
    
    // 프로퍼티 변경 시그널은 생략 - BlueZ에서 처리
}

void GattCharacteristic::StartNotify() {
    std::cout << "StartNotify 호출됨: " << m_objectPath << std::endl;
    
    if (!(m_properties & (GattProperty::PROP_NOTIFY | GattProperty::PROP_INDICATE))) {
        throw sdbus::Error(sdbus::Error::Name("org.bluez.Error.NotSupported"), "Characteristic does not support notifications");
    }
    
    if (!m_notifying) {
        m_notifying = true;
        // 프로퍼티 변경 시그널은 생략 - BlueZ에서 처리
    }
}

void GattCharacteristic::StopNotify() {
    std::cout << "StopNotify 호출됨: " << m_objectPath << std::endl;
    
    if (m_notifying) {
        m_notifying = false;
        // 프로퍼티 변경 시그널은 생략 - BlueZ에서 처리
    }
}

std::string GattCharacteristic::UUID() {
    return m_uuid.toBlueZFormat();
}

sdbus::ObjectPath GattCharacteristic::Service() {
    return sdbus::ObjectPath(m_servicePath);
}

std::vector<uint8_t> GattCharacteristic::Value() {
    return m_value;
}

bool GattCharacteristic::Notifying() {
    return m_notifying;
}

std::vector<std::string> GattCharacteristic::Flags() {
    std::vector<std::string> flags;
    
    if (m_properties & GattProperty::PROP_BROADCAST)
        flags.push_back("broadcast");
    if (m_properties & GattProperty::PROP_READ)
        flags.push_back("read");
    if (m_properties & GattProperty::PROP_WRITE_WITHOUT_RESPONSE)
        flags.push_back("write-without-response");
    if (m_properties & GattProperty::PROP_WRITE)
        flags.push_back("write");
    if (m_properties & GattProperty::PROP_NOTIFY)
        flags.push_back("notify");
    if (m_properties & GattProperty::PROP_INDICATE)
        flags.push_back("indicate");
    if (m_properties & GattProperty::PROP_AUTHENTICATED_SIGNED_WRITES)
        flags.push_back("authenticated-signed-writes");
    
    return flags;
}

std::vector<sdbus::ObjectPath> GattCharacteristic::Descriptors() {
    std::vector<sdbus::ObjectPath> paths;
    for (const auto& path : m_descriptorPaths) {
        paths.push_back(sdbus::ObjectPath(path));
    }
    return paths;
}

void GattCharacteristic::setValue(const std::vector<uint8_t>& value) {
    m_value = value;
}

} // namespace ggk