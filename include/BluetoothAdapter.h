// BluetoothAdapter.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <functional>
#include <memory>

namespace ble {

class BluetoothAdapter {
public:
    BluetoothAdapter(sdbus::IConnection& connection);
    ~BluetoothAdapter() = default;

    // Initialize the Bluetooth adapter
    bool initialize(const std::string& adapterName = "hci0");
    
    // Power on the adapter
    bool powerOn();
    
    // Start LE advertising
    bool startAdvertising();
    
    // Setup appropriate permissions
    bool setupPermissions();
    
    // Get the adapter path
    std::string getAdapterPath() const { return m_adapterPath; }
    
    // Check if the adapter is ready
    bool isReady() const { return m_initialized && m_powered; }
    
private:
    sdbus::IConnection& m_connection;
    std::string m_adapterName;
    std::string m_adapterPath;
    bool m_initialized = false;
    bool m_powered = false;
    std::unique_ptr<sdbus::IProxy> m_adapterProxy;
    
    // Helper method to find the adapter
    bool findAdapter(const std::string& adapterName);
    
    // Helper method to write properties
    bool writeProperty(const std::string& interface, const std::string& property, const sdbus::Variant& value);

    bool removeDefaultUUIDs();
};

} // namespace ble