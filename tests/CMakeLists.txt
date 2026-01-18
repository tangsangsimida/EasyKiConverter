cmake_minimum_required(VERSION 3.16)
project(tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用 AUTOMOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 测试覆盖率选项
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

if(ENABLE_COVERAGE)
    message(STATUS "Tests: Code coverage is ENABLED")
    # GCC/MinGW 的覆盖率编译选项
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        add_compile_options(--coverage)
        add_link_options(--coverage)
    endif()
else()
    message(STATUS "Tests: Code coverage is DISABLED")
endif()

# 查找 Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Test Network)

# 设置包含路径
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/../src)
include_directories(${CMAKE_SOURCE_DIR}/../src/core/interfaces)
include_directories(${CMAKE_SOURCE_DIR}/mocks)
include_directories(${CMAKE_SOURCE_DIR}/helpers)

# 定义测试数据目录
set(FIXTURES_DIR "${CMAKE_SOURCE_DIR}/fixtures")
add_definitions(-DFIXTURES_DIR="${FIXTURES_DIR}")

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

# ========== 使用 Mock 的单元测试 ==========

# ========== ComponentService 单元测试（使用 Mock） ==========
add_executable(test_component_service_mock test_component_service_mock.cpp)

# 添加源文件（只包含 Service 和 Model，不包含真实的 API）
target_sources(test_component_service_mock PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentService.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
)

target_link_libraries(test_component_service_mock Qt6::Core Qt6::Test Qt6::Network)

# ========== ExportService 单元测试（使用 Mock） ==========
add_executable(test_export_service_mock test_export_service_mock.cpp)

# 添加源文件（只包含 Service 和 Model，不包含真实的导出器）
target_sources(test_export_service_mock PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentExportTask.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/GeometryUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/LayerMapper.cpp
)

target_link_libraries(test_export_service_mock Qt6::Core Qt6::Test)

# ========== 测试覆盖率报告 ==========
if(ENABLE_COVERAGE)
    # 添加测试目标
    enable_testing()

    # 注册所有测试
    add_test(NAME test_component_service COMMAND test_component_service)
    add_test(NAME test_export_service COMMAND test_export_service)
    add_test(NAME test_config_service COMMAND test_config_service)
    add_test(NAME test_component_data_collector COMMAND test_component_data_collector)
    add_test(NAME test_component_service_mock COMMAND test_component_service_mock)
    add_test(NAME test_export_service_mock COMMAND test_export_service_mock)

    # 添加覆盖率报告生成目标
    find_program(GCOVR gcovr)
    if(GCOVR)
        add_custom_target(coverage
            COMMAND ${GCOVR}
                --xml
                --output coverage.xml
                --exclude-throw-branches
                --exclude-unreachable-branches
                --print-summary
                --html-details
                --html-title "EasyKiConverter Coverage Report"
                -r ${CMAKE_SOURCE_DIR}/../
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating code coverage report..."
            VERBATIM
        )

        add_custom_target(coverage-clean
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/coverage
            COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/coverage.xml
            COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/*.gcda
            COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/*.gcno
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_SOURCE_DIR}/../src/*.gcda
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_SOURCE_DIR}/../src/*.gcno
            COMMENT "Cleaning coverage data..."
            VERBATIM
        )

        message(STATUS "Coverage reports will be generated in: ${CMAKE_BINARY_DIR}/coverage/")
        message(STATUS "Run: cmake --build . --target coverage")
    else()
        message(WARNING "gcovr not found. Coverage reports will not be generated.")
    endif()
endif()