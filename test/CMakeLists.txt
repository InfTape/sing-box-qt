cmake_minimum_required(VERSION 3.20)
project(sing-box-qt-tests LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

add_executable(sing-box-qt-unit-tests
    unit_utils_test.cpp
    ../src/utils/home/HomeFormat.cpp
    ../src/utils/proxy/ProxyNodeHelper.cpp
    ../src/utils/rule/RuleUtils.cpp
    ../src/utils/LogParser.cpp
    ../src/utils/subscription/SubscriptionUserinfo.cpp
    ../src/utils/subscription/SubscriptionFormat.cpp
)

target_include_directories(sing-box-qt-unit-tests PRIVATE
    ../src
)

target_link_libraries(sing-box-qt-unit-tests PRIVATE
    Qt6::Core
    Qt6::Test
)

enable_testing()
add_test(
    NAME sing-box-qt-unit-tests
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
        $<TARGET_FILE:sing-box-qt-unit-tests>
)
