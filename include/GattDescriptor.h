// include/GattDescriptor.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include "GattTypes.h"
#include "xml/GattDescriptor1.h"

namespace ggk {

class GattDescriptor : public sdbus::AdaptorInterfaces<org::bluez::GattDescriptor1_adaptor> {
public:
    GattDescriptor(sdbus::IConnection& connection,
                  const std::string& path,
                  const GattUuid& uuid,
                  uint8_t permissions,
                  const std::string& characteristicPath);
    
    ~GattDescriptor();
    
    // GattDescriptor1_adaptor 필수 메소드 구현
    std::vector<uint8_t> ReadValue(const std::map<std::string, sdbus::Variant>& options) override;
    void WriteValue(const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options) override;
    
    // 속성 구현
    std::string UUID() override;
    sdbus::ObjectPath Characteristic() override;
    std::vector<uint8_t> Value() override;
    std::vector<std::string> Flags() override;
    uint16_t Handle() override;
    void Handle(const uint16_t& value) override;
    
    // 경로 얻기
    std::string getPath() const;
    
    // 값 설정
    void setValue(const std::vector<uint8_t>& value);
    
    sdbus::IObject& getObject() { return AdaptorInterfaces::getObject(); }

private:
    std::string m_objectPath;
    GattUuid m_uuid;
    uint8_t m_permissions;
    std::string m_characteristicPath;
    std::vector<uint8_t> m_value;
    
    // 콜백 함수
    std::function<std::vector<uint8_t>()> m_readCallback;
    std::function<bool(const std::vector<uint8_t>&)> m_writeCallback;
};

} // namespace ggk