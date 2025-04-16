cmake_minimum_required(VERSION 3.10)
project(ble_peripheral VERSION 1.0)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(sdbus-c++ REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(BLUEZ REQUIRED bluez)

# Source files
set(SOURCES
    # 기본 클래스 및 인터페이스
    src/Logger.cpp
    src/Utils.cpp
    
    # BlueZ 유틸리티
    src/BlueZUtils.cpp
    
    # D-Bus 관련 클래스
    src/SDBusInterface.cpp
    src/SDBusConnection.cpp
    src/SDBusObject.cpp
    src/SDBusProxy.cpp
    src/SDBusError.cpp
    src/DBusName.cpp
    
    # GATT 클래스
    src/GattTypes.cpp
    src/GattApplication.cpp
    src/GattService.cpp
    src/GattCharacteristic.cpp
    src/GattDescriptor.cpp
    src/GattAdvertisement.cpp
    
    # 연결 및 HCI 관리
    src/ConnectionManager.cpp
    src/HciAdapter.cpp
    
    # 서버 클래스
    src/Server.cpp
)

# Create executable
add_executable(ble_peripheral 
    ${SOURCES}
    src/main.cpp
)

# Include directories
target_include_directories(ble_peripheral
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${GLIB_INCLUDE_DIRS}
        ${GIO_INCLUDE_DIRS}
        ${BLUEZ_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(ble_peripheral
    PRIVATE
        ${GLIB_LIBRARIES}
        ${GIO_LIBRARIES}
        ${BLUEZ_LIBRARIES}
        SDBusCpp::sdbus-c++
        bluetooth
        pthread
)