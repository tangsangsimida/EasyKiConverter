# EasyKiConverter 测试套件

该目录包含了 EasyKiConverter 项目的所有测试代码和配置。我们使用 **QtTest** 框架进行 C++ 开发过程中的质量保证。

## 目录结构

- `unit/`: 单元测试项目。针对核心逻辑（Core）、模型（Models）和服务（Services）进行独立验证。
  - `test_easyeda_api.cpp`: 验证与 EasyEDA 服务器交互的客户端逻辑（支持 Mock）。
- `common/`: 测试辅助类和通用 Mock 对象。
  - `MockNetworkAdapter.hpp`: 用于拦截网络请求的模拟适配器。
- `reports/`: (Git 忽略) 用于存放构建过程中生成的 XML/HTML 测试报告。

## 如何运行测试

### 1. 启用测试构建
在项目根目录下通过 CMake 配置时开启 `EASYKICONVERTER_BUILD_TESTS` 选项：

```powershell
cmake -B build -S . -DEASYKICONVERTER_BUILD_TESTS=ON
```

### 2. 构建并执行
使用 CMake 命令直接运行特定的测试目标：

```powershell
# 仅构建并运行特定的测试
cmake --build build --target test_easyeda_api
.\build\bin\test_easyeda_api.exe
```

或者使用 CTest 运行所有已注册的测试：

```powershell
cd build
ctest --output-on-failure
```

## 测试规范

1. **依赖注入**：所有涉及 IO（网络、文件系统）的类应通过接口（如 `INetworkAdapter`）接受依赖，以便在测试中注入 Mock。
2. **异步测试**：对于涉及信号槽的测试，务必使用 `QSignalSpy::wait()` 配合超时处理，严禁使用同步循环检查。
3. **隔离性**：每个测试用例应在 `init()` 中重置 Mock 状态，在 `cleanup()` 中执行必要的资源回收。

---
更多详细技术细节请参考 [项目开发手册：测试指南](../docs/developer/TESTING_GUIDE.md)。
