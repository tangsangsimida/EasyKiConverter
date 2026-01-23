cmake_minimum_required(VERSION 3.16)
project(tests)

# 启用测试
enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用 AUTOMOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Test Network)

# 查找 zlib（NetworkUtils.cpp 使用 zlib API）
find_package(ZLIB QUIET)

# 输出 zlib 查找结果（用于调试）
message(STATUS "ZLIB_FOUND: ${ZLIB_FOUND}")
message(STATUS "ZLIB_INCLUDE_DIRS: ${ZLIB_INCLUDE_DIRS}")
message(STATUS "ZLIB_LIBRARIES: ${ZLIB_LIBRARIES}")
message(STATUS "ZLIB_VERSION: ${ZLIB_VERSION}")

# 设置包含路径
include_directories(${CMAKE_SOURCE_DIR}/../src)

# 定义一个宏，让源文件使用正确的包含路径
add_definitions(-DTESTING_MODE)

# ========== ComponentService 测试 ==========
add_executable(test_component_service test_component_service.cpp)

# 添加源文件
target_sources(test_component_service PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentService.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaApi.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaImporter.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/JLCDatasheet.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
)

# 添加包含目录
target_include_directories(test_component_service PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接库（统一使用 PRIVATE 关键字风格）
target_link_libraries(test_component_service PRIVATE Qt6::Core Qt6::Test Qt6::Network)
if(ZLIB_FOUND)
    target_link_libraries(test_component_service PRIVATE ZLIB::ZLIB)
else()
    target_link_libraries(test_component_service PRIVATE z)
endif()

# 注册测试
add_test(NAME test_component_service COMMAND test_component_service)

# ========== ExportService 测试 ==========
add_executable(test_export_service test_export_service.cpp)

# 添加源文件
target_sources(test_export_service PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentExportTask.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterSymbol.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterFootprint.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/Exporter3DModel.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/GeometryUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/LayerMapper.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/SvgPathParser.cpp
)

target_include_directories(test_export_service PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接 Qt Network 和 zlib（统一使用 PRIVATE 关键字风格）
target_link_libraries(test_export_service PRIVATE Qt6::Core Qt6::Test Qt6::Network ZLIB::ZLIB)

# 注册测试
add_test(NAME test_export_service COMMAND test_export_service)

# ========== ExportServicePipeline 测试 ==========
add_executable(test_export_service_pipeline test_export_service_pipeline.cpp)

# 添加源文件
target_sources(test_export_service_pipeline PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService_Pipeline.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentExportTask.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterSymbol.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterFootprint.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/Exporter3DModel.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/GeometryUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/LayerMapper.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/SvgPathParser.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaImporter.cpp
    ${CMAKE_SOURCE_DIR}/../src/workers/FetchWorker.cpp
    ${CMAKE_SOURCE_DIR}/../src/workers/ProcessWorker.cpp
    ${CMAKE_SOURCE_DIR}/../src/workers/WriteWorker.cpp
)

target_include_directories(test_export_service_pipeline PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接 zlib
target_link_libraries(test_export_service_pipeline PRIVATE Qt6::Core Qt6::Test Qt6::Network ZLIB::ZLIB)

# 注册测试
add_test(NAME test_export_service_pipeline COMMAND test_export_service_pipeline)
# 设置 CTest 超时时间为 120 秒（流水线测试需要较长时间）
set_property(TEST test_export_service_pipeline PROPERTY TIMEOUT 120)

# ========== ConfigService 测试 ==========
add_executable(test_config_service test_config_service.cpp)

# 添加源文件
target_sources(test_config_service PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ConfigService.cpp
)

target_include_directories(test_config_service PRIVATE ${CMAKE_SOURCE_DIR}/../src)

target_link_libraries(test_config_service Qt6::Core Qt6::Test)

# 注册测试
add_test(NAME test_config_service COMMAND test_config_service)

# ========== ComponentDataCollector 测试 ==========
add_executable(test_component_data_collector test_component_data_collector.cpp)

# 添加源文件
target_sources(test_component_data_collector PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentDataCollector.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaApi.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaImporter.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
)

target_include_directories(test_component_data_collector PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接库（统一使用 PRIVATE 关键字风格）
target_link_libraries(test_component_data_collector PRIVATE Qt6::Core Qt6::Test Qt6::Network ZLIB::ZLIB)

# 注册测试
add_test(NAME test_component_data_collector COMMAND test_component_data_collector)
# 设置 CTest 超时时间为 60 秒（集成测试需要较长时间）
set_property(TEST test_component_data_collector PROPERTY TIMEOUT 60)

# ========== UUID 提取测试 ==========
add_executable(test_uuid_extraction test_uuid_extraction.cpp)
target_include_directories(test_uuid_extraction PRIVATE ${CMAKE_SOURCE_DIR}/../src)

target_link_libraries(test_uuid_extraction Qt6::Core)

# 注册测试
add_test(NAME test_uuid_extraction COMMAND test_uuid_extraction)

# ========== 图层映射测试 ==========
add_executable(test_layer_mapping test_layer_mapping.cpp)

# 添加源文件
target_sources(test_layer_mapping PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/core/utils/LayerMapper.cpp
)

target_include_directories(test_layer_mapping PRIVATE ${CMAKE_SOURCE_DIR}/../src)

target_link_libraries(test_layer_mapping Qt6::Core)

# 注册测试
add_test(NAME test_layer_mapping COMMAND test_layer_mapping)

# ========== 集成测试 ==========
add_executable(test_integration test_integration.cpp)

# 添加源文件
target_sources(test_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ConfigService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentExportTask.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaApi.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaImporter.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/JLCDatasheet.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterSymbol.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterFootprint.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/Exporter3DModel.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/GeometryUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/LayerMapper.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/SvgPathParser.cpp
)

target_include_directories(test_integration PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接 zlib
target_link_libraries(test_integration PRIVATE Qt6::Core Qt6::Test Qt6::Network ZLIB::ZLIB)

# 注册测试
add_test(NAME test_integration COMMAND test_integration)
# 设置 CTest 超时时间为 120 秒（集成测试需要较长时间）
set_property(TEST test_integration PROPERTY TIMEOUT 120)

# ========== 性能测试 ==========
add_executable(test_performance test_performance.cpp)

# 添加源文件
target_sources(test_performance PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ConfigService.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaApi.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaImporter.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterSymbol.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterFootprint.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/Exporter3DModel.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/GeometryUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/LayerMapper.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/SvgPathParser.cpp
)

target_include_directories(test_performance PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接 zlib
target_link_libraries(test_performance PRIVATE Qt6::Core Qt6::Test Qt6::Network ZLIB::ZLIB)

# 注册测试
add_test(NAME test_performance COMMAND test_performance)
# 设置 CTest 超时时间为 180 秒（性能测试需要较长时间）
set_property(TEST test_performance PROPERTY TIMEOUT 180)

# ========== 流水线性能基准测试 ==========
add_executable(test_pipeline_baseline test_pipeline_baseline.cpp)

# 添加源文件
target_sources(test_pipeline_baseline PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService_Pipeline.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ConfigService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentExportTask.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaApi.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaImporter.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterSymbol.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterFootprint.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/Exporter3DModel.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/GeometryUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/LayerMapper.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/SvgPathParser.cpp
    ${CMAKE_SOURCE_DIR}/../src/workers/FetchWorker.cpp
    ${CMAKE_SOURCE_DIR}/../src/workers/ProcessWorker.cpp
    ${CMAKE_SOURCE_DIR}/../src/workers/WriteWorker.cpp
    ${CMAKE_SOURCE_DIR}/../src/utils/BoundedThreadSafeQueue.h
)

target_include_directories(test_pipeline_baseline PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接 zlib
target_link_libraries(test_pipeline_baseline PRIVATE Qt6::Core Qt6::Test Qt6::Network ZLIB::ZLIB)

# 注册测试
add_test(NAME test_pipeline_baseline COMMAND test_pipeline_baseline)
# 设置 CTest 超时时间为 60 秒（流水线基准测试使用虚拟 ID）
set_property(TEST test_pipeline_baseline PROPERTY TIMEOUT 60)