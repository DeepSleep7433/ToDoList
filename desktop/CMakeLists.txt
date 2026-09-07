cmake_minimum_required(VERSION 3.16)
project(ToDoList VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

# 核心库：纯 C++，不依赖 Qt（可独立测试、未来可移植）
add_subdirectory(TodoCore)

# QML 薄壳：需要 Qt6，未安装时自动跳过（不影响 TodoCore 与测试）
find_package(Qt6 QUIET COMPONENTS Quick Qml Gui)
if(Qt6_FOUND)
  add_subdirectory(app)
else()
  message(STATUS "Qt6 not found - skipping QML app shell (desktop/app)")
endif()