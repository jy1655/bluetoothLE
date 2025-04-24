// DataSimulator.cpp
#include "DataSimulator.h"
#include <iostream>
#include <chrono>
#include <random>
#include <ctime>

namespace ble {

DataSimulator::DataSimulator() {
    // Initialize random seed
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

DataSimulator::~DataSimulator() {
    // Ensure all threads are stopped
    stopSimulation();
}

void DataSimulator::startBatterySimulation(std::function<void(uint8_t)> batteryCallback) {
    // Stop any existing simulation
    if (m_batteryThread) {
        m_running = false;
        if (m_batteryThread->joinable()) {
            m_batteryThread->join();
        }
    }
    
    // Start a new simulation
    m_running = true;
    m_batteryThread = std::make_unique<std::thread>(&DataSimulator::batterySimulationWorker, this, batteryCallback);
}

void DataSimulator::startCustomDataSimulation(std::function<void(const std::vector<uint8_t>&)> customDataCallback) {
    // Stop any existing simulation
    if (m_customDataThread) {
        m_running = false;
        if (m_customDataThread->joinable()) {
            m_customDataThread->join();
        }
    }
    
    // Start a new simulation
    m_running = true;
    m_customDataThread = std::make_unique<std::thread>(&DataSimulator::customDataSimulationWorker, this, customDataCallback);
}

void DataSimulator::stopSimulation() {
    m_running = false;
    
    // Join battery thread if running
    if (m_batteryThread && m_batteryThread->joinable()) {
        m_batteryThread->join();
    }
    
    // Join custom data thread if running
    if (m_customDataThread && m_customDataThread->joinable()) {
        m_customDataThread->join();
    }
}

void DataSimulator::batterySimulationWorker(std::function<void(uint8_t)> callback) {
    uint8_t level = 100;
    bool decreasing = true;
    
    while (m_running) {
        // Update battery level every 10 seconds
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        // Change battery level
        if (decreasing) {
            level -= 5;
            if (level <= 20) {
                decreasing = false;
            }
        } else {
            level += 5;
            if (level >= 100) {
                decreasing = true;
            }
        }
        
        // Call the callback with the new level
        callback(level);
    }
}

void DataSimulator::customDataSimulationWorker(std::function<void(const std::vector<uint8_t>&)> callback) {
    // Set up random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);
    
    while (m_running) {
        // Update custom data every 5 seconds
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // Generate random data
        std::vector<uint8_t> randomData;
        for (int i = 0; i < 4; i++) {
            randomData.push_back(static_cast<uint8_t>(distrib(gen)));
        }
        
        // Call the callback with the new data
        callback(randomData);
    }
}

} // namespace ble