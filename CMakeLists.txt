# /mnt/d/project/cpp/concurrency-toolkit/CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(ConcurrencyToolkit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Build tests" ON)

# 添加 core 子目录（这会加载 core/CMakeLists.txt）
add_subdirectory(core)

# 可选：添加其他模块
# add_subdirectory(containers)
# add_subdirectory(synchronization)

if(BUILD_TESTING)
    enable_testing()
endif()