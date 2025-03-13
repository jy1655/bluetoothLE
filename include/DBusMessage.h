// DBusMessage.h
#pragma once

#include <gio/gio.h>
#include <string>
#include <memory>
#include "DBusTypes.h"
#include "DBusError.h"

namespace ggk {

class DBusMessage {
public:
    // 메시지 생성 메서드들 - 매개변수 유형 수정
    static DBusMessage createMethodCall(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& method
    );

    // GDBusMethodInvocation* -> GDBusMethodInvocationPtr& 사용
    static DBusMessage createMethodReturn(const GDBusMethodInvocationPtr& invocation);
    
    static DBusMessage createSignal(
        const std::string& path,
        const std::string& interface,
        const std::string& name
    );

    // GDBusMethodInvocation* -> GDBusMethodInvocationPtr& 사용
    static DBusMessage createError(
        const GDBusMethodInvocationPtr& invocation,
        const DBusError& error
    );

    // 파라미터 관리 - GVariant* -> GVariantPtr& 사용
    void addArgument(const GVariantPtr& variant);
    // 가변 인자는 스마트 포인터로 처리하기 어려우므로 대체 API 필요
    // 예: addArguments 대신 addArgumentsList(std::vector<GVariantPtr>& variants) 사용
    void addArgumentsList(const std::vector<GVariantPtr>& variants);
    
    // 반환 값도 스마트 포인터로
    GVariantPtr getBody() const;

    // 메시지 속성 접근자
    DBusMessageType getType() const;
    std::string getInterface() const;
    std::string getPath() const;
    std::string getMember() const;
    std::string getDestination() const;
    std::string getSender() const;
    std::string getSignature() const;

    // 언래핑 메서드 개선 - 원시 포인터 대신 참조 반환
    const GDBusMessage& getMessage() const { return *message; }

    // 이동 생성자와 이동 할당 연산자 추가
    DBusMessage(DBusMessage&& other) noexcept = default;
    DBusMessage& operator=(DBusMessage&& other) noexcept = default;

private:
    // DBusTypes.h에서 정의한 GDBusMessagePtr 사용, 중복 정의 제거
    GDBusMessagePtr message;

    // private 생성자 - 팩토리 메서드를 통해서만 생성 가능
    explicit DBusMessage(GDBusMessage* message);
    // 또는 스마트 포인터를 받는 생성자 추가
    explicit DBusMessage(GDBusMessagePtr messagePtr);
    
    // 복사 금지
    DBusMessage(const DBusMessage&) = delete;
    DBusMessage& operator=(const DBusMessage&) = delete;
};

} // namespace ggk