// DBusTypes.h
#pragma once

#include <gio/gio.h>
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace ggk {

// D-Bus 기본 타입 정의
struct DBusArgument {
    std::string signature;   // D-Bus 타입 시그니처
    std::string name;        // 인자 이름
    std::string direction;   // "in" 또는 "out"
    std::string description; // 문서화용 설명
};

// D-Bus 메서드 호출을 위한 구조체
struct DBusMethodCall {
    std::string sender;      // 메서드 호출자의 D-Bus 이름
    std::string interface;   // 인터페이스 이름
    std::string method;      // 메서드 이름
    GVariant* parameters;    // 메서드 파라미터
    GDBusMethodInvocation* invocation;  // 호출 컨텍스트
    uint32_t timeout_ms;     // 타임아웃 (밀리초)
};

// D-Bus 시그널 정의
struct DBusSignal {
    std::string name;
    std::vector<DBusArgument> arguments;
    std::string description;
};

// D-Bus 시그널 발신을 위한 구조체
struct DBusSignalEmission {
    GDBusConnection* connection;  // D-Bus 연결
    std::string destination;     // 대상 (없으면 브로드캐스트)
    GDBusSignalFlags flags;      // 시그널 플래그
    GVariant* parameters;        // 시그널 파라미터
    std::string interface;       // 인터페이스 이름
};

// D-Bus 프로퍼티 정의
struct DBusProperty {
    std::string name;            // 프로퍼티 이름
    std::string signature;       // D-Bus 타입 시그니처
    bool readable;              // 읽기 가능 여부
    bool writable;              // 쓰기 가능 여부
    bool emitsChangedSignal;    // 값 변경 시 시그널 발생 여부
    std::function<GVariant*()> getter;  // 값 읽기 함수
    std::function<void(GVariant*)> setter;  // 값 쓰기 함수
    std::function<bool(const std::string&)> accessControl;  // 접근 제어 함수
};

// D-Bus 인트로스펙션 정보
struct DBusIntrospection {
    std::string version;        // 인터페이스 버전
    std::string description;    // 인터페이스 설명
    bool includeStandardInterfaces;  // 표준 인터페이스 포함 여부
    std::map<std::string, std::string> annotations;  // 추가 메타데이터
};

// D-Bus 보안 정책
struct DBusSecurity {
    bool requireAuth;           // 인증 필요 여부
    std::vector<std::string> allowedUsers;   // 허용된 사용자 목록
    std::vector<std::string> allowedGroups;  // 허용된 그룹 목록
    std::function<bool(const std::string&)> authorizer;  // 커스텀 인증 함수
};

} // namespace ggk