# Core 模块

add_subdirectory(easyeda)
add_subdirectory(kicad)
add_subdirectory(utils)

add_library(EasyKiConverterCore STATIC)

target_link_libraries(EasyKiConverterCore
    PUBLIC
        EasyKiConverterEasyeda
        EasyKiConverterKicad
        EasyKiConverterUtils
)

target_include_directories(EasyKiConverterCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)