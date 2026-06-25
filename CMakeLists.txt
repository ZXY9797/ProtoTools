cmake_minimum_required(VERSION 3.16)

if(APPLE)
    project(KPtools VERSION 1.0.0 LANGUAGES CXX OBJCXX)
else()
    project(KPtools VERSION 1.0.0 LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0002 NEW)

add_subdirectory(app)
