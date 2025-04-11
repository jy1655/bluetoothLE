#pragma once

#include <gio/gio.h>
#include <string>
#include <map>
#include <functional>
#include <mutex>
#include "Logger.h"
#include "DBusTypes.h"
#include "DBusError.h"
#include "DBusObjectPath.h"

namespace ggk {

/**
 * @brief D-Bus 연결을 관리하는 클래스
 * 
 * 이 클래스는 D-Bus 연결을 생성하고 관리하며, 메서드 호출, 시그널 발생,
 * 객체 등록 등의 D-Bus 통신 기능을 제공합니다.
 */
class DBusConnection {
public:
    // 콜백 타입 정의
    using MethodHandler = std::function<void(const DBusMethodCall&)>;
    using SignalHandler = std::function<void(const std::string&, GVariantPtr)>;
    
    /**
     * @brief 생성자
     * 
     * @param busType 연결할 D-Bus 유형 (시스템 또는 세션)
     */
    DBusConnection(GBusType busType = G_BUS_TYPE_SYSTEM);
    
    /**
     * @brief 소멸자
     * 
     * 모든 등록된 객체를 해제하고 연결을 종료합니다.
     */
    ~DBusConnection();
    
    // 복사 방지
    DBusConnection(const DBusConnection&) = delete;
    DBusConnection& operator=(const DBusConnection&) = delete;
    
    // 연결 관리
    /**
     * @brief D-Bus에 연결
     * 
     * @return 연결 성공 여부
     */
    bool connect();
    
    /**
     * @brief D-Bus 연결 해제
     * 
     * @return 연결 해제 성공 여부
     */
    bool disconnect();
    
    /**
     * @brief 연결 상태 확인
     * 
     * @return 연결 상태
     */
    bool isConnected() const;
    
    // 메시지 전송
    /**
     * @brief D-Bus 메서드 호출
     * 
     * @param destination 목적지 서비스 이름
     * @param path 객체 경로
     * @param interface 인터페이스 이름
     * @param method 메서드 이름
     * @param parameters 매개변수 (기본값: null)
     * @param replySignature 예상 응답 시그니처 (기본값: 빈 문자열)
     * @param timeoutMs 제한 시간 (밀리초, 기본값: -1 = 무제한)
     * @return 응답 데이터
     */
    GVariantPtr callMethod(
        const std::string& destination,
        const DBusObjectPath& path,
        const std::string& interface,
        const std::string& method,
        GVariantPtr parameters = makeNullGVariantPtr(),
        const std::string& replySignature = "",
        int timeoutMs = -1
    );
    
    /**
     * @brief D-Bus 시그널 발생
     * 
     * @param path 객체 경로
     * @param interface 인터페이스 이름
     * @param signalName 시그널 이름
     * @param parameters 매개변수 (기본값: null)
     * @return 성공 여부
     */
    bool emitSignal(
        const DBusObjectPath& path,
        const std::string& interface,
        const std::string& signalName,
        GVariantPtr parameters = makeNullGVariantPtr()
    );
    
    // 객체 등록
    /**
     * @brief D-Bus 객체 등록
     * 
     * @param path 객체 경로
     * @param introspectionXml 인트로스펙션 XML
     * @param methodHandlers 메서드 핸들러 맵
     * @param properties 프로퍼티 맵
     * @return 등록 성공 여부
     */
    bool registerObject(
        const DBusObjectPath& path,
        const std::string& introspectionXml,
        const std::map<std::string, std::map<std::string, MethodHandler>>& methodHandlers,
        const std::map<std::string, std::vector<DBusProperty>>& properties
    );
    
    /**
     * @brief D-Bus 객체 등록 해제
     * 
     * @param path 객체 경로
     * @return 등록 해제 성공 여부
     */
    bool unregisterObject(const DBusObjectPath& path);
    
    // 속성 변경 알림
    /**
     * @brief 속성 변경 시그널 발생
     * 
     * @param path 객체 경로
     * @param interface 인터페이스 이름
     * @param propertyName 속성 이름
     * @param value 새 값
     * @return 성공 여부
     */
    bool emitPropertyChanged(
        const DBusObjectPath& path,
        const std::string& interface,
        const std::string& propertyName,
        GVariantPtr value
    );
    
    // 시그널 처리
    /**
     * @brief 시그널 감시 추가
     * 
     * @param sender 발신자 (필터링용, 빈 문자열 = 모든 발신자)
     * @param interface 인터페이스 (필터링용, 빈 문자열 = 모든 인터페이스)
     * @param signalName 시그널 이름 (필터링용, 빈 문자열 = 모든 시그널)
     * @param path 객체 경로 (필터링용, 빈 경로 = 모든 경로)
     * @param handler 시그널 핸들러
     * @return 감시 ID (해제 시 필요)
     */
    guint addSignalWatch(
        const std::string& sender,
        const std::string& interface,
        const std::string& signalName,
        const DBusObjectPath& path,
        SignalHandler handler
    );
    
    /**
     * @brief 시그널 감시 제거
     * 
     * @param watchId 감시 ID
     * @return 제거 성공 여부
     */
    bool removeSignalWatch(guint watchId);
    
    /**
     * @brief 원시 GDBusConnection 획득
     * 
     * @return 원시 GDBusConnection 포인터
     */
    GDBusConnection* getRawConnection() const { return connection.get(); }

private:
    // D-Bus 연결
    GBusType busType;
    GDBusConnectionPtr connection;
    
    // 등록된 객체 추적
    std::map<std::string, guint> registeredObjects;
    std::map<guint, SignalHandler> signalHandlers;
    
    // 스레드 안전성
    mutable std::mutex mutex;
    
    // D-Bus 메서드 호출 핸들러
    static void handleMethodCall(
        GDBusConnection* connection,
        const gchar* sender,
        const gchar* objectPath,
        const gchar* interfaceName,
        const gchar* methodName,
        GVariant* parameters,
        GDBusMethodInvocation* invocation,
        gpointer userData
    );
    
    // 속성 접근자 핸들러
    static GVariant* handleGetProperty(
        GDBusConnection* connection,
        const gchar* sender,
        const gchar* objectPath,
        const gchar* interfaceName,
        const gchar* propertyName,
        GError** error,
        gpointer userData
    );
    
    static gboolean handleSetProperty(
        GDBusConnection* connection,
        const gchar* sender,
        const gchar* objectPath,
        const gchar* interfaceName,
        const gchar* propertyName,
        GVariant* value,
        GError** error,
        gpointer userData
    );
    
    // 시그널 핸들러
    static void handleSignal(
        GDBusConnection* connection,
        const gchar* sender,
        const gchar* objectPath,
        const gchar* interfaceName,
        const gchar* signalName,
        GVariant* parameters,
        gpointer userData
    );
};

} // namespace ggk