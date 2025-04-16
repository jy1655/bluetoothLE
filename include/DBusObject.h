#pragma once

#include "DBusTypes.h"
#include "DBusInterface.h"
#include "DBusObjectPath.h"
#include <map>
#include <string>
#include <functional>
#include <mutex>
#include <vector>
#include <memory>

namespace ggk {

/**
 * @brief Base class for D-Bus objects
 * 
 * D-Bus 객체 등록, 인터페이스, 메서드, 속성을 관리하는 기본 클래스입니다.
 */
class DBusObject {
public:
    /**
     * @brief 생성자
     * 
     * @param connection D-Bus 연결
     * @param path 객체 경로
     */
    DBusObject(std::shared_ptr<IDBusConnection> connection, const DBusObjectPath& path);
    
    /**
     * @brief 소멸자
     * 
     * 자동으로 객체를 등록 해제합니다.
     */
    virtual ~DBusObject();
    
    /**
     * @brief 인터페이스 추가
     * 
     * @param interface 인터페이스 이름
     * @param properties 인터페이스 속성 목록
     * @return 성공 여부
     */
    bool addInterface(const std::string& interface, 
                     const std::vector<DBusProperty>& properties = {});
    
    /**
     * @brief 인터페이스에 메서드 추가
     * 
     * @param interface 인터페이스 이름
     * @param method 메서드 이름
     * @param handler 메서드 핸들러 함수
     * @return 성공 여부
     */
    bool addMethod(const std::string& interface, 
                  const std::string& method, 
                  IDBusConnection::MethodHandler handler);
    
    /**
     * @brief 시그니처가 있는 메서드 추가
     * 
     * @param interface 인터페이스 이름
     * @param method 메서드 이름
     * @param handler 메서드 핸들러 함수
     * @param inSignature 입력 시그니처
     * @param outSignature 출력 시그니처
     * @return 성공 여부
     */
    bool addMethodWithSignature(
        const std::string& interface, 
        const std::string& method, 
        IDBusConnection::MethodHandler handler,
        const std::string& inSignature = "", 
        const std::string& outSignature = "");
    
    /**
     * @brief 속성 변경 시그널 발생
     * 
     * @param interface 인터페이스 이름
     * @param name 속성 이름
     * @param value 새 값
     * @return 성공 여부
     */
    bool emitPropertyChanged(
        const std::string& interface, 
        const std::string& name, 
        GVariantPtr value);
    
    /**
     * @brief 시그널 발생
     * 
     * @param interface 인터페이스 이름
     * @param name 시그널 이름
     * @param parameters 시그널 매개변수 (nullptr = 매개변수 없음)
     * @return 성공 여부
     */
    bool emitSignal(
        const std::string& interface, 
        const std::string& name, 
        GVariantPtr parameters = makeNullGVariantPtr());
    
    /**
     * @brief D-Bus에 객체 등록
     * 
     * 모든 인터페이스와 메서드 추가를 완료한 후 호출합니다.
     * 
     * @return 성공 여부
     */
    bool registerObject();
    
    /**
     * @brief D-Bus에서 객체 등록 해제
     * 
     * @return 성공 여부
     */
    bool unregisterObject();

    /**
     * @brief 등록 완료 처리
     * 
     * 모든 인터페이스와 메서드 추가를 완료하고 객체를 등록합니다.
     * 이 메서드 호출 이후에는 더 이상 인터페이스나 메서드를 추가할 수 없습니다.
     * 
     * @return 성공 여부
     */
    bool finishRegistration();
    
    /**
     * @brief 객체 경로 획득
     * 
     * @return 객체 경로
     */
    const DBusObjectPath& getPath() const { return path; }
    
    /**
     * @brief D-Bus 연결 획득
     * 
     * @return D-Bus 연결
     */
    std::shared_ptr<IDBusConnection> getConnection() const { return connection; }
    
    /**
     * @brief 등록 상태 확인
     * 
     * @return D-Bus에 등록되었는지 여부
     */
    bool isRegistered() const { return registered; }
    
    /**
     * @brief 등록 가능 상태 확인
     * 
     * @return 인터페이스와 메서드를 추가할 수 있는지 여부
     */
    bool canModify() const { return !registered && !registrationFinished; }
    
    /**
     * @brief Introspect 메서드 호출 처리
     * 
     * @param call 메서드 호출 정보
     */
    void handleIntrospect(const DBusMethodCall& call);
    
protected:
    /**
     * @brief 인트로스펙션 XML 생성
     * 
     * @return XML 문자열
     */
    std::string generateIntrospectionXml() const;
    
    /**
     * @brief 객체의 인터페이스 확인
     * 
     * @param interface 인터페이스 이름
     * @return 인터페이스를 가지고 있는지 여부
     */
    bool hasInterface(const std::string& interface) const;
    
    /**
     * @brief 표준 인터페이스 추가
     * 
     * Introspectable 및 Properties 인터페이스를 추가합니다.
     */
    void addStandardInterfaces();

private:
    std::shared_ptr<IDBusConnection> connection;  ///< D-Bus 연결
    DBusObjectPath path;                         ///< 객체 경로
    bool registered;                             ///< 등록 상태
    bool registrationFinished;                   ///< 등록 완료 상태
    mutable std::mutex mutex;                    ///< 동기화 뮤텍스
    
    // 인터페이스 관리
    std::map<std::string, std::vector<DBusProperty>> interfaces;  ///< 인터페이스 속성
    std::map<std::string, std::map<std::string, IDBusConnection::MethodHandler>> methodHandlers;  ///< 메서드 핸들러
    std::map<std::string, std::map<std::string, std::pair<std::string, std::string>>> methodSignatures;  ///< 메서드 시그니처
};

} // namespace ggk