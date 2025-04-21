// src/LEAdvertisement.cpp
#include "GattAdvertisement.h"
#include <iostream>

namespace ggk {

LEAdvertisement::LEAdvertisement(sdbus::IConnection& connection,
                               const std::string& path,
                               const std::string& name)
    : AdaptorInterfaces(connection, sdbus::ObjectPath(path)),
      m_objectPath(path),
      m_localName(name) {
    
    // 인터페이스 등록
    registerAdaptor();
    std::cout << "LEAdvertisement 생성됨: " << m_objectPath << " (이름: " << name << ")" << std::endl;
}

LEAdvertisement::~LEAdvertisement() {
    // 어댑터 등록 해제
    unregisterAdaptor();
    std::cout << "LEAdvertisement 소멸됨: " << m_objectPath << std::endl;
}

std::string LEAdvertisement::Type() {
    // peripheral 또는 broadcast
    return "peripheral";
}

void LEAdvertisement::Release() {
    std::cout << "LEAdvertisement Release 호출됨: " << m_objectPath << std::endl;
    // BlueZ가 이 광고를 해제하라고 요청할 때 호출
}

std::vector<std::string> LEAdvertisement::ServiceUUIDs() {
    return m_serviceUUIDs;
}

std::map<uint16_t, sdbus::Variant> LEAdvertisement::ManufacturerData() {
    std::map<uint16_t, sdbus::Variant> data;
    
    // 예시: 제조사 ID 0x0059 (Nordic Semiconductor)
    std::vector<uint8_t> mfgData = {0x01, 0x02, 0x03, 0x04};
    data[0x0059] = sdbus::Variant(mfgData);
    
    return data;
}

std::vector<std::string> LEAdvertisement::SolicitUUIDs() {
    // 일반적으로 사용하지 않음
    return {};
}

std::map<std::string, sdbus::Variant> LEAdvertisement::ServiceData() {
    // 서비스별 데이터 (일반적으로 사용하지 않음)
    return {};
}

std::vector<std::string> LEAdvertisement::Includes() {
    return m_includes;
}

std::string LEAdvertisement::LocalName() {
    return m_localName;
}

uint16_t LEAdvertisement::Appearance() {
    // BlueZ 5.82 호환성을 위해 구현
    // 0 = 지정되지 않음
    return 0;
}

uint16_t LEAdvertisement::Duration() {
    // 광고 지속 시간 (밀리초)
    // 0 = 무기한
    return 0;
}

uint16_t LEAdvertisement::Timeout() {
    // 광고 타임아웃 (초)
    // 0 = 타임아웃 없음
    return 0;
}

std::string LEAdvertisement::SecondaryChannel() {
    // 보조 광고 채널 (일반적으로 사용하지 않음)
    return "";
}

bool LEAdvertisement::Discoverable() {
    // BlueZ 5.82에서는 true로 설정해야 함
    return true;
}

uint16_t LEAdvertisement::DiscoverableTimeout() {
    // 발견 가능 타임아웃 (초)
    // 0 = 타임아웃 없음
    return 0;
}

uint32_t LEAdvertisement::MinInterval() {
    // 최소 광고 간격 (일반적으로 사용하지 않음)
    return 0;
}

uint32_t LEAdvertisement::MaxInterval() {
    // 최대 광고 간격 (일반적으로 사용하지 않음)
    return 0;
}

int16_t LEAdvertisement::TxPower() {
    // TX 파워 (dBm)
    // -127 ~ +20
    // -127 = 정의되지 않음
    return -127;
}

void LEAdvertisement::addServiceUUID(const GattUuid& uuid) {
    std::string uuidStr = uuid.toBlueZFormat();
    
    // 중복 방지
    for (const auto& existingUuid : m_serviceUUIDs) {
        if (existingUuid == uuidStr) return;
    }
    
    m_serviceUUIDs.push_back(uuidStr);
    std::cout << "ServiceUUID 추가됨: " << uuidStr << std::endl;
    
    // service-uuids가 Includes에 없으면 추가
    if (std::find(m_includes.begin(), m_includes.end(), "service-uuids") == m_includes.end()) {
        m_includes.push_back("service-uuids");
    }
}

} // namespace ggk