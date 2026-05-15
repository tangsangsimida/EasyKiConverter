# UI 模块

add_subdirectory(viewmodels)
add_subdirectory(utils)

add_library(EasyKiConverterUI STATIC)

target_link_libraries(EasyKiConverterUI
    PUBLIC
        EasyKiConverterViewModels
        EasyKiConverterUIUtils
)

target_include_directories(EasyKiConverterUI
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)