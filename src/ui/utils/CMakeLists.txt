# UI Utils 模块

add_library(EasyKiConverterUIUtils STATIC
    ConfigManager.cpp
)

target_include_directories(EasyKiConverterUIUtils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterUIUtils
    PUBLIC
        Qt6::Core
)