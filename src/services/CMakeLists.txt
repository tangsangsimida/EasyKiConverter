# Services 模块

add_library(EasyKiConverterServices STATIC
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
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterServices
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Concurrent
        Qt6::Quick
        QXlsx::QXlsx
        EasyKiConverterModels
        EasyKiConverterCore
        EasyKiConverterEasyeda
        EasyKiConverterKicad
        EasyKiConverterUtils
        EasyKiConverterWorkers
)