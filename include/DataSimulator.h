// DataSimulator.h
#pragma once

#include <thread>
#include <memory>
#include <atomic>
#include <functional>
#include <vector>
#include <cstdint>

namespace ble {

class DataSimulator {
public:
    DataSimulator();
    ~DataSimulator();

    // Start simulation with callback for battery updates
    void startBatterySimulation(std::function<void(uint8_t)> batteryCallback);
    
    // Start simulation with callback for custom data updates
    void startCustomDataSimulation(std::function<void(const std::vector<uint8_t>&)> customDataCallback);
    
    // Stop all simulations
    void stopSimulation();

private:
    std::atomic<bool> m_running{false};
    std::unique_ptr<std::thread> m_batteryThread;
    std::unique_ptr<std::thread> m_customDataThread;
    
    // Battery simulation worker
    void batterySimulationWorker(std::function<void(uint8_t)> callback);
    
    // Custom data simulation worker
    void customDataSimulationWorker(std::function<void(const std::vector<uint8_t>&)> callback);
};

} // namespace ble