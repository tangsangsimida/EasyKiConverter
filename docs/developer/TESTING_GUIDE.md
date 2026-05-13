# 测试开发指南 (Testing Guide)

本文档定义 EasyKiConverter 的测试架构、补测优先级、Mock 策略和新增测试要求。目标是让测试既能快速反馈，又能覆盖转换正确性、网络韧性和导出流水线这几类高风险路径。

## 1. 当前测试基线

当前自动化测试由 `ctest` 统一调度，主要包括：

- `tests/unit/`: C++ 单元测试，覆盖模型、配置、缓存、CLI 参数、日志、部分服务和 ViewModel。
- `tests/ui/`: QML/QtQuickTest 测试，覆盖基础组件、主题和部分绑定行为。
- `tests/benchmark/`: Qt benchmark，用于序列化/反序列化等核心路径的性能观察。
- `tests/manual/`: 暂不适合自动化的人工回归清单，例如窗口状态、真实桌面环境和 CLI 人工验收。

本地验证优先使用项目脚本：

```bash
python3 tools/python/build_project.py --test
```

CI 和 headless 环境运行 Qt/QML 测试时必须设置：

```bash
export QT_QPA_PLATFORM=offscreen
```

## 2. 分层测试架构

测试应按风险和反馈速度分层，不同层级承担不同职责：

| 层级 | 目录 | 目标 | 约束 |
|------|------|------|------|
| Unit | `tests/unit/` | 验证单个类、纯函数、状态机、序列化、参数解析 | 不访问真实网络；文件 IO 使用 `QTemporaryDir` |
| Integration | `tests/integration/` | 验证多模块协作，例如 Mock 网络到 KiCad 文件输出 | 使用固定 fixture 和 golden file；不依赖外部服务 |
| UI/QML | `tests/ui/` | 验证 QML 组件属性、绑定、关键交互状态 | 使用 `QtQuickTest`；headless 下可运行 |
| Benchmark | `tests/benchmark/` | 观察核心路径性能退化 | 不作为功能正确性的唯一依据 |
| Manual | `tests/manual/` | 覆盖系统托盘、真实窗口管理器、安装包等环境相关场景 | 每次发布前按清单执行 |

建议后续新增 `tests/fixtures/` 和 `tests/golden/`：

- `tests/fixtures/easyeda/`: EasyEDA API 响应、CAD JSON、异常响应样例。
- `tests/fixtures/bom/`: CSV/XLSX BOM 样例，覆盖列名变化、空行、重复项和非法 ID。
- `tests/golden/kicad/`: 期望生成的 `.kicad_sym`、`.kicad_mod`、`.wrl`、`.step`、`fp-lib-table` 片段。

测试中读取这些资产时优先使用 `tests/common/TestPaths.hpp`，不要依赖当前工作目录。该 helper 通过 `EASYKICONVERTER_TEST_SOURCE_DIR` 定位 `tests/` 根目录，并提供 fixture/golden 路径解析、UTF-8 文本读取、换行归一化和 golden 文本比较能力。

## 3. 建设性改进建议

### 3.1 建立覆盖率报告

CMake 已提供 `ENABLE_COVERAGE`，可通过以下方式生成覆盖率报告：

**方式一：通过构建脚本（推荐）**

```bash
python3 tools/python/build_project.py --coverage
```

这会自动启用 `ENABLE_COVERAGE=ON`、`EASYKICONVERTER_BUILD_TESTS=ON`，并在构建和测试完成后调用 `generate_coverage.py` 生成报告。

**方式二：独立脚本**

```bash
# 完整流程（配置 → 构建 → 测试 → 报告）
python3 tools/python/generate_coverage.py

# 仅生成报告（需已有 .gcda 文件）
python3 tools/python/generate_coverage.py --report-only

# 额外生成 XML 报告
python3 tools/python/generate_coverage.py --xml
```

**报告工具优先级**：gcovr > lcov+genhtml。如缺工具会给出明确安装提示：

```bash
pip install gcovr            # 推荐
sudo apt install lcov        # 备选
```

**输出目录**：`tests/reports/coverage/`（HTML: `index.html`，XML: `coverage.xml`）

