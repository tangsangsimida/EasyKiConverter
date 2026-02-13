# EasyEDA 模块

add_library(EasyKiConverterEasyeda STATIC
    EasyedaApi.cpp
    EasyedaImporter.cpp
    EasyedaSymbolImporter.cpp
    EasyedaFootprintImporter.cpp
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
        EasyKiConverterModels
        EasyKiConverterUtils
)

# 链接 zlib（通过 EasyKiConverterUtils 已经链接了，这里不需要重复链接）
# EasyKiConverterUtils 已经正确处理了 Qt zlib 和 vcpkg zlib 的情况
