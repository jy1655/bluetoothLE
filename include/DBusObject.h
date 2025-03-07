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

class DBusObject {
    friend class DBusObjectTest;

public:
    // 생성자
    DBusObject(DBusConnection& connection, const DBusObjectPath& path);
    virtual ~DBusObject();
    
    // 인터페이스 및 메서드 등록
    bool addInterface(const std::string& interface, const std::vector<DBusProperty>& properties = {});
    bool addMethod(const std::string& interface, const std::string& method, DBusConnection::MethodHandler handler);
    
    // 속성 관련
    bool setProperty(const std::string& interface, const std::string& name, GVariantPtr value);
    GVariantPtr getProperty(const std::string& interface, const std::string& name) const;
    bool emitPropertyChanged(const std::string& interface, const std::string& name, GVariantPtr value);
    
    // 시그널 관련
    bool emitSignal(const std::string& interface, const std::string& name, GVariantPtr parameters = makeNullGVariantPtr());
    
    // 등록/해제
    bool registerObject();
    bool unregisterObject();
    
    // 접근자
    const DBusObjectPath& getPath() const { return path; }
    DBusConnection& getConnection() const { return connection; }
    bool isRegistered() const { return registered; }
    
protected:
    // 인트로스펙션 XML 생성
    std::string generateIntrospectionXml() const;
    
private:
    DBusConnection& connection;
    DBusObjectPath path;
    bool registered;
    mutable std::mutex mutex;
    
    // 인터페이스 관리
    std::map<std::string, std::vector<DBusProperty>> interfaces;
    std::map<std::string, std::map<std::string, DBusConnection::MethodHandler>> methodHandlers;
};

} // namespace ggk

