#include "DBusName.h"
#include "Utils.h"  // Utils 유틸리티 함수 사용을 위해 추가

namespace ggk {

DBusName& DBusName::getInstance() {
    static DBusName instance;
    return instance;
}

DBusName::DBusName() 
    : busType(G_BUS_TYPE_SYSTEM), // 실제 하드웨어는 시스템 버스 사용
      initialized(false), 
      busNameAcquired(false) {
    // 연결 객체는 필요할 때 초기화
    connection = std::make_unique<DBusConnection>(busType);
}

DBusName::~DBusName() {
    shutdown();
}

#ifdef TESTING
void DBusName::setBusType(GBusType type) {
    if (initialized) {
        // 이미 초기화된 경우 먼저 연결 해제
        shutdown();
    }
    
    busType = type;
    connection = std::make_unique<DBusConnection>(type);
}
#endif

bool DBusName::initialize(const std::string& name) {
    if (initialized) {
        return busNameAcquired;
    }
    
    busName = name;
    
    if (!connection || !connection->connect()) {
        Logger::error("Failed to connect to D-Bus");
        return false;
    }
    
    // 버스 네임 요청
    try {
        // 복잡한 GVariant 생성을 위한 안전한 방법
        GVariant* paramsRaw = g_variant_new("(su)", busName.c_str(), 0x4);
        // makeGVariantPtr 패턴 사용
        GVariantPtr params = makeGVariantPtr(paramsRaw, true);  // true = 소유권 이전
        
        GVariantPtr result = connection->callMethod(
            "org.freedesktop.DBus",
            DBusObjectPath("/org/freedesktop/DBus"),
            "org.freedesktop.DBus",
            "RequestName",
            std::move(params),  // 이동 의미론 사용하여 소유권 이전
            "(u)"
        );
        
        if (result) {
            guint32 ret = 0;
            g_variant_get(result.get(), "(u)", &ret);
            
            if (ret == 1) {  // DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER
                Logger::info("Successfully acquired bus name: " + busName);
                busNameAcquired = true;
            } else {
                Logger::error("Failed to acquire bus name: " + busName + ", return code: " + std::to_string(ret));
                busNameAcquired = false;
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Failed to request bus name: " + std::string(e.what()));
        busNameAcquired = false;
    }
    
    initialized = connection && connection->isConnected();
    return busNameAcquired;
}

void DBusName::shutdown() {
    if (busNameAcquired && connection) {
        try {
            // 빌더 사용해서 GVariant 생성
            GVariantBuilder builder;
            g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);
            g_variant_builder_add(&builder, "s", busName.c_str());
            
            // 빌더로부터 GVariant 생성
            GVariant* params = g_variant_builder_end(&builder);
            
            // makeGVariantPtr 패턴 사용
            GVariantPtr params_ptr = makeGVariantPtr(params, true);
            
            connection->callMethod(
                "org.freedesktop.DBus",
                DBusObjectPath("/org/freedesktop/DBus"),
                "org.freedesktop.DBus",
                "ReleaseName",
                std::move(params_ptr)  // 소유권 이전
            );
        } catch (...) {
            // 종료 과정에서 예외가 발생해도 무시
        }
        busNameAcquired = false;
    }
    
    if (connection) {
        connection->disconnect();
    }
    initialized = false;
}

DBusConnection& DBusName::getConnection() {
    if (!connection) {
        // 연결이 없으면 생성
        connection = std::make_unique<DBusConnection>(busType);
    }
    return *connection;
}

} // namespace ggk