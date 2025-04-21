// include/GattCharacteristic.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include "GattTypes.h"
#include "xml/GattCharacteristic1.h"

namespace ggk {

class GattCharacteristic : public sdbus::AdaptorInterfaces<org::bluez::GattCharacteristic1_adaptor> {
public:
    GattCharacteristic(sdbus::IConnection& connection,
                      const std::string& path,
                      const GattUuid& uuid,
                      uint8_t properties,
                      const std::string& servicePath);
    
    ~GattCharacteristic();
    
    // GattCharacteristic1_adaptor 필수 메소드 구현
    std::vector<uint8_t> ReadValue(const std::map<std::string, sdbus::Variant>& options) override;
    void WriteValue(const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options) override;
    void StartNotify() override;
    void StopNotify() override;
    
    // 속성 구현
    std::string UUID() override;
    sdbus::ObjectPath Service() override;
    std::vector<uint8_t> Value() override;
    bool Notifying() override;
    std::vector<std::string> Flags() override;
    std::vector<sdbus::ObjectPath> Descriptors() override;
    
    // 경로 얻기
    std::string getPath() const { return m_objectPath; }
    
    // 값 설정
    void setValue(const std::vector<uint8_t>& value);
    
private:
    std::string m_objectPath;
    GattUuid m_uuid;
    uint8_t m_properties;
    std::string m_servicePath;
    std::vector<uint8_t> m_value;
    bool m_notifying = false;
    std::vector<std::string> m_descriptorPaths;
    
    // 콜백 함수
    std::function<std::vector<uint8_t>()> m_readCallback;
    std::function<bool(const std::vector<uint8_t>&)> m_writeCallback;
};

} // namespace ggk