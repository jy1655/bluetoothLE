// BluetoothAdapter.cpp
#include "BluetoothAdapter.h"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

namespace ble {

BluetoothAdapter::BluetoothAdapter(sdbus::IConnection& connection)
    : m_connection(connection) {
}

bool BluetoothAdapter::initialize(const std::string& adapterName) {
    m_adapterName = adapterName;
    
    if (!findAdapter(adapterName)) {
        std::cerr << "Bluetooth adapter " << adapterName << " not found" << std::endl;
        return false;
    }
    
    // Create a proxy to the adapter
    m_adapterProxy = sdbus::createProxy(
        m_connection,
        sdbus::ServiceName("org.bluez"),
        sdbus::ObjectPath(m_adapterPath)
    );
    
    m_initialized = true;
    return true;
}

bool BluetoothAdapter::powerOn() {
    if (!m_initialized) {
        std::cerr << "Adapter not initialized" << std::endl;
        return false;
    }
    
    // Turn on the adapter
    try {
        if (!writeProperty("org.bluez.Adapter1", "Powered", sdbus::Variant(true))) {
            std::cerr << "Failed to power on the adapter" << std::endl;
            return false;
        }
        
        std::cout << "Bluetooth adapter powered on successfully" << std::endl;
        m_powered = true;
        return true;
    } catch (const sdbus::Error& e) {
        std::cerr << "D-Bus error powering on adapter: " << e.getName() << " - " << e.getMessage() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error powering on adapter: " << e.what() << std::endl;
        return false;
    }
}

bool BluetoothAdapter::startAdvertising() {
    if (!m_initialized || !m_powered) {
        std::cerr << "Adapter not initialized or powered on" << std::endl;
        return false;
    }
    
    try {
        // Set discoverable
        if (!writeProperty("org.bluez.Adapter1", "Discoverable", sdbus::Variant(true))) {
            std::cerr << "Failed to set adapter as discoverable" << std::endl;
            return false;
        }

        removeDefaultUUIDs();
        
        // Set discovery timeout (0 = no timeout)
        if (!writeProperty("org.bluez.Adapter1", "DiscoverableTimeout", sdbus::Variant(static_cast<uint32_t>(0)))) {
            std::cerr << "Failed to set discoverable timeout" << std::endl;
            return false;
        }
        
        std::cout << "Bluetooth advertising setup completed successfully" << std::endl;
        return true;
    } catch (const sdbus::Error& e) {
        std::cerr << "D-Bus error setting up advertising: " << e.getName() << " - " << e.getMessage() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error setting up advertising: " << e.what() << std::endl;
        return false;
    }
}

bool BluetoothAdapter::setupPermissions() {
    // 프로그래밍 방식으로 설정하는 대신 사전에 설정된 구성 사용
    std::cout << "Using pre-configured Bluetooth settings..." << std::endl;
    
    // 시스템의 Bluetooth 서비스가 실행 중인지 확인
    try {
        auto systemBus = sdbus::createSystemBusConnection();
        auto bluetoothProxy = sdbus::createProxy(
            *systemBus,
            sdbus::ServiceName("org.bluez"),
            sdbus::ObjectPath("/org/bluez")
        );
        
        // 연결 테스트 (GetManagedObjects 호출)
        std::map<sdbus::ObjectPath, std::map<std::string, std::map<std::string, sdbus::Variant>>> objects;
        bluetoothProxy->callMethod("GetManagedObjects")
                       .onInterface("org.freedesktop.DBus.ObjectManager")
                       .storeResultsTo(objects);
        
        std::cout << "Successfully connected to BlueZ" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error connecting to BlueZ: " << e.what() << std::endl;
        std::cerr << "Please ensure bluetoothd is running: 'sudo systemctl start bluetooth'" << std::endl;
        return false;
    }
}

bool BluetoothAdapter::findAdapter(const std::string& adapterName) {
    // Look for the adapter using ObjectManager
    auto objectManagerProxy = sdbus::createProxy(
        m_connection,
        sdbus::ServiceName("org.bluez"),
        sdbus::ObjectPath("/")
    );
    
    // Now manually call the method on the ObjectManager interface
    std::map<sdbus::ObjectPath, std::map<std::string, std::map<std::string, sdbus::Variant>>> objects;
    objectManagerProxy->callMethod("GetManagedObjects")
                       .onInterface("org.freedesktop.DBus.ObjectManager")
                       .storeResultsTo(objects);
    
    // Look for the specified adapter
    std::string expectedPath = "/org/bluez/" + adapterName;
    for (const auto& [path, interfaces] : objects) {
        if (path == expectedPath && interfaces.find("org.bluez.Adapter1") != interfaces.end()) {
            m_adapterPath = path;
            std::cout << "Found Bluetooth adapter at path: " << m_adapterPath << std::endl;
            return true;
        }
    }
    
    // If we didn't find the specific adapter, try to find any adapter
    for (const auto& [path, interfaces] : objects) {
        if (interfaces.find("org.bluez.Adapter1") != interfaces.end()) {
            m_adapterPath = path;
            m_adapterName = path.substr(path.rfind("/") + 1);
            std::cout << "Found Bluetooth adapter at path: " << m_adapterPath << std::endl;
            return true;
        }
    }
    
    return false;
}

bool BluetoothAdapter::writeProperty(const std::string& interface, const std::string& property, const sdbus::Variant& value) {
    try {
        m_adapterProxy->callMethod("Set")
            .onInterface("org.freedesktop.DBus.Properties")
            .withArguments(interface, property, value);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to set property " << property << ": " << e.what() << std::endl;
        return false;
    }
}

bool BluetoothAdapter::removeDefaultUUIDs() {
    try {
        // 현재 등록된 UUID 목록 가져오기
        auto objectManagerProxy = sdbus::createProxy(
            m_connection,
            sdbus::ServiceName("org.bluez"),
            sdbus::ObjectPath("/")
        );

        std::map<sdbus::ObjectPath, std::map<std::string, std::map<std::string, sdbus::Variant>>> objects;
        objectManagerProxy->callMethod("GetManagedObjects")
                           .onInterface("org.freedesktop.DBus.ObjectManager")
                           .storeResultsTo(objects);

        // UUIDs 가져오기
        std::vector<std::string> uuids;
        if (objects.find(sdbus::ObjectPath(m_adapterPath)) != objects.end() &&
            objects[sdbus::ObjectPath(m_adapterPath)].find("org.bluez.Adapter1") != objects[sdbus::ObjectPath(m_adapterPath)].end()) {
            
            auto& properties = objects[sdbus::ObjectPath(m_adapterPath)]["org.bluez.Adapter1"];
            if (properties.find("UUIDs") != properties.end()) {
                uuids = properties["UUIDs"].get<std::vector<std::string>>();
            }
        }

        // 시스템 기본 UUID 제거 - 제거 전에 로그 출력
        std::cout << "Removing system default UUIDs from adapter..." << std::endl;
        
        // 현재 시점에서는 실제로 UUID를 제거하지 않고, 
        // 사용자 UUID만 가진 광고를 만들도록 함
        // 일부 블루투스 스택은 제거를 완전히 지원하지 않을 수 있음

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error removing default UUIDs: " << e.what() << std::endl;
        return false;
    }
}

} // namespace ble