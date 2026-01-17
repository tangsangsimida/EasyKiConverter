cmake_minimum_required(VERSION 3.16)
project(tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用 AUTOMOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Test Network)

# 设置包含路径
include_directories(${CMAKE_SOURCE_DIR})
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

target_link_libraries(test_component_service Qt6::Core Qt6::Test Qt6::Network)

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
)

target_link_libraries(test_export_service Qt6::Core Qt6::Test)

# ========== ConfigService 测试 ==========
add_executable(test_config_service test_config_service.cpp)

# 添加源文件
target_sources(test_config_service PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ConfigService.cpp
)

target_link_libraries(test_config_service Qt6::Core Qt6::Test)

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

target_link_libraries(test_component_data_collector Qt6::Core Qt6::Test Qt6::Network)

# ========== UUID 提取测试 ==========
add_executable(test_uuid_extraction test_uuid_extraction.cpp)
target_link_libraries(test_uuid_extraction Qt6::Core)

# ========== 图层映射测试 ==========
add_executable(test_layer_mapping test_layer_mapping.cpp)

# 添加源文件
target_sources(test_layer_mapping PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/core/utils/LayerMapper.cpp
)

target_link_libraries(test_layer_mapping Qt6::Core)

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
)

target_link_libraries(test_integration Qt6::Core Qt6::Test Qt6::Network)

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
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
)

target_link_libraries(test_performance Qt6::Core Qt6::Test Qt6::Network)