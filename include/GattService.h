// GattService.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include "BleConstants.h"
#include "xml/GattService1.h"

namespace ble {

class GattService : public sdbus::AdaptorInterfaces<org::bluez::GattService1_adaptor> {
public:
    GattService(sdbus::IConnection& connection, 
               const std::string& path,
               const std::string& uuid, 
               bool isPrimary);
    
    virtual ~GattService();
    
    // GattService1_adaptor required methods
    std::string UUID() override;
    bool Primary() override;
    std::vector<sdbus::ObjectPath> Includes() override;
    uint16_t Handle() override;
    void Handle(const uint16_t& value) override;
    
    // Utility methods
    std::string getPath() const { return m_objectPath; }
    std::string getUuid() const { return m_uuid; }
    
private:
    std::string m_objectPath;
    std::string m_uuid;
    bool m_isPrimary;
    uint16_t m_handle = 0;
};

} // namespace ble