cmake_minimum_required(VERSION 3.20)
project(gps-ui-qt VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_AUTOMOC ON)

# Qt
set(CMAKE_PREFIX_PATH "C:/Qt/6.9.3/msvc2022_64")
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network WebEngineWidgets Concurrent)

# UHD
set(UHD_DIR "C:/Program Files/UHD")
set(UHD_INCLUDE_DIR "${UHD_DIR}/include")
set(UHD_LIBRARY "${UHD_DIR}/lib/uhd.lib")

# ===== Main GPS App =====
set(SOURCES
    src/core/rf_core.c
    src/rsa_verify.c
    src/main.cpp
    src/mainwindow.cpp
    src/mapwindow.cpp
)

set(HEADERS
    src/rsa_verify.h
    src/mainwindow.h
    src/mapwindow.h
)

set(ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc")
configure_file("${CMAKE_SOURCE_DIR}/favicon.ico" "${CMAKE_CURRENT_BINARY_DIR}/favicon.ico" COPYONLY)
file(WRITE "${ICON_RC}" "IDI_ICON1 ICON \"favicon.ico\"\n")

add_executable(gps-ui-qt WIN32 ${SOURCES} ${HEADERS} "${ICON_RC}")
target_include_directories(gps-ui-qt PRIVATE src src/core ${UHD_INCLUDE_DIR})
target_link_libraries(gps-ui-qt PRIVATE Qt6::Core Qt6::Widgets Qt6::Network Qt6::WebEngineWidgets Qt6::Concurrent ${UHD_LIBRARY} ws2_32 bcrypt ncrypt winmm)
set(OUTPUT_DIR "C:/Users/Whaoy/Documents/win-realtime/gps-ui-qt/gps-ui-qt_portable")
set_target_properties(gps-ui-qt PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_DIR}")
source_group("src" FILES ${SOURCES} ${HEADERS})

# ===== gps-core =====
add_executable(gps-core "src/gpssim.c" "src/getopt.c")
target_include_directories(gps-core PRIVATE "src" "src/core")
target_link_libraries(gps-core PRIVATE ws2_32 bcrypt ncrypt)
set_target_properties(gps-core PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_DIR}")
