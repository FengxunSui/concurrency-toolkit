# core/CMakeLists.txt

# 这是一个头文件-only 库，创建 INTERFACE 目标
add_library(concurrency-toolkit INTERFACE)

target_include_directories(concurrency-toolkit INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)