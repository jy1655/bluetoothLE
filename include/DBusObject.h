// DBusObject.h
#pragma once

#include "DBusTypes.h"
#include "DBusConnection.h"
#include "DBusObjectPath.h"
#include <map>
#include <string>
#include <functional>
#include <mutex>

namespace ggk {

/**
 * @brief D-Bus 객체 기본 클래스
 * 
 * D-Bus 인터페이스, 메서드, 속성을 관리하고 D-Bus 시스템에 객체를 등록하는
 * 기본 클래스입니다. 대부분의 D-Bus 객체(GattService, GattCharacteristic 등)는
 * 이 클래스를 상속합니다.
 */
class DBusObject {
    friend class DBusObjectTest;

public:
    /**
     * @brief 생성자
     * 
     * @param connection D-Bus 연결
     * @param path 객체 경로
     */
    DBusObject(DBusConnection& connection, const DBusObjectPath& path);
    
    /**
     * @brief 소멸자
     * 
     * 등록된 객체를 자동으로 등록 해제합니다.
     */
    virtual ~DBusObject();
    
    /**
     * @brief 인터페이스 추가
     * 
     * @param interface 인터페이스 이름
     * @param properties 속성 목록
     * @return 성공 여부
     */
    bool addInterface(const std::string& interface, const std::vector<DBusProperty>& properties = {});
    
    /**
     * @brief 메서드 추가
     * 
     * @param interface 인터페이스 이름
     * @param method 메서드 이름
     * @param handler 메서드 핸들러
     * @return 성공 여부
     */
    bool addMethod(const std::string& interface, const std::string& method, DBusConnection::MethodHandler handler);
    
    /**
     * @brief 속성 설정
     * 
     * @param interface 인터페이스 이름
     * @param name 속성 이름
     * @param value 설정할 값
     * @return 성공 여부
     */
     bool setProperty(const std::string& interface, const std::string& name, GVariantPtr value);
    
    /**
     * @brief 속성 획득
     * 
     * @param interface 인터페이스 이름
     * @param name 속성 이름
     * @return 속성 값 (없거나 실패 시 null)
     */
     GVariantPtr getProperty(const std::string& interface, const std::string& name) const;
    
    /**
     * @brief 속성 변경 시그널 발생
     * 
     * @param interface 인터페이스 이름
     * @param name 속성 이름
     * @param value 변경된 값
     * @return 성공 여부
     */
     bool emitPropertyChanged(const std::string& interface, const std::string& name, GVariantPtr value);
    
    /**
     * @brief 시그널 발생
     * 
     * @param interface 인터페이스 이름
     * @param name 시그널 이름
     * @param parameters 시그널 매개변수 (기본값: null)
     * @return 성공 여부
     */
     bool emitSignal(const std::string& interface, const std::string& name, GVariantPtr parameters = makeNullGVariantPtr());
    
    /**
     * @brief D-Bus에 객체 등록
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
     * @brief 객체 경로 가져오기
     * 
     * @return 객체 경로
     */
    const DBusObjectPath& getPath() const { return path; }
    
    /**
     * @brief D-Bus 연결 가져오기
     * 
     * @return D-Bus 연결
     */
    DBusConnection& getConnection() const { return connection; }
    
    /**
     * @brief 등록 상태 확인
     * 
     * @return 등록되었는지 여부
     */
    bool isRegistered() const { return registered; }
    
protected:
    /**
     * @brief 인트로스펙션 XML 생성
     * 
     * D-Bus 인트로스펙션을 위한 XML 문서를 생성합니다.
     * 
     * @return 인트로스펙션 XML
     */
    std::string generateIntrospectionXml() const;
    
private:
    DBusConnection& connection;       ///< D-Bus 연결
    DBusObjectPath path;              ///< 객체 경로
    bool registered;                  ///< 등록 상태
    mutable std::mutex mutex;         ///< 동기화 뮤텍스
    
    // 인터페이스 관리
    std::map<std::string, std::vector<DBusProperty>> interfaces;                            ///< 인터페이스 -> 속성 맵
    std::map<std::string, std::map<std::string, DBusConnection::MethodHandler>> methodHandlers;  ///< 인터페이스 -> 메서드 -> 핸들러 맵
};

} // namespace ggk