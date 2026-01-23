# KiCad 模块

add_library(EasyKiConverterKicad STATIC
    ExporterSymbol.cpp
    ExporterFootprint.cpp
    Exporter3DModel.cpp
)

target_include_directories(EasyKiConverterKicad
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterKicad
    PUBLIC
        Qt6::Core
        EasyKiConverterModels
        EasyKiConverterUtils
)