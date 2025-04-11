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

add_definitions(-DTESTING)

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
    ${PROJECT_INCLUDE_DIR}/Utils.h
    ${PROJECT_INCLUDE_DIR}/Logger.h
    # HCI
    ${PROJECT_INCLUDE_DIR}/HciAdapter.h
    #${PROJECT_INCLUDE_DIR}/Mgmt.h
    # DBus
    ${PROJECT_INCLUDE_DIR}/BlueZUtils.h
    ${PROJECT_INCLUDE_DIR}/BlueZConstants.h
    ${PROJECT_INCLUDE_DIR}/DBusTypes.h
    ${PROJECT_INCLUDE_DIR}/DBusXml.h
    ${PROJECT_INCLUDE_DIR}/DBusError.h
    ${PROJECT_INCLUDE_DIR}/DBusConnection.h
    ${PROJECT_INCLUDE_DIR}/DBusName.h
    ${PROJECT_INCLUDE_DIR}/DBusMessage.h
    ${PROJECT_INCLUDE_DIR}/DBusObject.h
    # GATT
    #${PROJECT_INCLUDE_DIR}/GattTypes.h
    #${PROJECT_INCLUDE_DIR}/GattCallbacks.h
    #${PROJECT_INCLUDE_DIR}/GattService.h
    #${PROJECT_INCLUDE_DIR}/GattCharacteristic.h
    #${PROJECT_INCLUDE_DIR}/GattDescriptor.h
    #${PROJECT_INCLUDE_DIR}/GattApplication.h
    #${PROJECT_INCLUDE_DIR}/GattAdvertisement.h
    # Server
    #${PROJECT_INCLUDE_DIR}/ConnectionManager.h
    # 테스트 환경헤더 (미사용시 주석처리 필요)
    DBusTestEnvironment.h
)

# 테스트할 대상 소스 파일 추가
set(TEST_SOURCES
    ${PROJECT_SRC_DIR}/Utils.cpp    
    ${PROJECT_SRC_DIR}/Logger.cpp
    # HCI
    ${PROJECT_SRC_DIR}/HciAdapter.cpp
    #${PROJECT_SRC_DIR}/Mgmt.cpp
    # DBus
    ${PROJECT_SRC_DIR}/BlueZUtils.cpp
    ${PROJECT_SRC_DIR}/DBusXml.cpp
    ${PROJECT_SRC_DIR}/DBusError.cpp
    ${PROJECT_SRC_DIR}/DBusConnection.cpp
    ${PROJECT_SRC_DIR}/DBusName.cpp
    ${PROJECT_SRC_DIR}/DBusMessage.cpp
    ${PROJECT_SRC_DIR}/DBusObject.cpp
    # GATT
    #${PROJECT_SRC_DIR}/GattTypes.cpp
    #${PROJECT_SRC_DIR}/GattService.cpp
    #${PROJECT_SRC_DIR}/GattCharacteristic.cpp
    #${PROJECT_SRC_DIR}/GattDescriptor.cpp
    #${PROJECT_SRC_DIR}/GattApplication.cpp
    #${PROJECT_SRC_DIR}/GattAdvertisement.cpp
    # Server
    #${PROJECT_SRC_DIR}/ConnectionManager.cpp
)

include_directories(${GIO_INCLUDE_DIRS})

# 테스트 실행 파일 추가
add_executable(run_tests
    ${TEST_SOURCES}
    ${TEST_HEADERS}
    main.cpp
    DBusTestEnvironment.cpp     # 주석처리시 main.cpp 에도 주석처리 작업 필요

    #-- Util Test -- (약 1ms 소요)
    
    DBusObjectPathTest.cpp
    UtilsTest.cpp
    
    #-- HCI Test -- (약 30000ms 소요)
    
    #HciSocketTest.cpp
    #HciAdapterTest.cpp
    #MgmtTest.cpp
    
    #-- DBus Test --
    
    DBusXmlTest.cpp
    DBusErrorTest.cpp
    DBusConnectionTest.cpp
    DBusMessageTest.cpp
    DBusObjectTest.cpp
    
    # GATT Test

    #GattTypesTest.cpp
    #GattServiceTest.cpp        # 헤더파일의 `private:` 주석처리 후 테스트 가능
    #GattCharacteristicTest.cpp # 헤더파일의 `private:` 주석처리 후 테스트 가능
    #GattDescriptorTest.cpp     # 헤더파일의 `private:` 주석처리 후 테스트 가능
    #GattApplicationTest.cpp    # 헤더파일의 `private:` 주석처리 후 테스트 가능
    #GattAdvertisementTest.cpp

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
