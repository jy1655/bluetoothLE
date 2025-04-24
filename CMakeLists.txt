cmake_minimum_required(VERSION 3.10)
project(ble_peripheral VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find sdbus-c++ package
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDBUS REQUIRED sdbus-c++ IMPORTED_TARGET)

# Define source files
set(SOURCES
    src/main.cpp
    src/BleApplication.cpp
    src/GattService.cpp
    src/GattCharacteristic.cpp
    src/GattDescriptor.cpp
    src/BleAdvertisement.cpp
    src/BluetoothAdapter.cpp
    src/DataSimulator.cpp
)

# Define header files
set(HEADERS
    include/BleApplication.h
    include/GattService.h
    include/GattCharacteristic.h
    include/GattDescriptor.h
    include/BleAdvertisement.h
    include/BleConstants.h
    include/BluetoothAdapter.h
    include/DataSimulator.h
    include/xml/GattService1.h
    include/xml/GattCharacteristic1.h
    include/xml/GattDescriptor1.h
    include/xml/LEAdvertisement1.h
)

# Group files for IDE
source_group("Source Files" FILES ${SOURCES})
source_group("Header Files" FILES ${HEADERS})

# Create executable
add_executable(ble_peripheral ${SOURCES} ${HEADERS})

# Add include directories
target_include_directories(ble_peripheral PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(ble_peripheral PRIVATE
    PkgConfig::SDBUS
    pthread
)


# Optional: Enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ble_peripheral PRIVATE -Wall -Wextra -pedantic)
endif()