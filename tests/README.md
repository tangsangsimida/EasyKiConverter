# EasyKiConverter 测试套件

该目录包含了 EasyKiConverter 项目的所有测试代码和配置。我们使用 **QtTest** 框架进行 C++ 开发过程中的质量保证。

## 当前测试组成

当前测试由 CTest 统一调度，包含：

- `unit/`: C++ 单元测试项目。针对核心逻辑（Core）、模型（Models）、服务（Services）、CLI 和 ViewModel 进行独立验证。
  - `test_easyeda_api.cpp`: 验证与 EasyEDA 服务器交互的客户端逻辑（支持 Mock）。
  - `test_core_utils.cpp`: URL、gzip、KiCad 表转义和图层映射等核心工具测试。
  - `test_atomic_file_writer.cpp`: 原子写入和拷贝工具测试。
  - `test_temp_file_manager.cpp`: 导出临时文件提交和清理测试。
  - `test_export_report_generator.cpp`: 导出详细诊断报告生成测试。
  - `test_command_line_parser.cpp`: 命令行参数解析器测试。
  - `test_completion_generator.cpp`: Shell 补全脚本生成器测试。
  - `test_file_reader.cpp`: 文件读取工具测试。
  - `test_bom_parser.cpp`: BOM/LCSC 编号解析测试。
  - `test_kicad_library_table_manager.cpp`: KiCad 项目库表注册测试。
- `integration/`: 跨模块集成测试，覆盖 fixture 数据导入到 KiCad 文件输出的离线路径。
- `ui/`: UI/QML 自动化测试。
- `benchmark/`: 基准测试，用于观察序列化、反序列化等核心路径的性能趋势。
- `manual/`: 手动测试清单，供测试人员按步骤执行回归验证。
  - `WINDOW_STATE_MANUAL_CHECKLIST.md`: 主窗口显示、最大化恢复、窗口化恢复和位置持久化测试清单。
  - `CLI_TEST_MANUAL.md`: CLI 模式手动测试手册，包含完整的测试用例和步骤。
- `common/`: 测试辅助类和通用 Mock 对象。
  - `MockNetworkClient.hpp`: 用于拦截统一网络客户端请求的模拟实现。
- `reports/`: (Git 忽略) 用于存放构建过程中生成的 XML/HTML 测试报告。
- `fixtures/`: EasyEDA 响应、CAD JSON、BOM 文件等输入样例。
- `golden/`: KiCad 输出、库表和报告等期望结果，用于稳定的 golden-file 对比。

## 已自动化的导出 GUI 回归流程

以下原本需要手动点选的导出相关流程已经纳入自动化：

- 导出按钮状态：无元器件、未选择导出类型、导出中禁用、停止导出按钮显示。
- 导出参数传递：元器件 ID、输出目录、库名、符号/封装/3D、预览图、数据手册、覆盖、更新模式、调试模式和库描述字段。
- 导出进度展示：空闲隐藏、导出中显示、百分比取整、状态文案联动。
- 导出结果列表：结果列表显示、全部/导出中/成功/失败筛选、失败项重试和删除回调。
- 关闭窗口流程：导出中关闭弹出强制退出确认，空闲关闭弹出退出选项，已有对话框时不重复弹出，`Esc` 关闭/打开正确提示。
- 并行导出管线：fixture 数据成功导出、缺失预加载数据失败、导出中取消。
- ViewModel 状态：取消时 pending/in_progress 项转为失败，批量重试只重置失败项。

## 如何运行测试

推荐使用项目构建脚本：

```bash
python3 tools/python/build_project.py --test
```

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

也可以按标签分层运行：

```bash
cd build
ctest -L fast --output-on-failure         # 单元测试 + QML UI 快速回归
ctest -L export-flow --output-on-failure  # 导出 GUI/集成流程
ctest -L integration --output-on-failure  # 跨模块集成测试
ctest -L slow --output-on-failure         # benchmark 等较慢测试
```

在 CI、WSL 或无桌面环境中运行 Qt/QML 测试时，需要设置：

```bash
export QT_QPA_PLATFORM=offscreen
export QT_QPA_PLATFORMTHEME=none
```

### 3. 手动测试

窗口状态、系统托盘、真实文件管理器打开、桌面映射、任务栏可见性这类场景目前不适合完全依赖 headless 自动化测试。执行相关回归时，请参考：

- [窗口状态手动测试清单](./manual/WINDOW_STATE_MANUAL_CHECKLIST.md)
- [CLI 模式手动测试手册](./manual/CLI_TEST_MANUAL.md)

## CI 分层建议

- PR 快速校验：`ctest -L fast --output-on-failure`，覆盖大部分单元和 QML 交互回归。
- 导出相关改动：额外运行 `ctest -L export-flow --output-on-failure`。
- 发布前/夜间：运行完整 `ctest --output-on-failure`，包含 benchmark 和全量集成测试。
- 所有 headless UI/CI 场景都应设置 `QT_QPA_PLATFORM=offscreen` 和 `QT_QPA_PLATFORMTHEME=none`，避免 Qt 在无显示环境中加载 GTK 平台主题。

## Golden 文件策略

KiCad 输出格式、符号/封装结构和 3D 模型引用使用 `tests/golden/kicad/` 下的期望文件做稳定对比。新增导出格式或修复 KiCad 兼容性问题时，优先补 golden 测试；只有确认输出格式变更符合预期后，才更新 golden 文件。

## 测试规范

1. **依赖注入**：所有涉及 IO（网络、文件系统）的类应通过接口（如 `INetworkClient`）接受依赖，以便在测试中注入 Mock。
2. **异步测试**：对于涉及信号槽的测试，务必使用 `QSignalSpy::wait()` 配合超时处理，严禁使用同步循环检查。
3. **隔离性**：每个测试用例应在 `init()` 中重置 Mock 状态，在 `cleanup()` 中执行必要的资源回收。
---
更多详细技术细节、补测优先级和路线图请参考 [测试指南](../docs/developer/TESTING_GUIDE.md)。
