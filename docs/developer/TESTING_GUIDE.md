# 测试开发指南 (Testing Guide)

本文档详细介绍了 EasyKiConverter 的测试架构、Mock 策略及如何编写新的测试用例。

## 1. 测试架构概览

我们采用分层测试策略：
- **单元测试 (Unit Tests)**: 针对最小可测试单元（函数、类）进行隔离验证。
- **集成测试 (Integration Tests)**: 验证多个模块间的协同（如从网络获取数据到 KiCad 文件生成的完整链路）。
- **QML/UI 测试**: 使用 `QtQuickTest` 验证视图逻辑。

## 2. 依赖注入与 Mock 策略

为了实现可测试性，核心组件（如 `EasyedaApi`）不应直接使用具体的 IO 类（如 `NetworkUtils`），而是通过接口进行交互。

### INetworkAdapter 接口
所有的网络操作都定义在 `INetworkAdapter` 接口中。

```cpp
// 示例：注入适配器
EasyedaApi api(new MockNetworkAdapter());
```

### 使用 MockNetworkAdapter
在测试中，你可以通过 `MockNetworkAdapter` 预设 API 的返回结果：

```cpp
QJsonObject mockData;
mockData.insert("success", true);
mockAdapter->addJsonResponse("https://api.url", mockData);
```

## 3. 编写异步测试的关键点

### QSignalSpy 的正确姿势
对于异步信号提取，务必使用 `wait()` 方法而不是死循环：

```cpp
QSignalSpy spy(api, &EasyedaApi::componentInfoFetched);
api->fetchComponentInfo("C123");

// 必须使用 wait 等待事件循环处理
QVERIFY2(spy.wait(2000), "信号未在 2s 内触发");
QCOMPARE(spy.count(), 1);
```

## 4. CMake 测试配置

单元测试在 `tests/unit/CMakeLists.txt` 中定义。添加新测试时，请确保：
1. 使用 `target_include_directories` 包含必要的 `src` 层级。
2. 将对应的 Mock 头文件加入 `add_executable` 以便 `AUTOMOC` 正确生成符号。

## 5. 持续集成 (CI)

GitHub Actions 会自动执行所有启用了测试的构建。如果单元测试不通过，CI 流程将会失败并阻止合并。

---
如有疑问，请咨询核心开发团队。
