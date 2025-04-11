#pragma once

#include <string>
#include <gio/gio.h>
#include "DBusTypes.h"

namespace ggk {

/**
 * @brief D-Bus 오류를 처리하는 클래스
 * 
 * BlueZ 5.82+ 호환 D-Bus 오류 처리를 위한 개선된 클래스
 */
class DBusError {
public:
    // 표준 D-Bus 오류 코드
    static const char* ERROR_FAILED;
    static const char* ERROR_NO_REPLY;
    static const char* ERROR_NOT_SUPPORTED;
    static const char* ERROR_INVALID_ARGS;
    static const char* ERROR_INVALID_SIGNATURE;
    static const char* ERROR_UNKNOWN_METHOD;
    static const char* ERROR_UNKNOWN_OBJECT;
    static const char* ERROR_UNKNOWN_INTERFACE;
    static const char* ERROR_UNKNOWN_PROPERTY;
    static const char* ERROR_PROPERTY_READ_ONLY;
    
    /**
     * @brief 이름과 메시지로 오류 생성
     * 
     * @param name 오류 이름 (D-Bus 오류 코드)
     * @param message 오류 메시지
     */
    DBusError(const std::string& name, const std::string& message);
    
    /**
     * @brief GError로부터 오류 생성
     * 
     * @param error GError 포인터 (소유권 이전 없음)
     */
    explicit DBusError(const GError* error);
    
    /**
     * @brief 오류 이름 획득
     * 
     * @return 오류 이름
     */
    const std::string& getName() const { return name; }
    
    /**
     * @brief 오류 메시지 획득
     * 
     * @return 오류 메시지
     */
    const std::string& getMessage() const { return message; }
    
    /**
     * @brief GError로 변환
     * 
     * 이 메서드는 새로운 GError 객체를 생성하여 스마트 포인터로 반환합니다.
     * 
     * @return GErrorPtr GError를 관리하는 스마트 포인터
     */
    GErrorPtr toGError() const;
    
    /**
     * @brief 예외 메시지로 변환
     * 
     * @return 문자열 형식의 오류 메시지
     */
    std::string toString() const;
    
    /**
     * @brief 특정 D-Bus 오류에 해당하는지 확인
     * 
     * @param errorName 확인할 오류 이름
     * @return 일치 여부
     */
    bool isErrorType(const std::string& errorName) const;

private:
    std::string name;    ///< 오류 이름 (D-Bus 오류 코드)
    std::string message; ///< 오류 메시지
};

} // namespace ggk