# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(ble_peripheral VERSION 1.0)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 필수 패키지 찾기
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDBUS REQUIRED sdbus-c++ IMPORTED_TARGET)

# 헤더 파일 목록
set(HEADERS
    include/GattApplication.h
    include/GattService.h
    include/GattCharacteristic.h
    include/GattDescriptor.h
    include/GattAdvertisement.h
    include/GattTypes.h
    include/xml/GattService1.h
    include/xml/GattCharacteristic1.h
    include/xml/GattDescriptor1.h
    include/xml/LEAdvertisement1.h
)

# 소스 파일 목록
set(SOURCES
    src/main.cpp
    src/GattApplication.cpp
    src/GattService.cpp
    src/GattCharacteristic.cpp
    src/GattDescriptor.cpp
    src/GattAdvertisement.cpp
)

# IDE에서 헤더 파일 그룹화 (선택사항)
source_group("Header Files" FILES ${HEADERS})
source_group("Source Files" FILES ${SOURCES})

# 실행 파일 생성 (헤더 파일 포함)
add_executable(ble_peripheral ${SOURCES} ${HEADERS})

# 헤더 디렉토리 설정
target_include_directories(ble_peripheral PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 라이브러리 링크
target_link_libraries(ble_peripheral PRIVATE
    PkgConfig::SDBUS
    pthread
)

# 인스톨 타겟 설정
install(TARGETS ble_peripheral DESTINATION bin)

# 헤더 파일을 인스톨할 경우 (선택사항)
# install(FILES ${HEADERS} DESTINATION include/ble_peripheral)