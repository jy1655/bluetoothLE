// src/GattService.cpp
#include "GattService.h"
#include <iostream>

namespace ggk {

GattService::GattService(sdbus::IConnection& connection, 
                         const std::string& path,
                         const GattUuid& uuid, 
                         bool isPrimary)
    : AdaptorInterfaces(connection, sdbus::ObjectPath(path)),
      m_objectPath(path),
      m_uuid(uuid),
      m_isPrimary(isPrimary) {
    
    // 인터페이스 등록
    registerAdaptor();
    std::cout << "GattService 생성됨: " << m_objectPath << " (UUID: " << uuid.toString() << ")" << std::endl;
}

GattService::~GattService() {
    // 어댑터 등록 해제
    unregisterAdaptor();
    std::cout << "GattService 소멸됨: " << m_objectPath << std::endl;
}

std::string GattService::UUID() {
    return m_uuid.toBlueZFormat();
}

bool GattService::Primary() {
    return m_isPrimary;
}

std::vector<sdbus::ObjectPath> GattService::Includes() {
    // 이 서비스가 포함하는 다른 서비스 목록 (일반적으로 비어있음)
    return {};
}

} // namespace ggk