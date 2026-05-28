cmake_minimum_required(VERSION 3.16)
project(dialed_in)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(GNUInstallDirs)

# Find Qt packages
find_package(Qt5 COMPONENTS Core Gui Widgets Network REQUIRED)

# toml++: use the system package when available, else the vendored single header.
find_package(tomlplusplus QUIET)

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
    dial_config.cpp
    dial_config.h
    ${COMMON_HEADERS}
)
target_link_libraries(dial_gui PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
)

# Path to the installed default config, read when no user config exists.
target_compile_definitions(dial_gui PRIVATE
    DIALEDIN_DEFAULT_CONFIG="${CMAKE_INSTALL_FULL_DATADIR}/dialedIn/config.toml"
)

if(tomlplusplus_FOUND)
    target_link_libraries(dial_gui PRIVATE tomlplusplus::tomlplusplus)
else()
    target_include_directories(dial_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/extern)
endif()

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

install(FILES config/default.toml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/dialedIn
    RENAME config.toml
)
