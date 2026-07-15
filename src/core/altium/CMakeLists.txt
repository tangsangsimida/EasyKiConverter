# Altium 模块

add_library(EasyKiConverterAltium STATIC)

target_sources(EasyKiConverterAltium
    PRIVATE
        # 导出器
        ExporterAltiumSymbol.h
        ExporterAltiumSymbol.cpp
        ExporterAltiumFootprint.h
        ExporterAltiumFootprint.cpp

        # 写入器
        writers/AltiumSchLibWriter.h
        writers/AltiumSchLibWriter.cpp
        writers/AltiumPcbLibWriter.h
        writers/AltiumPcbLibWriter.cpp

        # OLE 复合文档
        compound/OLECompoundWriter.h
        compound/OLECompoundWriter.cpp

        # 工具类
        utils/AltiumBinaryWriter.h
        utils/AltiumBinaryWriter.cpp
        utils/AltiumCoord.h
        utils/AltiumLayerMap.h
        utils/AltiumLayerMap.cpp

        # 模型定义
        models/AltiumCommon.h
        models/AltiumSchComponent.h
        models/AltiumPcbComponent.h
)

target_include_directories(EasyKiConverterAltium
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterAltium
    PUBLIC
        Qt6::Core
        EasyKiConverterModels
)
