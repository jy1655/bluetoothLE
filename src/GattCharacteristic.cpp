// GattCharacteristic.cpp
#include "GattCharacteristic.h"
#include <iostream>

namespace ble {

GattCharacteristic::GattCharacteristic(sdbus::IConnection& connection,
                                    const std::string& path,
                                    const std::string& uuid,
                                    uint8_t properties,
                                    const std::string& servicePath)
    : AdaptorInterfaces(connection, sdbus::ObjectPath(path)),
      m_objectPath(path),
      m_uuid(uuid),
      m_properties(properties),
      m_servicePath(servicePath) {
    
    // Initialize value
    m_value = {0};
    
    // Register the adaptor
    registerAdaptor();
    
    // Emit the InterfacesAdded signal for this object
    getObject().emitInterfacesAddedSignal({sdbus::InterfaceName{org::bluez::GattCharacteristic1_adaptor::INTERFACE_NAME}});
    
    std::cout << "GattCharacteristic created: " << m_objectPath << " (UUID: " << uuid << ")" << std::endl;
}

GattCharacteristic::~GattCharacteristic() {
    // Emit the InterfacesRemoved signal when this object is destroyed
    getObject().emitInterfacesRemovedSignal({sdbus::InterfaceName{org::bluez::GattCharacteristic1_adaptor::INTERFACE_NAME}});
    
    // Unregister the adaptor
    unregisterAdaptor();
    
    std::cout << "GattCharacteristic destroyed: " << m_objectPath << std::endl;
}

std::vector<uint8_t> GattCharacteristic::ReadValue(const std::map<std::string, sdbus::Variant>& options) {
    std::cout << "ReadValue called on: " << m_objectPath << std::endl;
    
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

void GattCharacteristic::WriteValue(const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options) {
    std::cout << "WriteValue called on: " << m_objectPath << std::endl;
    
    // Process options (e.g., offset)
    uint16_t offset = 0;
    if (options.count("offset") > 0) {
        try {
            offset = options.at("offset").get<uint16_t>();
        } catch (...) {
            // Keep offset = 0 if conversion fails
        }
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
    std::cout << "Written value: ";
    for (auto byte : value) {
        std::cout << std::hex << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Emit PropertiesChanged if this is a read & notify characteristic
    if (m_properties & GattProperty::PROP_NOTIFY) {
        notifyValueChanged();
    }
}

void GattCharacteristic::StartNotify() {
    std::cout << "StartNotify called on: " << m_objectPath << std::endl;
    
    if (!(m_properties & (GattProperty::PROP_NOTIFY | GattProperty::PROP_INDICATE))) {
        throw sdbus::Error(sdbus::Error::Name("org.bluez.Error.NotSupported"), "Characteristic does not support notifications");
    }
    
    if (!m_notifying) {
        m_notifying = true;
    }
}

void GattCharacteristic::StopNotify() {
    std::cout << "StopNotify called on: " << m_objectPath << std::endl;
    
    if (m_notifying) {
        m_notifying = false;
    }
}

std::string GattCharacteristic::UUID() {
    return m_uuid;
}

sdbus::ObjectPath GattCharacteristic::Service() {
    return sdbus::ObjectPath(m_servicePath);
}

std::vector<uint8_t> GattCharacteristic::Value() {
    return m_value;
}

bool GattCharacteristic::WriteAcquired() {
    // Always return false as we don't support acquire-write
    return false;
}

bool GattCharacteristic::NotifyAcquired() {
    // Always return false as we don't support acquire-notify
    return false;
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
    if (m_properties & GattProperty::PROP_EXTENDED_PROPERTIES)
        flags.push_back("extended-properties");
    
    return flags;
}

uint16_t GattCharacteristic::Handle() {
    // BlueZ-assigned handle value, typically 0x0000 (auto-assign)
    return m_handle;
}

void GattCharacteristic::Handle(const uint16_t& value) {
    // Handle value setter - typically not used (BlueZ manages it)
    m_handle = value;
}

uint16_t GattCharacteristic::MTU() {
    // Return 0 for no connection or let BlueZ provide the actual value
    return 0;
}

void GattCharacteristic::setValue(const std::vector<uint8_t>& value) {
    m_value = value;
}

void GattCharacteristic::notifyValueChanged() {
    if (m_notifying && (m_properties & GattProperty::PROP_NOTIFY)) {
        // Create a vector of property names to notify
        std::vector<sdbus::PropertyName> properties;
        properties.push_back(sdbus::PropertyName("Value"));
        
        // Emit a PropertiesChanged signal for the Value property
        getObject().emitPropertiesChangedSignal(
            org::bluez::GattCharacteristic1_adaptor::INTERFACE_NAME,
            properties
        );
        
        std::cout << "Notification sent for: " << m_objectPath << std::endl;
    }
}

} // namespace ble