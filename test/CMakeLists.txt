cmake_minimum_required(VERSION 3.10)
project(obj_manager_test LANGUAGES CXX)

# C++17 사용 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 컴파일 옵션
add_compile_options(-Wall -Wextra -pedantic)

# sdbus-c++ 찾기
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDBUS REQUIRED sdbus-c++)

# sdbus-c++ 경로 추가
include_directories(${SDBUS_INCLUDE_DIRS})
link_directories(${SDBUS_LIBRARY_DIRS})

# 소스 및 헤더
set(SOURCES
    test.cpp
    obj-manager-test.h
)

# 실행 파일 생성
add_executable(obj_manager_test ${SOURCES})

# 라이브러리 링크
target_link_libraries(obj_manager_test ${SDBUS_LIBRARIES})
