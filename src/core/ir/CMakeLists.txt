# IR 中间表示层模块
# EDA 无关的通用数据类型和映射表

add_library(EasyKiConverterIR STATIC)

target_sources(EasyKiConverterIR
    PRIVATE
        IRTypes.h
        SymbolIR.h
        SymbolIR.cpp
        FootprintIR.h
        FootprintIR.cpp
        Model3DIR.h
        ComponentIR.h
        EasyedaLayerMap.h
        EasyedaLayerMap.cpp
        EasyedaPadShapeMap.h
        EasyedaPinTypeMap.h
)

target_include_directories(EasyKiConverterIR
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterIR
    PUBLIC
        Qt6::Core
        Qt6::Gui
)
