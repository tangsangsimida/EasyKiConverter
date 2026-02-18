# Services 模块

# 先定义目标，这样子目录里的 target_include_directories 才能找到它
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

# 然后添加子目录，子目录会填充 PIPELINE_SOURCES 和 PIPELINE_HEADERS 变量
# 并且子目录里的 target_include_directories 也能正常工作了
add_subdirectory(pipeline)

# 将子目录定义的源码加入目标
target_sources(EasyKiConverterServices PRIVATE
    ${PIPELINE_SOURCES}
    ${PIPELINE_HEADERS}
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
        EasyKiConverterSafeUtils
        EasyKiConverterWorkers
)

# 条件链接 QXlsx
if(TARGET QXlsx::QXlsx)
    target_link_libraries(EasyKiConverterServices PUBLIC QXlsx::QXlsx)
endif()
