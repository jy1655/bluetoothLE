// GattService.cpp
#include "GattService.h"
#include <iostream>

namespace ble {

GattService::GattService(sdbus::IConnection& connection, 
                       const std::string& path,
                       const std::string& uuid, 
                       bool isPrimary)
    : AdaptorInterfaces(connection, sdbus::ObjectPath(path)),
      m_objectPath(path),
      m_uuid(uuid),
      m_isPrimary(isPrimary) {
    
    // Register the adaptor interface
    registerAdaptor();
    
    // Emit the InterfacesAdded signal for this object
    getObject().emitInterfacesAddedSignal({sdbus::InterfaceName{org::bluez::GattService1_adaptor::INTERFACE_NAME}});
    
    std::cout << "GattService created: " << m_objectPath << " (UUID: " << uuid << ")" << std::endl;
}

GattService::~GattService() {
    // Emit the InterfacesRemoved signal when this object is destroyed
    getObject().emitInterfacesRemovedSignal({sdbus::InterfaceName{org::bluez::GattService1_adaptor::INTERFACE_NAME}});
    
    // Unregister the adaptor
    unregisterAdaptor();
    
    std::cout << "GattService destroyed: " << m_objectPath << std::endl;
}

std::string GattService::UUID() {
    return m_uuid;
}

bool GattService::Primary() {
    return m_isPrimary;
}

std::vector<sdbus::ObjectPath> GattService::Includes() {
    // List of included services, usually empty
    return {};
}

uint16_t GattService::Handle() {
    // BlueZ-assigned handle value, typically 0x0000 (auto-assign)
    return m_handle;
}

void GattService::Handle(const uint16_t& value) {
    // Handle value setter - typically not used (BlueZ manages it)
    m_handle = value;
}

} // namespace ble