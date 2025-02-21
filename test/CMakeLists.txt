cmake_minimum_required(VERSION 3.10)
project(BLE_Test)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Google Test 라이브러리 추가
enable_testing()
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Find required system packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(BLUEZ REQUIRED bluez)

# 프로젝트 소스 코드 포함
set(PROJECT_SRC_DIR ${CMAKE_SOURCE_DIR}/../src)
set(PROJECT_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/../include)  # 추가

include_directories(${PROJECT_SRC_DIR})
include_directories(${PROJECT_INCLUDE_DIR})  # include 폴더 추가

# 특정 헤더 파일만 추가
set(TEST_HEADERS
    ${PROJECT_INCLUDE_DIR}/DBusObjectPath.h
    ${PROJECT_INCLUDE_DIR}/HciAdapter.h
    ${PROJECT_INCLUDE_DIR}/HciSocket.h
    #${PROJECT_INCLUDE_DIR}/GattApplication.h
    #${PROJECT_INCLUDE_DIR}/GattService.h
    #${PROJECT_INCLUDE_DIR}/GattCharacteristic.h
    #${PROJECT_INCLUDE_DIR}/GattDescriptor.h
    #${PROJECT_INCLUDE_DIR}/GattObject.h
    ${PROJECT_INCLUDE_DIR}/Mgmt.h
    ${PROJECT_INCLUDE_DIR}/Utils.h
)

# 테스트할 대상 소스 파일 추가
set(TEST_SOURCES
    #${PROJECT_SRC_DIR}/DBusObject.cpp
    #${PROJECT_SRC_DIR}/DBusInterface.cpp
    #${PROJECT_SRC_DIR}/GattApplication.cpp
    ${PROJECT_SRC_DIR}/HciAdapter.cpp
    ${PROJECT_SRC_DIR}/HciSocket.cpp
    ${PROJECT_SRC_DIR}/Logger.cpp
    #${PROJECT_SRC_DIR}/GattDescriptor.cpp
    #${PROJECT_SRC_DIR}/GattObject.cpp
    ${PROJECT_SRC_DIR}/Mgmt.cpp
    ${PROJECT_SRC_DIR}/Utils.cpp
)

include_directories(${GIO_INCLUDE_DIRS})

# 테스트 실행 파일 추가
add_executable(run_tests
    ${TEST_SOURCES}
    ${TEST_HEADERS}
    main.cpp
    # HCI Test
    DBusObjectPathTest.cpp
    HciAdapterTest.cpp
    HciSocketTest.cpp
    MgmtTest.cpp
    UtilsTest.cpp
    # DBus Test

    # GATT Test

    # Server Test
)

# 라이브러리 연결
target_link_libraries(run_tests
    PRIVATE
        GTest::GTest
        GTest::Main
        pthread
        ${GLIB_LIBRARIES}
        ${GIO_LIBRARIES}
        ${BLUEZ_LIBRARIES}
        bluetooth
)
