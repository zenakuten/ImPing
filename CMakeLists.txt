cmake_minimum_required(VERSION 3.20)
project(imping VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies via vcpkg
find_package(imgui CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/app.cpp
    src/core/target.cpp
    src/ping/pinger.cpp
    src/ping/traceroute.cpp
    src/ui/main_window.cpp
    src/ui/chart_widget.cpp
    src/ui/target_panel.cpp
    src/ui/stats_panel.cpp
    src/ui/traceroute_panel.cpp
)

# Platform-specific ping/traceroute implementations
if(WIN32)
    list(APPEND SOURCES
        src/app_win32.cpp
        src/ping/pinger_win32.cpp
        src/ping/traceroute_win32.cpp
    )
else()
    list(APPEND SOURCES
        src/app_linux.cpp
        src/ping/pinger_linux.cpp
        src/ping/traceroute_linux.cpp
    )
endif()

set(HEADERS
    src/app.hpp
    src/core/ring_buffer.hpp
    src/core/target.hpp
    src/ping/ping_result.hpp
    src/ping/pinger.hpp
    src/ping/traceroute.hpp
    src/ui/main_window.hpp
    src/ui/chart_widget.hpp
    src/ui/target_panel.hpp
    src/ui/stats_panel.hpp
    src/ui/traceroute_panel.hpp
)

# Create executable (WIN32 for Windows GUI app, no console)
if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})
else()
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui::imgui
    SDL3::SDL3
)

# Platform-specific libraries for ping
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ws2_32
        iphlpapi
    )
else()
    # Linux/macOS - no additional libraries needed for raw sockets
endif()

# Enable warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
