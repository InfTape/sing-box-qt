cmake_minimum_required(VERSION 3.20)
project(sing-box-qt-tests LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Test Widgets Network Sql)
include(FetchContent)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG f0329d0efd2e62ceda5ee899c343ee445f6f7bd2
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

add_executable(sing-box-qt-unit-tests
    unit_tests_main.cpp
    config/config_builder_test.cpp
    config/config_mutator_test.cpp
    subscription/subscription_parser_basic_test.cpp
    subscription/subscription_parser_advanced_test.cpp
    utils/rule_and_ruleconfig_test.cpp
    utils/format_and_proxy_test.cpp
    utils/log_and_subscription_meta_test.cpp
    utils/misc_services_test.cpp
    ../src/services/config/ConfigBuilder.cpp
    ../src/services/config/ConfigMutator.cpp
    ../src/services/rules/RuleConfigService.cpp
    ../src/services/rules/SharedRulesStore.cpp
    ../src/services/subscription/SubscriptionParser.cpp
    ../src/services/kernel/KernelPlatform.cpp
    ../src/storage/AppSettings.cpp
    ../src/storage/ConfigIO.cpp
    ../src/storage/DatabaseService.cpp
    ../src/utils/Crypto.cpp
    ../src/utils/PortableMode.cpp
    ../src/utils/home/HomeFormat.cpp
    ../src/utils/Logger.cpp
    ../src/utils/settings/SettingsHelpers.cpp
    ../src/utils/proxy/ProxyNodeHelper.cpp
    ../src/utils/rule/RuleUtils.cpp
    ../src/utils/LogParser.cpp
    ../src/utils/subscription/SubscriptionHelpers.cpp
    ../src/utils/subscription/SubscriptionUserinfo.cpp
    ../src/utils/subscription/SubscriptionFormat.cpp
    ../src/core/DataUsageTracker.cpp
)

target_include_directories(sing-box-qt-unit-tests PRIVATE
    ../src
)

target_link_libraries(sing-box-qt-unit-tests PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Test
    Qt6::Widgets
    yaml-cpp::yaml-cpp
)

if (MSVC AND TARGET yaml-cpp)
    target_compile_options(yaml-cpp PRIVATE /wd4267)
endif()

enable_testing()
add_test(
    NAME sing-box-qt-unit-tests
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
        $<TARGET_FILE:sing-box-qt-unit-tests>
)
