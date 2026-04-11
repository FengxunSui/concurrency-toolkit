# core/test/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

file(GLOB TEST_SOURCES *.cpp)
add_executable(core_test ${TEST_SOURCES})

# 自动链接线程库（你的原有配置）
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

target_link_libraries(core_test PRIVATE 
    concurrency-toolkit  # 现在这是 STATIC 库，包含 .cpp 实现
    Catch2::Catch2WithMain
    Threads::Threads     # 测试本身也需要线程
)

# Catch2 测试发现（可选但推荐）
include(CTest)
include(Catch)
catch_discover_tests(core_test)

enable_testing()
add_test(NAME core_unit_tests COMMAND core_test)