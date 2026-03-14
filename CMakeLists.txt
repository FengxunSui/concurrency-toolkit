cmake_minimum_required(VERSION 3.20)
project(concurrency-toolkit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Build tests" ON)

add_library(concurrency-toolkit INTERFACE)
target_include_directories(concurrency-toolkit INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

if(BUILD_TESTING)
    enable_testing()
    
    add_subdirectory(third_party/Catch2)
    
    # 关键：传递模块路径给子目录
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Catch2/extras)
    
    add_subdirectory(core/test)
endif()