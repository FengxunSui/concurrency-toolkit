# core/CMakeLists.txt

# 收集所有需要编译的源文件（包括 lockfree 下的 .cpp）
file(GLOB_RECURSE CORE_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/lockfree/*.cpp
    # 如果有其他目录的 .cpp，继续添加
    # ${CMAKE_CURRENT_SOURCE_DIR}/other/*.cpp
)

# 改为 STATIC 库（不再是 INTERFACE）
add_library(concurrency-toolkit STATIC ${CORE_SOURCES})

target_include_directories(concurrency-toolkit PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 如果有线程依赖，也在这里链接（确保库本身线程安全）
find_package(Threads REQUIRED)
target_link_libraries(concurrency-toolkit PUBLIC Threads::Threads)

# 确保 C++20 标准传播
target_compile_features(concurrency-toolkit PUBLIC cxx_std_20)