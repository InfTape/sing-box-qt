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
    Svg
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
    src/app/AppBootstrapper.cpp
    src/app/AppContext.cpp
    src/app/impl/ConfigRepositoryAdapter.cpp
    src/app/impl/SettingsStoreAdapter.cpp
    src/app/impl/ThemeServiceAdapter.cpp
    src/app/impl/SystemProxyAdapter.cpp
    src/app/impl/AdminActionsAdapter.cpp
    src/app/ProxyUiController.cpp
    src/app/ProxyRuntimeController.cpp
    src/app/MainWindow.cpp
    src/app/TrayIcon.cpp
    src/core/KernelService.cpp
    src/core/ProcessManager.cpp
    src/core/ProxyService.cpp
    src/core/ProxyController.cpp
    src/core/DelayTestService.cpp
    src/dialogs/subscription/SubscriptionFormDialog.cpp
    src/dialogs/config/ConfigEditDialog.cpp
    src/dialogs/rules/RuleEditorDialog.cpp
    src/dialogs/rules/ManageRuleSetsDialog.cpp
    src/network/HttpClient.cpp
    src/network/WebSocketClient.cpp
    src/network/SubscriptionService.cpp
    src/services/subscription/SubscriptionParser.cpp
    src/services/config/ConfigBuilder.cpp
    src/services/config/ConfigManager.cpp
    src/services/config/ConfigMutator.cpp
    src/services/rules/SharedRulesStore.cpp
    src/services/rules/RuleConfigService.cpp
    src/storage/DatabaseService.cpp
    src/storage/ConfigIO.cpp
    src/storage/SubscriptionConfigStore.cpp
    src/dialogs/subscription/NodeEditDialog.cpp
    src/dialogs/editors/GenericNodeEditor.cpp
    src/storage/AppSettings.cpp
    src/system/SystemProxy.cpp
    src/system/AutoStart.cpp
    src/system/UpdateService.cpp
    src/system/AdminHelper.cpp
    src/views/home/HomeView.cpp
    src/views/components/TrafficChart.cpp
    src/views/proxy/ProxyView.cpp
    src/views/proxy/ProxyViewController.cpp
    src/views/subscription/SubscriptionView.cpp
    src/views/subscription/SubscriptionCard.cpp
    src/views/subscription/SubscriptionController.cpp
    src/views/connections/ConnectionsView.cpp
    src/views/rules/RulesView.cpp
    src/views/logs/LogView.cpp
    src/views/settings/SettingsView.cpp
    src/services/kernel/KernelManager.cpp
    src/services/kernel/KernelPlatform.cpp
    src/services/settings/SettingsService.cpp
    src/models/SettingsModel.cpp
    src/widgets/common/MenuComboBox.cpp
    src/widgets/logs/LogRowWidget.cpp
    src/widgets/rules/RuleCard.cpp
    src/widgets/common/ToggleSwitch.cpp
    src/widgets/common/ChevronToggle.cpp
    src/widgets/common/RoundedMenu.cpp
    src/utils/Logger.cpp
    src/utils/Crypto.cpp
    src/utils/LogParser.cpp
    src/utils/subscription/SubscriptionUserinfo.cpp
    src/utils/layout/CardGridLayoutHelper.cpp
    src/utils/layout/CardGridAnimation.cpp
    src/utils/subscription/SubscriptionHelpers.cpp
    src/utils/subscription/SubscriptionActions.cpp
    src/utils/subscription/SubscriptionFormat.cpp
    src/utils/proxy/ProxyActions.cpp
    src/utils/proxy/ProxyNodeHelper.cpp
    src/utils/home/HomeFormat.cpp
    src/utils/settings/SettingsHelpers.cpp
    src/utils/rule/RuleUtils.cpp
    src/utils/ThemeManager.cpp
)

set(HEADERS
    src/app/AppBootstrapper.h
    src/app/AppContext.h
    src/app/interfaces/ConfigRepository.h
    src/app/interfaces/SettingsStore.h
    src/app/interfaces/ThemeService.h
    src/app/interfaces/SystemProxyGateway.h
    src/app/interfaces/AdminActions.h
    src/app/impl/ConfigRepositoryAdapter.h
    src/app/impl/SettingsStoreAdapter.h
    src/app/impl/ThemeServiceAdapter.h
    src/app/impl/SystemProxyAdapter.h
    src/app/impl/AdminActionsAdapter.h
    src/app/ProxyUiController.h
    src/app/ProxyRuntimeController.h
    src/app/MainWindow.h
    src/app/TrayIcon.h
    src/core/KernelService.h
    src/core/ProcessManager.h
    src/core/ProxyService.h
    src/core/ProxyController.h
    src/core/DelayTestService.h
    src/dialogs/subscription/SubscriptionFormDialog.h
    src/dialogs/config/ConfigEditDialog.h
    src/dialogs/rules/RuleEditorDialog.h
    src/dialogs/rules/ManageRuleSetsDialog.h
    src/models/RuleItem.h
    src/network/HttpClient.h
    src/network/WebSocketClient.h
    src/network/SubscriptionService.h
    src/services/subscription/SubscriptionParser.h
    src/services/config/ConfigBuilder.h
    src/services/config/ConfigManager.h
    src/services/config/ConfigMutator.h
    src/services/rules/SharedRulesStore.h
    src/services/rules/RuleConfigService.h
    src/storage/DatabaseService.h
    src/dialogs/subscription/NodeEditDialog.h
    src/dialogs/editors/NodeEditor.h
    src/dialogs/editors/GenericNodeEditor.h
    src/storage/ConfigIO.h
    src/storage/SubscriptionConfigStore.h
    src/storage/AppSettings.h
    src/system/SystemProxy.h
    src/system/AutoStart.h
    src/system/UpdateService.h
    src/system/AdminHelper.h
    src/views/home/HomeView.h
    src/views/components/TrafficChart.h
    src/views/proxy/ProxyView.h
    src/views/proxy/ProxyViewController.h
    src/views/subscription/SubscriptionView.h
    src/views/subscription/SubscriptionCard.h
    src/views/subscription/SubscriptionController.h
    src/views/connections/ConnectionsView.h
    src/views/rules/RulesView.h
    src/views/logs/LogView.h
    src/views/settings/SettingsView.h
    src/services/kernel/KernelManager.h
    src/services/kernel/KernelPlatform.h
    src/services/settings/SettingsService.h
    src/models/SettingsModel.h
    src/widgets/common/MenuComboBox.h
    src/widgets/logs/LogRowWidget.h
    src/widgets/rules/RuleCard.h
    src/widgets/common/ToggleSwitch.h
    src/widgets/common/ChevronToggle.h
    src/widgets/common/RoundedMenu.h
    src/utils/Logger.h
    src/utils/Crypto.h
    src/utils/LogParser.h
    src/utils/subscription/SubscriptionUserinfo.h
    src/utils/layout/CardGridLayoutHelper.h
    src/utils/layout/CardGridAnimation.h
    src/utils/subscription/SubscriptionHelpers.h
    src/utils/subscription/SubscriptionActions.h
    src/utils/subscription/SubscriptionFormat.h
    src/utils/proxy/ProxyActions.h
    src/utils/proxy/ProxyNodeHelper.h
    src/utils/home/HomeFormat.h
    src/utils/settings/SettingsHelpers.h
    src/utils/rule/RuleUtils.h
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
    Qt6::Svg
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
