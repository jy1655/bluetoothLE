// include/IGattNode.h
#pragma once

#include <string>
#include "GattTypes.h"

namespace ggk {

/**
 * @brief 모든 GATT 노드(Service, Characteristic, Descriptor)의 공통 인터페이스
 */
class IGattNode {
public:
    virtual ~IGattNode() = default;
    
    /**
     * @brief 노드의 UUID 가져오기
     * @return UUID 객체
     */
    virtual const GattUuid& getUuid() const = 0;
    
    /**
     * @brief 노드의 D-Bus 객체 경로 가져오기
     * @return 객체 경로 문자열
     */
    virtual const std::string& getPath() const = 0;
    
    /**
     * @brief 인터페이스 설정 (D-Bus vtable 등록)
     * @return 설정 성공 여부
     */
    virtual bool setupInterfaces() = 0;
    
    /**
     * @brief 인터페이스 설정 상태 확인
     * @return 설정 완료 여부
     */
    virtual bool isInterfaceSetup() const = 0;
};

} // namespace ggk