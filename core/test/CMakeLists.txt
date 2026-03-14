cmake_minimum_required(VERSION 3.20)

file(GLOB TEST_SOURCES *.cpp)
add_executable(core_test ${TEST_SOURCES})

# 自动链接线程库
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

target_link_libraries(core_test PRIVATE 
    concurrency-toolkit
    Catch2::Catch2WithMain
    Threads::Threads
)

enable_testing()
add_test(NAME core_unit_tests COMMAND core_test)