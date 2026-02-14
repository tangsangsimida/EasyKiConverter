# EasyKiConverter Utils 模块

# 查找 Qt 组件
find_package(Qt6 REQUIRED COMPONENTS Core)

# 1. 日志系统库
add_library(EasyKiConverterLogging STATIC
    # 日志核心
    logging/Logger.cpp
    logging/PatternFormatter.cpp
    
    # 输出器
    logging/ConsoleAppender.cpp
    logging/FileAppender.cpp
    
    # 适配器
    logging/QtLogAdapter.cpp
)

# 设置目标属性
set_target_properties(EasyKiConverterLogging PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

# 链接 Qt 库
target_link_libraries(EasyKiConverterLogging
    PUBLIC
        Qt6::Core
)

# 包含目录
target_include_directories(EasyKiConverterLogging
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 编译选项
if(MSVC)
    target_compile_options(EasyKiConverterLogging PRIVATE /W4)
else()
    target_compile_options(EasyKiConverterLogging PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 导出目标
add_library(EasyKiConverter::Logging ALIAS EasyKiConverterLogging)


# 2. 通用工具库 (PathSecurity 等)
add_library(EasyKiConverterSafeUtils STATIC
    PathSecurity.cpp
    PathSecurity.h
    BoundedThreadSafeQueue.h
)

set_target_properties(EasyKiConverterSafeUtils PROPERTIES
    AUTOMOC ON
)

target_link_libraries(EasyKiConverterSafeUtils
    PUBLIC
        Qt6::Core
        EasyKiConverterLogging  # 工具库可能需要打日志
)

target_include_directories(EasyKiConverterSafeUtils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(EasyKiConverterSafeUtils PRIVATE /W4)
else()
    target_compile_options(EasyKiConverterSafeUtils PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(EasyKiConverter::SafeUtils ALIAS EasyKiConverterSafeUtils)
