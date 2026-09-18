# Altium 模块

add_library(EasyKiConverterAltium STATIC)

target_sources(EasyKiConverterAltium
    PRIVATE
        # 导出器
        ExporterAltiumSymbol.h
        ExporterAltiumSymbol.cpp
        AltiumSymbolPinConverter.h
        AltiumSymbolPinConverter.cpp
        ExporterAltiumFootprint.h
        ExporterAltiumFootprint.cpp

        # 写入器
        writers/AltiumSchFontRegistry.h
        writers/AltiumSchFontRegistry.cpp
        writers/AltiumSchImageStorageEncoder.h
        writers/AltiumSchImageStorageEncoder.cpp
        writers/AltiumSchImageStorageWriter.h
        writers/AltiumSchImageStorageWriter.cpp
        writers/AltiumSchComponentStorageWriter.h
        writers/AltiumSchComponentStorageWriter.cpp
        writers/AltiumSchOwnershipValidator.h
        writers/AltiumSchOwnershipValidator.cpp
        writers/AltiumSchGeometryValidator.h
        writers/AltiumSchGeometryValidator.cpp
        writers/AltiumSchLibWriter.h
        writers/AltiumSchLibWriter.cpp
        writers/AltiumSchLibraryHeaderWriter.h
        writers/AltiumSchLibraryHeaderWriter.cpp
        writers/AltiumSchGraphicOrderWriter.h
        writers/AltiumSchGraphicOrderWriter.cpp
        writers/AltiumSchTextRecordWriter.h
        writers/AltiumSchTextRecordWriter.cpp
        writers/AltiumSchImageRecordWriter.h
        writers/AltiumSchImageRecordWriter.cpp
        writers/AltiumSchPinRecordWriter.h
        writers/AltiumSchPinRecordWriter.cpp
        writers/AltiumSchPrimitiveRecordWriter.h
        writers/AltiumSchPrimitiveRecordWriter.cpp
        writers/AltiumSchComponentRecordWriter.h
        writers/AltiumSchComponentRecordWriter.cpp
        writers/AltiumPcbLibWriter.h
        writers/AltiumPcbLibWriter.cpp

        # 读取器
        readers/AltiumPcbLibReader.h
        readers/AltiumPcbLibReader.cpp
        readers/AltiumSchLibReader.h
        readers/AltiumSchLibReader.cpp

        # OLE 复合文档
        compound/OLECompoundReader.h
        compound/OLECompoundReader.cpp
        compound/OLECompoundWriter.h
        compound/OLECompoundWriter.cpp

        # 工具类
        utils/AltiumBinaryReader.h
        utils/AltiumBinaryReader.cpp
        utils/AltiumBinaryWriter.h
        utils/AltiumBinaryWriter.cpp
        utils/AltiumCoord.h
        utils/AltiumSymbolConversionUtils.h
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
        EasyKiConverterIR
)

if(TARGET ZLIB::ZLIB)
    target_link_libraries(EasyKiConverterAltium PUBLIC ZLIB::ZLIB)
else()
    target_link_libraries(EasyKiConverterAltium PUBLIC ${ZLIB_LIBRARY})
endif()
