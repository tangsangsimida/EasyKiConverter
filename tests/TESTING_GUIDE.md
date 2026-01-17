# Service 层单元测试指南

## 测试概述

本文档描述了 Service 层的单元测试策略和实施计划。

## 测试文件

### 1. ComponentService 测试 (test_component_service.cpp)

**测试目标**: 验证 ComponentService 的核心功能

**测试用例**:
- `testCreation()`: 验证服务对象创建
- `testFetchComponentData()`: 验证元件数据获取接口
- `testSetOutputPath()`: 验证输出路径设置和获取
- `testSignals()`: 验证所有信号定义正确

**当前状态**: ✅ 测试代码已编写

**注意事项**:
- 需要网络连接才能完整测试
- 实际测试需要 mock EasyedaApi
- 当前测试只验证接口调用不会崩溃

### 2. ExportService 测试 (test_export_service.cpp)

**测试目标**: 验证 ExportService 的导出功能

**测试用例**:
- `testCreation()`: 验证服务对象创建
- `testExportOptions()`: 验证导出选项结构体
- `testExecuteExportPipeline()`: 验证导出管道接口
- `testSignals()`: 验证所有信号定义正确

**当前状态**: ✅ 测试代码已编写

**注意事项**:
- 需要实际的元件数据才能完整测试
- 当前测试只验证接口调用不会崩溃

### 3. ConfigService 测试 (test_config_service.cpp)

**测试目标**: 验证 ConfigService 的配置管理功能

**测试用例**:
- `testCreation()`: 验证服务对象创建
- `testLoadConfig()`: 验证配置加载
- `testSaveConfig()`: 验证配置保存
- `testGetSetConfig()`: 验证配置获取和设置
- `testSignals()`: 验证信号定义正确

**当前状态**: ✅ 测试代码已编写

**注意事项**:
- 配置文件操作可能需要权限
- 测试会创建临时配置文件

### 4. ComponentDataCollector 测试 (test_component_data_collector.cpp)

**测试目标**: 验证 ComponentDataCollector 的状态机逻辑

**测试用例**:
- `testCreation()`: 验证收集器对象创建
- `testStateMachine()`: 验证状态机状态
- `testStart()`: 验证启动数据收集
- `testCancel()`: 验证取消数据收集
- `testSignals()`: 验证所有信号定义正确

**当前状态**: ✅ 测试代码已编写

**注意事项**:
- 测试状态转换逻辑
- 需要网络连接才能完整测试

## 编译问题

### 当前遇到的挑战

1. **包含路径问题**: 源文件中使用 `src/` 前缀的包含路径在测试环境中无法正确解析
2. **依赖关系复杂**: Service 层依赖多个模块（Core、Models、Workers）
3. **AUTOMOC 配置**: Qt 的元对象编译器需要正确配置

### 解决方案

#### 方案 1: 修复包含路径（推荐）

修改所有源文件中的包含路径，移除 `src/` 前缀：

```cpp
// 修改前
#include "src/services/ComponentService.h"

// 修改后
#include "services/ComponentService.h"
```

**优点**:
- 彻底解决问题
- 符合标准项目结构

**缺点**:
- 需要修改大量文件
- 可能影响主项目编译

#### 方案 2: 使用 CMake 包含路径（临时方案）

在测试的 CMakeLists.txt 中添加所有必要的包含路径：

```cmake
include_directories(${CMAKE_SOURCE_DIR}/../src)
include_directories(${CMAKE_SOURCE_DIR}/../src/core/easyeda)
include_directories(${CMAKE_SOURCE_DIR}/../src/core/kicad)
# ... 等等
```

**优点**:
- 不需要修改源代码
- 快速实施

**缺点**:
- 不够优雅
- 维护困难

#### 方案 3: 集成到主项目（最佳方案）

将测试集成到主项目的 CMakeLists.txt 中：

```cmake
# 在主项目 CMakeLists.txt 中添加
enable_testing()
add_subdirectory(tests)

# 添加测试
add_test(NAME ComponentServiceTest COMMAND test_component_service)
add_test(NAME ExportServiceTest COMMAND test_export_service)
add_test(NAME ConfigServiceTest COMMAND test_config_service)
add_test(NAME ComponentDataCollectorTest COMMAND test_component_data_collector)
```

**优点**:
- 统一管理
- 自动处理依赖
- 符合标准实践

**缺点**:
- 需要重构测试构建系统

## 测试运行指南

### 方案 1: 手动运行测试（当前）

```bash
cd tests/build
./test_component_service
./test_export_service
./test_config_service
./test_component_data_collector
```

### 方案 2: 使用 CTest（推荐）

```bash
cd build
ctest --verbose
```

## 下一步计划

1. **短期目标**:
   - 修复包含路径问题
   - 成功编译所有测试
   - 运行基础测试

2. **中期目标**:
   - 添加 mock 对象
   - 实现完整的单元测试
   - 添加测试覆盖率报告

3. **长期目标**:
   - 集成到 CI/CD 流程
   - 实现自动化测试
   - 添加性能测试

## 测试覆盖率目标

- **Service 层**: 目标 80% 覆盖率
- **ViewModel 层**: 目标 70% 覆盖率
- **Model 层**: 目标 90% 覆盖率

## 注意事项

1. **网络依赖**: 部分测试需要网络连接，考虑使用 mock
2. **文件系统**: 配置测试会创建临时文件，需要清理
3. **异步操作**: 信号槽测试需要使用 QSignalSpy 和 QTest::qWait
4. **资源管理**: 测试前后需要正确创建和销毁对象

## 参考资源

- [Qt Test Documentation](https://doc.qt.io/qt-6/qtest.html)
- [CMake Testing](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
- [Unit Testing Best Practices](https://github.com/google/googletest/blob/main/docs/primer.md)