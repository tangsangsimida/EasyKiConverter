# Services 模块

add_library(EasyKiConverterServices STATIC
    BomParser.cpp
    LcscImageService.cpp
    ComponentService.cpp
    ExportService.cpp
    ExportService_Pipeline.cpp
    ConfigService.cpp
    ComponentDataCollector.cpp
    ComponentExportTask.cpp
)

target_include_directories(EasyKiConverterServices
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EasyKiConverterServices
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Concurrent
        Qt6::Quick
        EasyKiConverterModels
        EasyKiConverterCore
        EasyKiConverterEasyeda
        EasyKiConverterKicad
        EasyKiConverterUtils
        EasyKiConverterWorkers
)

# 条件链接 QXlsx
if(TARGET QXlsx::QXlsx)
    target_link_libraries(EasyKiConverterServices PUBLIC QXlsx::QXlsx)
endif()
