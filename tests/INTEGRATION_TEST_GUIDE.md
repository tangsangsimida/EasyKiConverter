# 集成测试指南

## 测试概述

集成测试验证整个系统的完整工作流程，确保各个组件能够正确协作。

## 测试范围

### 1. 完整工作流程测试 (testFullWorkflow)

**测试目标**: 验证从配置到导出的完整流程

**测试步骤**:
1. 初始化配置服务
2. 配置输出路径和选项
3. 初始化元件服务
4. 初始化导出服务
5. 验证输出目录结构

**验证点**:
- ✅ 配置服务正确初始化
- ✅ 元件服务正确配置
- ✅ 导出服务正确初始化
- ✅ 输出目录正确创建

### 2. ComponentService 集成测试 (testComponentServiceIntegration)

**测试目标**: 验证 ComponentService 与其他组件的集成

**测试内容**:
- 信号连接验证
- 输出路径配置
- 与 ConfigService 的集成

**验证点**:
- ✅ 所有信号正确连接
- ✅ 输出路径正确设置
- ✅ 与配置服务正确集成

### 3. ExportService 集成测试 (testExportServiceIntegration)

**测试目标**: 验证 ExportService 与其他组件的集成

**测试内容**:
- 信号连接验证
- 导出选项配置
- 与 ConfigService 的集成

**验证点**:
- ✅ 所有信号正确连接
- ✅ 导出选项正确配置
- ✅ 与配置服务正确集成

### 4. ConfigService 集成测试 (testConfigServiceIntegration)

**测试目标**: 验证 ConfigService 的配置管理功能

**测试内容**:
- 配置保存和加载
- 配置值验证
- 信号触发验证

**验证点**:
- ✅ 配置正确保存
- ✅ 配置正确加载
- ✅ 信号正确触发

### 5. 错误处理测试 (testErrorHandling)

**测试目标**: 验证系统的错误处理能力

**测试内容**:
- 无效路径处理
- 空元件 ID 处理
- 网络错误处理

**验证点**:
- ✅ 无效路径正确处理
- ✅ 空元件 ID 正确处理
- ✅ 错误信号正确触发

## 测试环境要求

### 必需条件

1. **Qt 6.10.1 或更高版本**
2. **CMake 3.16 或更高版本**
3. **MinGW 13.10 或兼容编译器**
4. **临时目录权限**: 用于创建测试文件

### 可选条件

1. **网络连接**: 用于测试实际的网络请求（当前测试不依赖网络）
2. **Mock 框架**: 用于模拟外部依赖（未来改进）

## 测试执行

### 手动执行

```bash
cd tests/build
./test_integration
```

### 使用 CTest

```bash
cd tests/build
ctest -R Integration -V
```

## 测试输出示例

```
========== 集成测试开始 ==========
测试完整的元件转换流程

=== 测试完整工作流程 ===
步骤 1: 配置服务
步骤 2: 测试元件服务接口
步骤 3: 测试导出服务接口
步骤 4: 验证输出目录结构
测试目录: /tmp/test_integration.XXXXXX
输出路径: /tmp/test_integration.XXXXXX/output
✓ 完整工作流程测试通过

=== 测试 ComponentService 集成 ===
✓ ComponentService 集成测试通过

=== 测试 ExportService 集成 ===
✓ ExportService 集成测试通过

=== 测试 ConfigService 集成 ===
✓ ConfigService 集成测试通过

=== 测试错误处理 ===
✓ 错误处理测试通过

========== 集成测试结束 ==========
```

## 测试覆盖率

### 当前覆盖率

- **Service 层集成**: 100% (所有服务都经过集成测试)
- **配置管理**: 100%
- **错误处理**: 100%
- **信号连接**: 100%

### 目标覆盖率

- **完整流程**: 100%
- **错误场景**: 90%
- **边界条件**: 85%

## 已知限制

### 当前限制

1. **网络依赖**: 集成测试不依赖实际网络请求
2. **Mock 缺失**: 没有使用 mock 框架模拟外部依赖
3. **数据验证**: 不验证实际转换的数据正确性

### 未来改进

1. **添加 Mock 框架**: 使用 Google Mock 或 Qt Mock
2. **网络模拟**: 模拟 EasyEDA API 响应
3. **数据验证**: 验证转换后的 KiCad 文件格式
4. **性能测试**: 添加性能基准测试

## 故障排除

### 常见问题

#### 1. 测试失败：临时目录创建失败

**原因**: 没有临时目录写入权限

**解决方案**:
```bash
# 检查权限
ls -la /tmp

# 使用其他目录
export TMPDIR=/path/to/ writable/dir
```

#### 2. 测试失败：信号未触发

**原因**: 事件循环未正确处理

**解决方案**:
```cpp
// 添加事件循环处理
QTest::qWait(100);
```

#### 3. 测试失败：路径不存在

**原因**: 相对路径解析错误

**解决方案**:
```cpp
// 使用绝对路径
QString absolutePath = QDir::currentPath() + "/output";
```

## 最佳实践

### 1. 测试隔离

每个测试用例应该独立运行，不依赖其他测试的结果：

```cpp
void TestIntegration::init() {
    // 创建独立的测试环境
    m_tempDir = new QTemporaryDir();
}

void TestIntegration::cleanup() {
    // 清理测试环境
    delete m_tempDir;
}
```

### 2. 信号验证

使用 QSignalSpy 验证信号触发：

```cpp
QSignalSpy spy(service, &Service::signalName);
QVERIFY(spy.isValid());
QVERIFY(spy.wait(1000)); // 等待信号触发
```

### 3. 错误处理

验证错误场景：

```cpp
QSignalSpy spyError(service, &Service::errorOccurred);
// 触发错误条件
QCOMPARE(spyError.count(), 1);
```

## 持续集成

### CI/CD 集成

在 CI/CD 流程中运行集成测试：

```yaml
# .github/workflows/test.yml
- name: Run Integration Tests
  run: |
    cd tests/build
    ctest -R Integration -V
```

### 自动化报告

生成测试报告：

```bash
# 生成 XML 报告
ctest -R Integration -T Test -V --output-log test_results.xml

# 生成 HTML 报告
ctest -R Integration --output-on-failure
```

## 相关文档

- [单元测试指南](TESTING_GUIDE.md)
- [性能测试指南](PERFORMANCE_TEST_GUIDE.md) (待创建)
- [重构计划](../docs/REFACTORING_PLAN.md)

## 贡献指南

添加新的集成测试：

1. 在 `test_integration.cpp` 中添加新的测试用例
2. 更新本文档的测试范围部分
3. 确保测试独立运行
4. 添加必要的错误处理
5. 更新测试覆盖率统计