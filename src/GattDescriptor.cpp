// GattDescriptor.cpp
#include "GattDescriptor.h"
#include <iostream>

namespace ble {

GattDescriptor::GattDescriptor(sdbus::IConnection& connection,
                             const std::string& path,
                             const std::string& uuid,
                             uint8_t permissions,
                             const std::string& characteristicPath)
    : AdaptorInterfaces(connection, sdbus::ObjectPath(path)),
      m_objectPath(path),
      m_uuid(uuid),
      m_permissions(permissions),
      m_characteristicPath(characteristicPath) {
    
    // Register the adaptor
    registerAdaptor();
    
    // Emit the InterfacesAdded signal for this object
    getObject().emitInterfacesAddedSignal({sdbus::InterfaceName{org::bluez::GattDescriptor1_adaptor::INTERFACE_NAME}});
    
    std::cout << "GattDescriptor created: " << m_objectPath << " (UUID: " << uuid << ")" << std::endl;
}

GattDescriptor::~GattDescriptor() {
    // Emit the InterfacesRemoved signal when this object is destroyed
    getObject().emitInterfacesRemovedSignal({sdbus::InterfaceName{org::bluez::GattDescriptor1_adaptor::INTERFACE_NAME}});
    
    // Unregister the adaptor
    unregisterAdaptor();
    
    std::cout << "GattDescriptor destroyed: " << m_objectPath << std::endl;
}

std::vector<uint8_t> GattDescriptor::ReadValue(const std::map<std::string, sdbus::Variant>& options) {
    std::cout << "Descriptor ReadValue called on: " << m_objectPath << std::endl;
    
    // Process options (e.g., offset)
    uint16_t offset = 0;
    if (options.count("offset") > 0) {
        try {
            offset = options.at("offset").get<uint16_t>();
        } catch (...) {
            // Keep offset = 0 if conversion fails
        }
    }
    
    // Use the read callback if provided
    if (m_readCallback) {
        return m_readCallback();
    }
    
    // Otherwise return the stored value
    if (offset < m_value.size()) {
        return std::vector<uint8_t>(m_value.begin() + offset, m_value.end());
    }
    
    return {};
}

void GattDescriptor::WriteValue(const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options) {
    std::cout << "Descriptor WriteValue called on: " << m_objectPath << std::endl;
    
    // Process options (e.g., offset)
    uint16_t offset = 0;
    if (options.count("offset") > 0) {
        try {
            offset = options.at("offset").get<uint16_t>();
        } catch (...) {
            // Keep offset = 0 if conversion fails
        }
    }
    
    // Special handling for CCCD (Client Characteristic Configuration Descriptor)
    if (m_uuid == BleConstants::CCCD_UUID) {
        std::cout << "CCCD value set: ";
        if (!value.empty()) {
            std::cout << "0x" << std::hex << static_cast<int>(value[0]) << std::dec;
        }
        std::cout << std::endl;
    }
    
    // Use the write callback if provided
    if (m_writeCallback) {
        if (!m_writeCallback(value)) {
            throw sdbus::Error(sdbus::Error::Name("org.bluez.Error.Failed"), "Write operation rejected by callback");
        }
    }
    
    // Update the value
    if (offset == 0) {
        m_value = value;
    } else {
        // Handle partial writes with offset
        if (offset >= m_value.size()) {
            m_value.resize(offset + value.size(), 0);
        }
        
        std::copy(value.begin(), value.end(), m_value.begin() + offset);
    }
    
    // Print the received value for debugging
    std::cout << "Descriptor value: ";
    for (auto byte : value) {
        std::cout << std::hex << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Emit PropertiesChanged signal for Value property
    std::vector<sdbus::PropertyName> properties;
    properties.push_back(sdbus::PropertyName("Value"));
    getObject().emitPropertiesChangedSignal(
        org::bluez::GattDescriptor1_adaptor::INTERFACE_NAME,
        properties
    );
}

std::string GattDescriptor::UUID() {
    return m_uuid;
}

sdbus::ObjectPath GattDescriptor::Characteristic() {
    return sdbus::ObjectPath(m_characteristicPath);
}

std::vector<uint8_t> GattDescriptor::Value() {
    return m_value;
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

uint16_t GattDescriptor::Handle() {
    // BlueZ-assigned handle value, typically 0x0000 (auto-assign)
    return m_handle;
}

void GattDescriptor::Handle(const uint16_t& value) {
    // Handle value setter - typically not used (BlueZ manages it)
    m_handle = value;
}

void GattDescriptor::setValue(const std::vector<uint8_t>& value) {
    m_value = value;
    
    // Emit PropertiesChanged signal for Value property
    std::vector<sdbus::PropertyName> properties;
    properties.push_back(sdbus::PropertyName("Value"));
    
    getObject().emitPropertiesChangedSignal(
        org::bluez::GattDescriptor1_adaptor::INTERFACE_NAME,
        properties
    );
}

} // namespace ble