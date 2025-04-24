// GattDescriptor.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <functional>
#include <string>
#include "BleConstants.h"
#include "xml/GattDescriptor1.h"

namespace ble {

class GattDescriptor : public sdbus::AdaptorInterfaces<org::bluez::GattDescriptor1_adaptor> {
public:
    GattDescriptor(sdbus::IConnection& connection,
                  const std::string& path,
                  const std::string& uuid,
                  uint8_t permissions,
                  const std::string& characteristicPath);
    
    virtual ~GattDescriptor();
    
    // GattDescriptor1_adaptor required methods
    std::vector<uint8_t> ReadValue(const std::map<std::string, sdbus::Variant>& options) override;
    void WriteValue(const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options) override;
    
    // Property getters/setters
    std::string UUID() override;
    sdbus::ObjectPath Characteristic() override;
    std::vector<uint8_t> Value() override;
    std::vector<std::string> Flags() override;
    uint16_t Handle() override;
    void Handle(const uint16_t& value) override;
    
    // Utility methods
    std::string getPath() const { return m_objectPath; }
    void setValue(const std::vector<uint8_t>& value);
    
    // Callback setters
    void setReadCallback(std::function<std::vector<uint8_t>()> callback) { 
        m_readCallback = std::move(callback); 
    }
    
    void setWriteCallback(std::function<bool(const std::vector<uint8_t>&)> callback) { 
        m_writeCallback = std::move(callback); 
    }
    
private:
    std::string m_objectPath;
    std::string m_uuid;
    uint8_t m_permissions;
    std::string m_characteristicPath;
    std::vector<uint8_t> m_value;
    uint16_t m_handle = 0;
    
    // Callback functions
    std::function<std::vector<uint8_t>()> m_readCallback;
    std::function<bool(const std::vector<uint8_t>&)> m_writeCallback;
};

} // namespace ble