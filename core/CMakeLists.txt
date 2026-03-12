# 平台检测不变 ...

file(GLOB_RECURSE CORE_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

if(CORE_SOURCES)
    # 有源文件：STATIC 库
    add_library(core STATIC ${CORE_SOURCES})
    add_library(industrial::core ALIAS core)
    
    target_compile_definitions(core PUBLIC
        SPINLOCK_${SPINLOCK_PLATFORM}
    )
    target_include_directories(core PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    )
    target_compile_features(core PUBLIC cxx_std_17)
    
else()
    # 纯头文件：INTERFACE 库
    add_library(core INTERFACE)
    add_library(industrial::core ALIAS core)
    
    target_compile_definitions(core INTERFACE
        SPINLOCK_${SPINLOCK_PLATFORM}
    )
    target_include_directories(core INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    )
    target_compile_features(core INTERFACE cxx_std_17)
endif()

if(BUILD_TESTING)
    add_subdirectory(test)
endif()