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
    getObject().emitInterfacesRemovedSignal({sdbus::InterfaceName{org::bluez::GattService1_adaptor::INTERFACE_NAME}});
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

uint16_t GattService::Handle() {
    // BlueZ에 의해 할당된 핸들 값, 일반적으로 0x0000(자동 할당)
    return 0x0000;
}

void GattService::Handle(const uint16_t& value) {
    // 핸들 값 설정 - 일반적으로 동작하지 않음 (BlueZ가 관리)
    // 구현만 존재하는 더미 함수
}

} // namespace ggk