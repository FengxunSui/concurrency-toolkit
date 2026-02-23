# core/test/CMakeLists.txt

# 查找线程库（关键！）
find_package(Threads REQUIRED)

# 获取 Catch2
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# 收集测试源
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(core_test ${TEST_SOURCES})

# 链接库（关键：添加 Threads::Threads）
target_link_libraries(core_test PRIVATE
    industrial::core
    Catch2::Catch2WithMain
    Threads::Threads  # ← 解决 pthread 链接问题
)

# 测试发现
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(core_test)