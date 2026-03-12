# core/test/CMakeLists.txt

find_package(Threads REQUIRED)

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git 
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# 收集所有测试源
file(GLOB TEST_SOURCES "*.cpp")

# 只创建一个测试可执行文件
add_executable(core_test ${TEST_SOURCES})

target_link_libraries(core_test PRIVATE
    industrial::core
    Catch2::Catch2WithMain
    Threads::Threads
)

# 测试发现
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(core_test)