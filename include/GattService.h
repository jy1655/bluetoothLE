// include/GattService.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include "GattTypes.h"
#include "xml/GattService1.h"

namespace ggk {

class GattService : public sdbus::AdaptorInterfaces<org::bluez::GattService1_adaptor> {
public:
    GattService(sdbus::IConnection& connection, 
                const std::string& path,
                const GattUuid& uuid, 
                bool isPrimary);
    
    ~GattService();
    
    // GattService1_adaptor 필수 메소드 구현
    std::string UUID() override;
    bool Primary() override;
    std::vector<sdbus::ObjectPath> Includes() override;
    
    // 경로 얻기
    std::string getPath() const { return m_objectPath; }
    
    // UUID 얻기
    GattUuid getUuid() const { return m_uuid; }
    
private:
    std::string m_objectPath;
    GattUuid m_uuid;
    bool m_isPrimary;
};

} // namespace ggk