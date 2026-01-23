cmake_minimum_required(VERSION 3.20)
project(sing-box-qt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 configuration.
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    WebSockets
    Sql
    Concurrent
)

# Third-party libraries.
include(FetchContent)

# nlohmann/json - JSON parsing.
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# yaml-cpp - YAML parsing.
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# Source files.
set(SOURCES
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/TrayIcon.cpp
    src/core/KernelService.cpp
    src/core/ProcessManager.cpp
    src/core/ProxyService.cpp
    src/core/ProxyController.cpp
    src/core/DelayTestService.cpp
    src/network/HttpClient.cpp
    src/network/WebSocketClient.cpp
    src/network/SubscriptionService.cpp
    src/storage/DatabaseService.cpp
    src/storage/ConfigManager.cpp
    src/storage/AppSettings.cpp
    src/system/SystemProxy.cpp
    src/system/AutoStart.cpp
    src/system/UpdateService.cpp
    src/system/AdminHelper.cpp
    src/views/HomeView.cpp
    src/views/components/TrafficChart.cpp
    src/views/ProxyView.cpp
    src/views/SubscriptionView.cpp
    src/views/ConnectionsView.cpp
    src/views/RulesView.cpp
    src/views/LogView.cpp
    src/views/SettingsView.cpp
    src/widgets/ToggleSwitch.cpp
    src/widgets/RoundedMenu.cpp
    src/utils/Logger.cpp
    src/utils/Crypto.cpp
    src/utils/ThemeManager.cpp
)

set(HEADERS
    src/app/MainWindow.h
    src/app/TrayIcon.h
    src/core/KernelService.h
    src/core/ProcessManager.h
    src/core/ProxyService.h
    src/core/ProxyController.h
    src/core/DelayTestService.h
    src/network/HttpClient.h
    src/network/WebSocketClient.h
    src/network/SubscriptionService.h
    src/storage/DatabaseService.h
    src/storage/ConfigManager.h
    src/storage/AppSettings.h
    src/system/SystemProxy.h
    src/system/AutoStart.h
    src/system/UpdateService.h
    src/system/AdminHelper.h
    src/views/HomeView.h
    src/views/components/TrafficChart.h
    src/views/ProxyView.h
    src/views/SubscriptionView.h
    src/views/ConnectionsView.h
    src/views/RulesView.h
    src/views/LogView.h
    src/views/SettingsView.h
    src/widgets/ToggleSwitch.h
    src/widgets/RoundedMenu.h
    src/utils/Logger.h
    src/utils/Crypto.h
    src/utils/ThemeManager.h
)

# Resource files.
set(RESOURCES
    resources/resources.qrc
)

if(WIN32)
    list(APPEND RESOURCES resources/app.rc)
endif()

# Executable.
add_executable(${PROJECT_NAME} WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# Link libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
    Qt6::Sql
    Qt6::Concurrent
    nlohmann_json::nlohmann_json
    yaml-cpp::yaml-cpp
)

# Windows-specific configuration.
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ws2_32
        wininet
    )
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    endif()
    # Set Windows application icon.
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Include directories.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Install configuration.
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
