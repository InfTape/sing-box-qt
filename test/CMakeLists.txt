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
    unit_utils_test.cpp
    ../src/services/config/ConfigBuilder.cpp
    ../src/services/config/ConfigMutator.cpp
    ../src/services/subscription/SubscriptionParser.cpp
    ../src/services/kernel/KernelPlatform.cpp
    ../src/storage/AppSettings.cpp
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

enable_testing()
add_test(
    NAME sing-box-qt-unit-tests
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
        $<TARGET_FILE:sing-box-qt-unit-tests>
)
