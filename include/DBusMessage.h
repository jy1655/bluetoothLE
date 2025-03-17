// DBusMessage.h
#pragma once

#include <gio/gio.h>
#include <string>
#include <memory>
#include <vector>
#include "DBusTypes.h"
#include "DBusError.h"

namespace ggk {

/**
 * @brief D-Bus 메시지를 래핑하는 클래스
 * 
 * GDBusMessage 객체를 래핑하여 메서드 호출, 응답, 시그널, 오류 등의
 * D-Bus 메시지를 생성하고 관리합니다.
 */
class DBusMessage {
public:
    /**
     * @brief 메서드 호출 메시지 생성
     * 
     * @param destination 목적지 서비스 이름
     * @param path 객체 경로
     * @param interface 인터페이스 이름
     * @param method 메서드 이름
     * @return DBusMessage 생성된 메시지
     * @throws DBusError 메시지 생성 실패 시
     */
    static DBusMessage createMethodCall(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& method
    );

    /**
     * @brief 메서드 응답 메시지 생성
     * 
     * @param invocation 메서드 호출 객체
     * @return DBusMessage 생성된 메시지
     * @throws DBusError 메시지 생성 실패 시
     */
    static DBusMessage createMethodReturn(const GDBusMethodInvocationPtr& invocation);
    
    /**
     * @brief 시그널 메시지 생성
     * 
     * @param path 객체 경로
     * @param interface 인터페이스 이름
     * @param name 시그널 이름
     * @return DBusMessage 생성된 메시지
     * @throws DBusError 메시지 생성 실패 시
     */
    static DBusMessage createSignal(
        const std::string& path,
        const std::string& interface,
        const std::string& name
    );

    /**
     * @brief 오류 메시지 생성
     * 
     * @param invocation 메서드 호출 객체
     * @param error 오류 정보
     * @return DBusMessage 생성된 메시지
     * @throws DBusError 메시지 생성 실패 시
     */
    static DBusMessage createError(
        const GDBusMethodInvocationPtr& invocation,
        const DBusError& error
    );

    /**
     * @brief 메시지에 인자 추가
     * 
     * @param variant 추가할 인자
     */
    void addArgument(const GVariantPtr& variant);
    
    /**
     * @brief 메시지에 여러 인자 추가
     * 
     * @param variants 추가할 인자 목록
     */
    void addArgumentsList(const std::vector<GVariantPtr>& variants);
    
    /**
     * @brief 메시지 본문 가져오기
     * 
     * @return GVariantPtr 메시지 본문 (없으면 null)
     */
    GVariantPtr getBody() const;

    /**
     * @brief 메시지 타입 가져오기
     * 
     * @return DBusMessageType 메시지 타입
     */
    DBusMessageType getType() const;
    
    /**
     * @brief 인터페이스 이름 가져오기
     * 
     * @return std::string 인터페이스 이름
     */
    std::string getInterface() const;
    
    /**
     * @brief 객체 경로 가져오기
     * 
     * @return std::string 객체 경로
     */
    std::string getPath() const;
    
    /**
     * @brief 멤버(메서드/시그널) 이름 가져오기
     * 
     * @return std::string 멤버 이름
     */
    std::string getMember() const;
    
    /**
     * @brief 목적지 가져오기
     * 
     * @return std::string 목적지
     */
    std::string getDestination() const;
    
    /**
     * @brief 발신자 가져오기
     * 
     * @return std::string 발신자
     */
    std::string getSender() const;
    
    /**
     * @brief 시그니처 가져오기
     * 
     * @return std::string 시그니처
     */
    std::string getSignature() const;

    /**
     * @brief 원시 GDBusMessage 객체 참조 가져오기
     * 
     * @return const GDBusMessage& 원시 객체 참조
     */
    const GDBusMessage& getMessage() const { 
        return *message.get(); 
    }

    /**
     * @brief 이동 생성자
     */
    DBusMessage(DBusMessage&& other) noexcept = default;
    
    /**
     * @brief 이동 할당 연산자
     */
    DBusMessage& operator=(DBusMessage&& other) noexcept = default;

private:
    // D-Bus 메시지 객체
    GDBusMessagePtr message;

    /**
     * @brief 생성자 - raw 포인터로부터 메시지 생성
     * 
     * @param message 원시 GDBusMessage 객체 (소유권 가져옴)
     */
    explicit DBusMessage(GDBusMessage* message);
    
    // 복사 금지
    DBusMessage(const DBusMessage&) = delete;
    DBusMessage& operator=(const DBusMessage&) = delete;
};

} // namespace ggk