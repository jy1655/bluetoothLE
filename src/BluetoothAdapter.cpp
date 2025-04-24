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
    // Create a BlueZ configuration file with permissive settings
    const char* config = 
        "[General]\n"
        "ControllerMode = dual\n"
        "Privacy = off\n"
        "JustWorksRepairing = always\n"
        "Security = low\n\n"
        "[Policy]\n"
        "AutoEnable = true\n"
        "Pairable = true\n"
        "PairableTimeout = 0\n"
        "MinimumEncryptionKeySize = 1\n";
    
    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "Fork failed" << std::endl;
        return false;
    }
    
    if (pid == 0) {
        // Child process
        // Write the configuration to /etc/bluetooth/main.conf
        execl("/bin/sh", "sh", "-c", 
              std::string("echo '").append(config).append("' | sudo tee /etc/bluetooth/main.conf > /dev/null").c_str(),
              nullptr);
        
        // If execl returns, it failed
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            std::cout << "Bluetooth configuration file created successfully" << std::endl;
            
            // Now restart the bluetooth service
            pid_t restart_pid = fork();
            if (restart_pid == -1) {
                std::cerr << "Fork failed for restart" << std::endl;
                return false;
            }
            
            if (restart_pid == 0) {
                // Child process
                execl("/bin/sh", "sh", "-c", "sudo systemctl restart bluetooth", nullptr);
                
                // If execl returns, it failed
                exit(EXIT_FAILURE);
            } else {
                // Parent process
                int restart_status;
                waitpid(restart_pid, &restart_status, 0);
                
                if (WIFEXITED(restart_status) && WEXITSTATUS(restart_status) == 0) {
                    std::cout << "Bluetooth service restarted successfully" << std::endl;
                    // Give time for the bluetooth service to fully restart
                    sleep(2);
                    return true;
                } else {
                    std::cerr << "Failed to restart bluetooth service" << std::endl;
                    return false;
                }
            }
        } else {
            std::cerr << "Failed to create bluetooth configuration file" << std::endl;
            return false;
        }
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

} // namespace ble