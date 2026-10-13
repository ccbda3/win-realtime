cmake_minimum_required(VERSION 3.20)
project(gps-ui-qt VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_PREFIX_PATH "C:/Qt/6.9.3/msvc2022_64")

set(VERSION_TYPE "quanneng" CACHE STRING "构建版本: quanneng(全能版) 或 gaoduan(高端版)")
if(VERSION_TYPE STREQUAL "gaoduan")
    set(PORTABLE_DIR "C:/Users/Whaoy/Documents/win-realtime/gaoduan/gps-ui-qt_portable")
else()
    set(PORTABLE_DIR "C:/Users/Whaoy/Documents/win-realtime/gps-ui-qt/gps-ui-qt_portable")
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PORTABLE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PORTABLE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PORTABLE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${PORTABLE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PORTABLE_DIR}")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network WebEngineWidgets Concurrent)

set(UHD_DIR "C:/Program Files/UHD")
set(UHD_INCLUDE_DIR "${UHD_DIR}/include")
set(UHD_LIBRARY "${UHD_DIR}/lib/uhd.lib")

set(GPS_SOURCES
    gps-ui-qt/src/main.cpp
    gps-ui-qt/src/mainwindow.cpp
    gps-ui-qt/src/mainwindow.h
    gps-ui-qt/src/mapwindow.cpp
    gps-ui-qt/src/mapwindow.h
    gps-ui-qt/src/core/rf_core.c
    gps-ui-qt/src/rsa_verify.c)

qt_add_executable(gps-ui-qt WIN32 ${GPS_SOURCES} gps-ui-qt/appicon.rc)
target_include_directories(gps-ui-qt PRIVATE gps-ui-qt/src ${UHD_INCLUDE_DIR})
target_link_libraries(gps-ui-qt PRIVATE
    Qt6::Core Qt6::Widgets Qt6::Network Qt6::WebEngineWidgets Qt6::Concurrent
    ${UHD_LIBRARY} ws2_32 bcrypt ncrypt winmm)
target_compile_definitions(gps-ui-qt PRIVATE HAS_UHD)
if(VERSION_TYPE STREQUAL "gaoduan")
    target_compile_definitions(gps-ui-qt PRIVATE GPS_VERSION_GAODUAN)
endif()

add_executable(gps-core gps-ui-qt/src/core/gpssim.c gps-ui-qt/src/getopt.c gps-ui-qt/src/core/rf_core.c gps-ui-qt/src/rsa_verify.c)
target_include_directories(gps-core PRIVATE gps-ui-qt/src ${UHD_INCLUDE_DIR})
target_link_libraries(gps-core PRIVATE Qt6::Core Qt6::Network ${UHD_LIBRARY} ws2_32 bcrypt ncrypt)
target_compile_definitions(gps-core PRIVATE HAS_UHD)

add_custom_command(TARGET gps-ui-qt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/gps-ui-qt/static"
        "$<TARGET_FILE_DIR:gps-ui-qt>/static"
    COMMENT "复制静态前端文件")