**排除范围**：`build/`、`tests/`、第三方依赖、Qt 自动生成文件（moc_*, qrc_*, *_autogen/*）等。

覆盖率门槛建议分阶段推进：

| 阶段 | 总体行覆盖率 | 核心模块目标 | 要求 |
|------|--------------|--------------|------|
| Baseline | 只记录不阻断 | 只记录不阻断 | 先建立可信数据 |
| Phase 1 | 不低于 baseline - 2% | 网络、导入、导出模块持续提升 | PR 不允许明显回退 |
| Phase 2 | 60%+ | 核心转换链路 75%+ | CI 可作为合并门禁 |

### 3.2 补齐高风险模块测试

优先级按故障影响排序：

| 优先级 | 模块 | 需要补充的测试 |
|--------|------|----------------|
| P0 | `NetworkClient` / `AsyncNetworkRequest` | 并发限制、超时、重试、429 回退、弱网络降级、单例销毁后重建 |
| P0 | EasyEDA importer | Symbol/Footprint/CAD JSON fixture 解析、缺字段、非法图形、单位转换 |
| P0 | KiCad exporter | golden-file 输出对比、库表更新、特殊字符转义、3D 模型路径模式 |
| P0 | Export pipeline | 部分成功/失败、取消、跳过已存在文件、临时文件清理、报告生成 |
| P1 | CLI convert | `convert bom/component/batch` 真实命令流，验证退出码和输出文件 |
| P1 | BOM parser | CSV/XLSX 列名别名、重复料号、空单元格、非法 LCSC ID |
| P1 | Cache service | 损坏缓存、迁移、磁盘限额、并发读写、预览图缓存 |
| P1 | QML workflow | 导入、列表编辑、导出按钮状态、错误提示、设置持久化 |
| P2 | Performance | 大 BOM、大 CAD JSON、批量导出的 benchmark 趋势 |

### 3.3 强化测试可维护性

新增测试应遵守以下要求：

- 测试名描述行为，不描述实现，例如 `testRetryAfterRateLimit()`。
- 每个测试只验证一个主要行为；复杂流程拆成 fixture/helper。
- 使用 `QTemporaryDir`，不要写入用户目录、项目根目录或固定绝对路径。
- 禁止真实 HTTP 请求；网络相关测试必须通过 `MockNetworkClient` 或本地可控 fake。
- 异步测试必须使用 `QSignalSpy::wait()`，不要轮询或忙等。
- 对 KiCad 输出优先使用 golden file；必要时先做归一化再比较，避免时间戳、路径分隔符导致误报。
- 新增回归修复必须带最小失败用例，除非该场景只能人工验证。

## 4. 依赖注入与 Mock 策略

为了实现可测试性，核心组件不应直接绑定具体 IO 实现。涉及网络、文件、时间、系统状态的逻辑应通过接口或可替换依赖注入。

### INetworkClient 接口

所有网络操作都通过 `INetworkClient` 接口定义，测试中使用 Mock 实现。

```cpp
NetworkClient::destroyInstance();
auto mock = std::make_unique<MockNetworkClient>();
EasyedaApi api(mock.get());
```

### 使用 MockNetworkClient

测试中应预设完整响应，并断言请求 URL、请求次数和错误路径：

```cpp
QJsonObject mockData;
mockData.insert("success", true);
m_mockClient->addJsonResponse(expectedUrl, mockData);
```

涉及 `NetworkClient` 单例的测试必须先调用：

```cpp
NetworkClient::destroyInstance();
```

## 5. 异步测试要求

对于信号槽、线程池、网络回调和 QML 事件，务必使用事件循环友好的等待方式：

```cpp
QSignalSpy spy(api, &EasyedaApi::componentInfoFetched);
api->fetchComponentInfo("C123");

QVERIFY2(spy.wait(2000), "componentInfoFetched was not emitted within 2s");
QCOMPARE(spy.count(), 1);
```

禁止使用同步忙等、无限等待或依赖固定长时间 `qWait()` 的测试。短 `qWait()` 只可用于验证取消、防抖等确实需要时间窗口的行为，并必须说明原因。

## 6. CMake 测试配置

单元测试在 `tests/unit/CMakeLists.txt` 中定义。添加新测试时：

1. 使用 `target_include_directories` 包含必要的 `src` 和 `tests` 层级。
2. 将对应 Mock 头文件加入 `add_executable`，确保 `AUTOMOC` 能正确处理。
3. 使用 `add_test(NAME test_xxx COMMAND test_xxx)` 注册到 CTest。
4. UI 测试应保证 headless 环境可运行。
5. 后续建议为测试设置 CTest labels，例如 `unit`、`integration`、`ui`、`benchmark`，便于 CI 分组执行。

## 7. CI 与发布质量门槛

每个 PR 至少应满足：

- Linux、Windows、macOS 构建通过。
- `ctest --output-on-failure --no-tests=error` 通过。
- 新增生产逻辑有对应单元测试或集成测试。
- 涉及导出格式、网络、缓存、CLI 的变更必须补回归测试或说明只能人工验证的原因。

发布前额外执行：

- `tests/manual/CLI_TEST_MANUAL.md`
- `tests/manual/WINDOW_STATE_MANUAL_CHECKLIST.md`
- 一次带代表性 BOM 的端到端转换验证。

## 8. 推荐补测路线图

短期优先：

1. 建立覆盖率报告脚本，先记录 baseline。
2. 新增 `tests/fixtures/` 和 `tests/golden/`，为 importer/exporter 测试准备稳定样例。
3. 补 `NetworkClient` 的限流、重试、超时和销毁重建测试。
4. 补 KiCad exporter golden-file 测试，覆盖符号、封装、3D 模型路径。

中期推进：

1. 新增 `tests/integration/`，覆盖 Mock EasyEDA 数据到 KiCad 文件输出的完整链路。
2. 补 CLI `convert bom/component/batch` 集成测试。
3. 扩展 QML 测试到主流程状态变化和错误提示。
4. 为大 BOM 和批量导出建立 benchmark 趋势记录。
