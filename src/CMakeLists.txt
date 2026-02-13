# EasyKiConverter 源码模块

# 添加子模块（注意顺序：Utils 需要最先添加，因为其他模块依赖日志系统）
add_subdirectory(utils)
add_subdirectory(models)
add_subdirectory(core)
add_subdirectory(workers)
add_subdirectory(services)
add_subdirectory(ui)

# 创建主源码库（不包含 main.cpp，main.cpp 在根目录）
add_library(EasyKiConverterLib STATIC)

# 链接所有子模块（QXlsx 是必须的依赖）
target_link_libraries(EasyKiConverterLib
    PRIVATE
        EasyKiConverterLogging
        EasyKiConverterModels
        EasyKiConverterCore
        EasyKiConverterWorkers
        EasyKiConverterServices
        EasyKiConverterUI
        QXlsx::QXlsx
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

# 确保 Services 模块可以访问 Workers 的头文件
target_include_directories(EasyKiConverterServices
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/workers
)