cmake_minimum_required(VERSION 3.20)
project(concurrency-toolkit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Build tests" ON)

# 关键：先添加 core，创建 concurrency-toolkit 目标
add_subdirectory(core)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(third_party/Catch2)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Catch2/extras)
    add_subdirectory(core/test)
endif()