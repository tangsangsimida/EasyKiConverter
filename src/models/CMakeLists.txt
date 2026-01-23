# Models 模块

add_library(EasyKiConverterModels STATIC
    ComponentData.cpp
    SymbolData.cpp
    FootprintData.cpp
    Model3DData.cpp
)

target_include_directories(EasyKiConverterModels
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)