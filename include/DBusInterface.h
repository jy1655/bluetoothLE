#pragma once

#include <gio/gio.h>
#include <string>
#include <list>
#include <memory>
#include <vector>
#include "DBusMethod.h"
#include "DBusObjectPath.h"

namespace ggk {

class DBusInterface {
public:
    // 시그널 정의를 위한 구조체
    struct Signal {
        std::string name;
        std::vector<std::string> arguments;
    };

    // 프로퍼티 정의를 위한 구조체
    struct Property {
        std::string name;
        std::string type;
        bool readable;
        bool writable;
        GVariant* (*getter)(void);
        void (*setter)(GVariant*);
    };

    explicit DBusInterface(const std::string& name);
    virtual ~DBusInterface() = default;

    // 인터페이스 이름 관리
    virtual const std::string& getName() const { return name; }
    void setName(const std::string& newName) { name = newName; }
    virtual GVariant* getObjectsManaged() { return nullptr; }

    // D-Bus 객체 경로 관리
    virtual DBusObjectPath getPath() const = 0;  // 순수 가상 함수로 선언

    // 메서드 관리
    void addMethod(const std::shared_ptr<DBusMethod>& method);
    bool callMethod(const std::string& methodName,
                   GDBusConnection* pConnection,
                   GVariant* pParameters,
                   GDBusMethodInvocation* pInvocation,
                   gpointer pUserData) const;

    // 시그널 관리
    void addSignal(const std::string& name, const std::vector<std::string>& arguments);
    void emitSignal(GDBusConnection* pConnection,
                   const DBusObjectPath& path,
                   const std::string& signalName,
                   GVariant* pParameters) const;

    // 프로퍼티 관리
    void addProperty(const std::string& name,
                    const std::string& type,
                    bool readable,
                    bool writable,
                    GVariant* (*getter)(void) = nullptr,
                    void (*setter)(GVariant*) = nullptr);

    // 인트로스펙션 지원
    virtual std::string generateIntrospectionXML(int depth = 0) const;

protected:
    std::string name;
    // XML 생성 헬퍼 함수
    std::string generateMethodXML(const DBusMethod& method, int depth) const;
    std::string generateSignalXML(const Signal& signal, int depth) const;
    std::string generatePropertyXML(const Property& property, int depth) const;

private:
    std::list<std::shared_ptr<DBusMethod>> methods;
    std::list<Signal> signals;
    std::list<Property> properties;
};

} // namespace ggk