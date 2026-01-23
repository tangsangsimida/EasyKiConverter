# EasyEDA 模块

add_library(EasyKiConverterEasyeda STATIC
    EasyedaApi.cpp
    EasyedaImporter.cpp
    JLCDatasheet.cpp
)

target_include_directories(EasyKiConverterEasyeda
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterEasyeda
    PUBLIC
        Qt6::Network
        ${ZLIB_LIBRARY}
        EasyKiConverterModels
        EasyKiConverterUtils
)