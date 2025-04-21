// src/GattDescriptor.cpp
#include "GattDescriptor.h"
#include <iostream>

namespace ggk {

GattDescriptor::GattDescriptor(sdbus::IConnection& connection,
                             const std::string& path,
                             const GattUuid& uuid,
                             uint8_t permissions,
                             const std::string& characteristicPath)
    : AdaptorInterfaces(connection, sdbus::ObjectPath(path)),
      m_objectPath(path),
      m_uuid(uuid),
      m_permissions(permissions),
      m_characteristicPath(characteristicPath) {
    
    // 인터페이스 등록
    registerAdaptor();
    std::cout << "GattDescriptor 생성됨: " << m_objectPath << " (UUID: " << uuid.toString() << ")" << std::endl;
}

GattDescriptor::~GattDescriptor() {
    // 어댑터 등록 해제
    unregisterAdaptor();
    std::cout << "GattDescriptor 소멸됨: " << m_objectPath << std::endl;
}

std::vector<uint8_t> GattDescriptor::ReadValue(const std::map<std::string, sdbus::Variant>& options) {
    std::cout << "Descriptor ReadValue 호출됨: " << m_objectPath << std::endl;
    
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

void GattDescriptor::WriteValue(const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options) {
    std::cout << "Descriptor WriteValue 호출됨: " << m_objectPath << std::endl;
    
    // 옵션 처리 (예: offset)
    uint16_t offset = 0;
    if (options.count("offset") > 0) {
        try {
            offset = options.at("offset").get<uint16_t>();
        } catch (...) {
            // 변환 실패 시 offset = 0 유지
        }
    }
    
    // CCCD (Client Characteristic Configuration Descriptor, UUID 0x2902) 특별 처리
    if (m_uuid.toBlueZFormat() == "00002902-0000-1000-8000-00805f9b34fb") {
        std::cout << "CCCD 값 설정: ";
        if (!value.empty()) {
            std::cout << "0x" << std::hex << (int)value[0] << std::dec;
        }
        std::cout << std::endl;
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
}

std::string GattDescriptor::UUID() {
    return m_uuid.toBlueZFormat();
}

sdbus::ObjectPath GattDescriptor::Characteristic() {
    return sdbus::ObjectPath(m_characteristicPath);
}

std::vector<std::string> GattDescriptor::Flags() {
    std::vector<std::string> flags;
    
    if (m_permissions & GattPermission::PERM_READ)
        flags.push_back("read");
    if (m_permissions & GattPermission::PERM_WRITE)
        flags.push_back("write");
    if (m_permissions & GattPermission::PERM_READ_ENCRYPTED)
        flags.push_back("encrypt-read");
    if (m_permissions & GattPermission::PERM_WRITE_ENCRYPTED)
        flags.push_back("encrypt-write");
    if (m_permissions & GattPermission::PERM_READ_AUTHENTICATED)
        flags.push_back("encrypt-authenticated-read");
    if (m_permissions & GattPermission::PERM_WRITE_AUTHENTICATED)
        flags.push_back("encrypt-authenticated-write");
    
    return flags;
}

std::string GattDescriptor::getPath() const {
    return m_objectPath;
}

void GattDescriptor::setValue(const std::vector<uint8_t>& value) {
    m_value = value;
}

} // namespace ggk