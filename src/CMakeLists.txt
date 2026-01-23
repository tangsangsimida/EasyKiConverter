# EasyKiConverter 源码模块

# 添加子模块
add_subdirectory(models)
add_subdirectory(core)
add_subdirectory(services)
add_subdirectory(workers)
add_subdirectory(ui)

# 创建主源码库
add_library(EasyKiConverterLib STATIC
    main.cpp
)

# 链接所有子模块
target_link_libraries(EasyKiConverterLib
    PRIVATE
        EasyKiConverterModels
        EasyKiConverterCore
        EasyKiConverterServices
        EasyKiConverterWorkers
        EasyKiConverterUI
)

# 包含目录
target_include_directories(EasyKiConverterLib
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 编译定义
if(ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT)
    target_compile_definitions(EasyKiConverterLib PRIVATE ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT)
endif()

# 编译选项
if(MSVC)
    target_compile_options(EasyKiConverterLib PRIVATE /W4)
else()
    target_compile_options(EasyKiConverterLib PRIVATE -Wall -Wextra -Wpedantic)
endif()