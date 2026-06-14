cmake_minimum_required(VERSION 3.20)
project(concurrency-toolkit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Build tests" ON)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

# ========== Sanitizer 函数 ==========
function(add_sanitizer_flags target)
    if(ENABLE_ASAN)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target} PRIVATE
                -fsanitize=address,leak
                -fno-omit-frame-pointer
                -g
            )
            target_link_options(${target} PRIVATE
                -fsanitize=address,leak
            )
        elseif(MSVC)
            target_compile_options(${target} PRIVATE /fsanitize=address)
            set_target_properties(${target} PROPERTIES LINK_FLAGS "/INCREMENTAL:NO")
        endif()
    endif()
endfunction()

# 关键：先添加 core，创建 concurrency-toolkit 目标
add_subdirectory(core)

# 对 core 的主 target 加 Sanitizer
add_sanitizer_flags(concurrency-toolkit)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(third_party/Catch2)
    # 不对 Catch2 加 Sanitizer，避免编译问题
    
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Catch2/extras)
    add_subdirectory(core/test)
endif()