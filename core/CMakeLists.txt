# core/CMakeLists.txt

# 平台检测
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i[3-6]86")
    set(SPINLOCK_PLATFORM "X86")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm")
    set(SPINLOCK_PLATFORM "ARM")
else()
    # 备用检测
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles("
        #if defined(__x86_64__) || defined(__i386__)
        int main() {}
        #else
        #error Not x86
        #endif
    " IS_X86)
    if(IS_X86)
        set(SPINLOCK_PLATFORM "X86")
    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()
endif()

message(STATUS "SpinLock platform: ${SPINLOCK_PLATFORM}")

# 创建库
add_library(core INTERFACE)
add_library(industrial::core ALIAS core)

target_compile_definitions(core INTERFACE
    SPINLOCK_${SPINLOCK_PLATFORM}
)

target_include_directories(core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_compile_features(core INTERFACE cxx_std_17)

# 测试
if(BUILD_TESTING)
    add_subdirectory(test)
endif()