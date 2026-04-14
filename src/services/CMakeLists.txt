# Services 模块

# 先定义目标，这样子目录里的 target_include_directories 才能找到它
add_library(EasyKiConverterServices STATIC
    BomParser.cpp
    LcscImageService.cpp
    ComponentService.cpp
    ComponentCacheService.cpp
    ConfigService.cpp
    MemoryMonitor.cpp
    ComponentQueueManager.cpp
    ParallelFetchContext.cpp
    PreviewImageExporter.cpp
    DatasheetExporter.cpp
)

# 新架构导出模块
add_subdirectory(export)

# 将子目录定义的源码加入目标
target_sources(EasyKiConverterServices PRIVATE
    ${EXPORT_SOURCES}
    ${EXPORT_HEADERS}
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
# 注意：QXlsx 库的 CMake 配置会触发 GuiPrivate 警告，这是 QXlsx 库的问题，不是我们项目的问题
if(TARGET QXlsx::QXlsx)
    target_link_libraries(EasyKiConverterServices PUBLIC QXlsx::QXlsx)
endif()
