#pragma once

#include <gio/gio.h>
#include <string>
#include <vector>
#include "DBusObjectPath.h"
#include "Logger.h"

namespace ggk {

struct DBusInterface;

class DBusMethod {
public:
    // 메서드 콜백 delegate
    using Callback = void (*)(const DBusInterface& self,
                            GDBusConnection* pConnection,
                            const std::string& methodName,
                            GVariant* pParameters,
                            GDBusMethodInvocation* pInvocation,
                            void* pUserData);

    // 비동기 콜백 지원
    using AsyncCallback = void (*)(GObject* source,
                                 GAsyncResult* result,
                                 gpointer user_data);

    // 생성자
    DBusMethod(const DBusInterface* pOwner,
               const std::string& name,
               const char* pInArgs[],
               const char* pOutArgs,
               Callback callback);

    // 소멸자
    ~DBusMethod() = default;

    // 복사 생성자와 대입 연산자 비활성화
    DBusMethod(const DBusMethod&) = delete;
    DBusMethod& operator=(const DBusMethod&) = delete;

    // 이동 생성자와 대입 연산자는 허용
    DBusMethod(DBusMethod&&) = default;
    DBusMethod& operator=(DBusMethod&&) = default;

    // Accessors
    const std::string& getName() const { return name; }
    const std::vector<std::string>& getInArgs() const { return inArgs; }
    const std::string& getOutArgs() const { return outArgs; }

    // 메서드 호출
    template<typename T>
    void call(GDBusConnection* pConnection,
              const DBusObjectPath& path,
              const std::string& interfaceName,
              const std::string& methodName,
              GVariant* pParameters,
              GDBusMethodInvocation* pInvocation,
              void* pUserData) const {
        if (!callback) {
            Logger::error("DBusMethod contains no callback: [" + path.toString() + 
                         "]:[" + interfaceName + "]:[" + methodName + "]");
            g_dbus_method_invocation_return_dbus_error(
                pInvocation,
                "org.bluez.Error.NotImplemented",
                "This method is not implemented");
            return;
        }

        Logger::debug("Calling method: [" + path.toString() + 
                     "]:[" + interfaceName + "]:[" + methodName + "]");
        
        callback(*static_cast<const T*>(pOwner),
                pConnection,
                methodName,
                pParameters,
                pInvocation,
                pUserData);
    }

    // 비동기 호출
    void callAsync(GDBusConnection* pConnection,
                  const DBusObjectPath& path,
                  const std::string& interfaceName,
                  GVariant* pParameters,
                  AsyncCallback callback,
                  gpointer user_data) const;

    // 인트로스펙션 XML 생성
    std::string generateIntrospectionXML(int depth) const;

private:
    const DBusInterface* pOwner;
    std::string name;
    std::vector<std::string> inArgs;
    std::string outArgs;
    Callback callback;

    // 로깅 헬퍼
    void logMethodCall(const std::string& methodName,
                      GVariant* parameters) const;
};

} // namespace ggk