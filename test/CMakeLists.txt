cmake_minimum_required(VERSION 3.20)
project(FluorescentButtonTest LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_executable(FluorescentButtonTest
    main.cpp
)

target_link_libraries(FluorescentButtonTest PRIVATE
    Qt6::Core
    Qt6::Widgets
)
