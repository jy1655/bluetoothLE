// src/DBusName.cpp
#include "DBusName.h"

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

void DBusName::setBusType(GBusType type) {
    if (initialized) {
        // 이미 초기화된 경우 먼저 연결 해제
        shutdown();
    }
    
    busType = type;
    connection = std::make_unique<DBusConnection>(type);
}

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
        GVariantPtr result = connection->callMethod(
            "org.freedesktop.DBus",
            DBusObjectPath("/org/freedesktop/DBus"),
            "org.freedesktop.DBus",
            "RequestName",
            GVariantPtr(g_variant_new("(su)", busName.c_str(), 0x4), &g_variant_unref),
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
            connection->callMethod(
                "org.freedesktop.DBus",
                DBusObjectPath("/org/freedesktop/DBus"),
                "org.freedesktop.DBus",
                "ReleaseName",
                GVariantPtr(g_variant_new("(s)", busName.c_str()), &g_variant_unref)
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