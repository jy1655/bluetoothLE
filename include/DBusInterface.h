// DBusInterface.h
#pragma once

#include <gio/gio.h>
#include <string>
#include <map>
#include <memory>
#include <vector>
#include <functional>
#include "DBusTypes.h"
#include "DBusMethod.h"
#include "DBusObjectPath.h"

namespace ggk {

class DBusInterface {
public:
    explicit DBusInterface(const std::string& name);
    virtual ~DBusInterface() = default;

    // 기본 접근자
    virtual const std::string& getName() const { return name; }
    virtual DBusObjectPath getPath() const = 0;

    // 메서드 관리
    void addMethod(std::shared_ptr<DBusMethod> method);
    void removeMethod(const std::string& methodName);
    std::shared_ptr<DBusMethod> findMethod(const std::string& methodName) const;
    bool invokeMethod(const DBusMethodCall& call) const;
    
    // 시그널 관리
    void addSignal(const DBusSignal& signal);
    void removeSignal(const std::string& signalName);
    void emitSignal(const DBusSignalEmission& emission) const;
    
    // 프로퍼티 관리
    void addProperty(const DBusProperty& property);
    void removeProperty(const std::string& propertyName);
    GVariant* getProperty(const std::string& propertyName) const;
    bool setProperty(const std::string& propertyName, 
                    GVariant* value,
                    const std::string& sender);
    
    // 인트로스펙션
    virtual std::string generateIntrospectionXML(
        const DBusIntrospection& config) const;

protected:
    // 이벤트 핸들러
    virtual void onPropertyChanged(const std::string& propertyName,
                                 GVariant* oldValue,
                                 GVariant* newValue) const;
    virtual void onMethodCalled(const DBusMethodCall& call) const;
    virtual void onSignalEmitted(const DBusSignalEmission& emission) const;

    // 유틸리티 메서드
    void notifyPropertyChanged(const std::string& propertyName,
                             GVariant* oldValue,
                             GVariant* newValue) const;
    bool validatePropertyAccess(const std::string& propertyName,
                              const std::string& sender,
                              bool isWrite) const;

private:
    std::string name;
    std::map<std::string, std::shared_ptr<DBusMethod>> methods;
    std::map<std::string, DBusSignal> signals;
    std::map<std::string, DBusProperty> properties;

    // XML 이스케이프 유틸리티
    static std::string escapeXml(const std::string& str);
    // XML 생성 헬퍼
    std::string generateMethodsXML(const DBusIntrospection& config) const;
    std::string generateSignalsXML(const DBusIntrospection& config) const;
    std::string generatePropertiesXML(const DBusIntrospection& config) const;
    

    // 에러 처리
    void handleError(const std::string& errorName,
                    const std::string& errorMessage) const;
};

} // namespace ggk