// GattCharacteristic.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <functional>
#include <string>
#include <vector>
#include "BleConstants.h"
#include "xml/GattCharacteristic1.h"

namespace ble {

class GattCharacteristic : public sdbus::AdaptorInterfaces<org::bluez::GattCharacteristic1_adaptor> {
public:
    GattCharacteristic(sdbus::IConnection& connection,
                      const std::string& path,
                      const std::string& uuid,
                      uint8_t properties,
                      const std::string& servicePath);
    
    virtual ~GattCharacteristic();
    
    // GattCharacteristic1_adaptor required methods
    std::vector<uint8_t> ReadValue(const std::map<std::string, sdbus::Variant>& options) override;
    void WriteValue(const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options) override;
    void StartNotify() override;
    void StopNotify() override;
    
    // Property getters/setters
    std::string UUID() override;
    sdbus::ObjectPath Service() override;
    std::vector<uint8_t> Value() override;
    bool WriteAcquired() override;
    bool NotifyAcquired() override;
    bool Notifying() override;
    std::vector<std::string> Flags() override;
    uint16_t Handle() override;
    void Handle(const uint16_t& value) override;
    uint16_t MTU() override;
    
    // Utility methods
    std::string getPath() const { return m_objectPath; }
    void setValue(const std::vector<uint8_t>& value);
    void notifyValueChanged();
    
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
    uint8_t m_properties;
    std::string m_servicePath;
    std::vector<uint8_t> m_value;
    bool m_notifying = false;
    uint16_t m_handle = 0;
    
    // Callback functions
    std::function<std::vector<uint8_t>()> m_readCallback;
    std::function<bool(const std::vector<uint8_t>&)> m_writeCallback;
};

} // namespace ble