cmake_minimum_required(VERSION 3.16)
project(dialed_in)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt packages
find_package(Qt5 COMPONENTS Core Gui Widgets Network REQUIRED)

# Set include directories
include_directories(${Qt5Core_INCLUDE_DIRS}
                  ${Qt5Gui_INCLUDE_DIRS}
                  ${Qt5Widgets_INCLUDE_DIRS}
                  ${Qt5Network_INCLUDE_DIRS})

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Common header
set(COMMON_HEADERS
    dial_common.h
)

# GUI executable
add_executable(dial_gui 
    dial_gui.cpp
    dial_gui.h
    ${COMMON_HEADERS}
)
target_link_libraries(dial_gui PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
)

# Daemon executable
add_executable(dial_daemon
    dial_daemon.cpp
    ${COMMON_HEADERS}
)
target_link_libraries(dial_daemon PRIVATE
    Qt5::Core
    Qt5::Network
)

# Installation
install(TARGETS dial_gui dial_daemon
    RUNTIME DESTINATION bin
)

 