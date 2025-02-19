#pragma once

#include <gio/gio.h>
#include <string>
#include <list>
#include <memory>
#include <stdexcept>
#include "DBusInterface.h"
#include "DBusObjectPath.h"

namespace ggk {

class DBusObjectError : public std::runtime_error {
public:
    explicit DBusObjectError(const std::string& what) : std::runtime_error(what) {}
};

class DBusObject {
public:
    using InterfaceList = std::list<std::shared_ptr<DBusInterface>>;
    using ChildList = std::list<DBusObject>;

    enum class State {
        INITIALIZED,
        PUBLISHED,
        ERROR
    };

    explicit DBusObject(const DBusObjectPath& path, bool publish = true);
    DBusObject(DBusObject* pParent, const DBusObjectPath& pathElement);
    virtual ~DBusObject();

    // 복사 및 이동
    DBusObject(const DBusObject&) = delete;
    DBusObject& operator=(const DBusObject&) = delete;
    DBusObject(DBusObject&&) = default;
    DBusObject& operator=(DBusObject&&) = default;

    // 상태 관리
    bool publish(GDBusConnection* connection);
    bool unpublish();
    State getState() const { return state; }
    bool isPublished() const { return state == State::PUBLISHED; }

    // 경로 관리
    const DBusObjectPath& getPathNode() const;
    DBusObjectPath getPath() const;
    DBusObject& getParent();
    const DBusObject& getParent() const;

    // 자식 객체 관리
    const ChildList& getChildren() const;
    DBusObject& addChild(const DBusObjectPath& pathElement);
    DBusObject* findChild(const DBusObjectPath& path);
    void removeChild(const DBusObjectPath& path);

    // 인터페이스 관리
    const InterfaceList& getInterfaces() const;
    
    template<typename T>
    std::shared_ptr<T> addInterface(std::shared_ptr<T> interface) {
        if (!interface) {
            throw DBusObjectError("Null interface pointer");
        }
        auto baseInterface = std::static_pointer_cast<DBusInterface>(interface);
        if (!baseInterface) {
            throw DBusObjectError("Interface must inherit from DBusInterface");
        }
        interfaces.push_back(baseInterface);
        onInterfaceAdded(baseInterface);
        return interface;
    }

    template<typename T>
    std::shared_ptr<T> findInterface(const std::string& interfaceName) const {
        for (const auto& interface : interfaces) {
            if (interface->getName() == interfaceName) {
                return std::dynamic_pointer_cast<T>(interface);
            }
        }
        return nullptr;
    }

    // 인터페이스 검색
    std::shared_ptr<const DBusInterface> findInterface(
        const DBusObjectPath& path,
        const std::string& interfaceName,
        const DBusObjectPath& basePath = DBusObjectPath()) const;

    // 메서드 호출
    bool callMethod(
        const DBusObjectPath& path,
        const std::string& interfaceName,
        const std::string& methodName,
        GDBusConnection* pConnection,
        GVariant* pParameters,
        GDBusMethodInvocation* pInvocation,
        gpointer pUserData,
        const DBusObjectPath& basePath = DBusObjectPath()) const;

    // 시그널 발신
    void emitSignal(
        GDBusConnection* pBusConnection,
        const std::string& interfaceName,
        const std::string& signalName,
        GVariant* pParameters);

    // 인트로스펙션
    std::string generateIntrospectionXML(int depth = 0) const;

protected:
    virtual void onStateChanged(State oldState, State newState);
    virtual void onInterfaceAdded(const std::shared_ptr<DBusInterface>& interface);
    virtual void onChildAdded(DBusObject& child);

private:
    State state;
    DBusObjectPath path;
    InterfaceList interfaces;
    ChildList children;
    DBusObject* pParent;
    GDBusConnection* dbusConnection;

    void setState(State newState);
    bool validatePath() const;
    void cleanup();
};

} // namespace ggk