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
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(BLUEZ REQUIRED bluez)

# Source files
set(SOURCES
    src/HciSocket.cpp
    src/Logger.cpp
    src/Utils.cpp
    src/BLEPeripheralManager.cpp
    src/HciAdapter.cpp      
    src/Mgmt.cpp
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
        bluetooth
        pthread
)

# Installation
install(TARGETS ble_peripheral
    RUNTIME DESTINATION bin
)

# Testing (optional)
option(BUILD_TESTING "Build the testing tree." OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()