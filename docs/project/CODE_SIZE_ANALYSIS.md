# 代码文件规模分析报告

> 分析日期：2026-07-17 | 工具：`/tmp/analyze_code_size.py`

## 概述

本报告基于行数分析项目源码文件的规模分布，识别需要拆分或重构的过长文件。为后续 [ADR 012: IR 架构重构](adr/012-intermediate-representation-refactor.md) 和代码质量改进提供依据。

| 类型 | 文件数 | 过长 | 偏长 | 健康率 |
|------|--------|------|------|--------|
| 头文件 (.h) | 132 | 3 (2%) | 8 (6%) | 92% |
| 源文件 (.cpp) | 143 | 3 (2%) | 31 (22%) | 76% |
| QML 文件 | 52 | 7 (13%) | 12 (23%) | 63% |
| Python 脚本 | 12 | 3 (25%) | 5 (42%) | 33% |
| CMake 文件 | 21 | 2 (10%) | 2 (10%) | 81% |
| **合计** | **360** | **18** | **29** | -- |

阈值：头文件 warn=300/fail=500，源文件 warn=500/fail=1000，QML warn=300/fail=500，Python warn=500/fail=800，CMake warn=200/fail=400。

---

## 一、头文件问题清单

### 过长（>500 行）

| 文件 | 总行数 | 代码行 | 注释行 | 问题分析 |
|------|--------|--------|--------|---------|
| `src/services/ComponentCacheService.h` | 591 | 122 | 380 | 注释占比 64%，Doxygen 文档过重或接口声明过多 |
| `src/services/ComponentService.h` | 534 | 142 | 315 | 同上，服务接口过大 |
| `src/core/network/INetworkClient.h` | 523 | 386 | 90 | 纯代码行过多，接口定义臃肿 |

### 偏长（300-500 行）

| 文件 | 总行数 | 代码行 | 说明 |
|------|--------|--------|------|
| `src/models/SymbolData.h` | 491 | 323 | 结构体过多，IR 重构后自动拆分 |
| `src/services/ConfigService.h` | 406 | 77 | 注释 266 行，文档比代码重 |
| `src/models/FootprintData.h` | 403 | 278 | IR 重构后自动拆分 |
| `src/utils/CommandLineParser.h` | 390 | 101 | 注释 228 行 |
| `src/services/export/ExportProgress.h` | 320 | 171 | 含 ExportOptions 等多个结构体 |

### 处理建议

- `SymbolData.h`、`FootprintData.h`：ADR 012 IR 重构时自然解决，无需单独处理
- `ComponentCacheService.h`、`ComponentService.h`：拆分接口，提取子服务或使用 Pimpl 隐藏实现细节
- `INetworkClient.h`：接口过大，考虑按功能分组（同步/异步/资源类型）拆分为多个接口
- 注释过重的文件：审查 Doxygen 注释是否重复了代码本身已表达的信息

---

## 二、源文件问题清单

### 过长（>1000 行）

| 文件 | 总行数 | 代码行 | 注释行 | 问题分析 |
|------|--------|--------|--------|---------|
| `src/services/ComponentCacheService.cpp` | 1,509 | 1,181 | 118 | 项目最长文件，缓存读写/过期/迁移逻辑全集中 |
| `src/ui/viewmodels/ComponentListViewModel.cpp` | 1,385 | 1,082 | 109 | ViewModel 职责过多：列表管理+搜索+选择+批量操作 |
| `src/services/ComponentService.cpp` | 1,184 | 902 | 117 | 数据获取+解析+缓存+错误处理全在一个类 |

### 偏长（500-1000 行，按行数排序）

| 文件 | 行数 | 说明 |
|------|------|------|
| `src/ui/viewmodels/ExportProgressViewModel.cpp` | 950 | 导出进度管理 |
| `src/services/export/ParallelExportService.cpp` | 946 | 并行导出协调 |
| `src/main.cpp` | 857 | 入口文件混入了 CLI/GUI 切换逻辑 |
| `src/workers/WriteWorker.cpp` | 841 | 文件写入工作线程 |
| `src/core/altium/writers/AltiumPcbLibWriter.cpp` | 757 | PcbLib 二进制写入 |
| `src/models/SymbolDataSerializer.cpp` | 728 | IR 重构后自然解决 |
| `src/services/export/TempFileManager.cpp` | 714 | 临时文件管理 |
| `src/core/kicad/SymbolGraphicsGenerator.cpp` | 691 | KiCad 符号图形生成 |
| `src/core/kicad/ExporterSymbol.cpp` | 686 | KiCad 符号导出 |
| `src/core/kicad/Exporter3DModel.cpp` | 684 | 3D 模型导出 |
| `src/models/FootprintDataSerializer.cpp` | 677 | IR 重构后自然解决 |
| `src/core/kicad/FootprintGraphicsGenerator.cpp` | 664 | KiCad 封装图形生成 |
| `src/ui/viewmodels/ExportSettingsViewModel.cpp` | 622 | 导出设置 ViewModel |
| `src/core/altium/compound/OLECompoundWriter.cpp` | 609 | OLE 二进制写入 |
| `src/core/network/AsyncNetworkRequest.cpp` | 595 | 异步网络请求 |
| `src/services/export/FootprintExportStage.cpp` | 569 | 封装导出阶段 |
| 另有 15 个文件在 500-569 行之间 | -- | -- |

### 处理建议

**ADR 012 IR 重构自动解决的**（6 个文件）：
- `SymbolDataSerializer.cpp`、`FootprintDataSerializer.cpp` -- 序列化逻辑合并到 IR 层
- `SymbolData.h` 相关的模型文件 -- 拆分为 IR + Importer

**需要独立拆分的**：
- `ComponentCacheService.cpp`（1,509 行）：拆分为 `CacheReader` / `CacheWriter` / `CacheMigration` 三个子类
- `ComponentListViewModel.cpp`（1,385 行）：提取 `ComponentSearchManager`、`ComponentSelectionManager` 等子管理器
- `ComponentService.cpp`（1,184 行）：提取 `ComponentFetcher`、`ComponentParser` 等
- `main.cpp`（857 行）：CLI 入口逻辑已迁移到 `CliConverter`，剩余 GUI 初始化可提取为 `ApplicationSetup` 类

**可接受但需关注的**（导出器和写入器）：
- `AltiumPcbLibWriter.cpp`（757 行）：二进制格式写入天然较长，可按原语类型拆分方法但收益有限
- KiCad 导出器系列（各 660-690 行）：与 IR 重构后的导出器接口调整一并处理

---

## 三、QML 文件问题清单（问题最集中）

### 过长（>500 行）

| 文件 | 总行数 | 代码行 | 问题分析 |
|------|--------|--------|---------|
| `src/ui/qml/components/ComponentListCard.qml` | 1,128 | 1,020 | 项目最长 QML，列表+搜索+工具栏+状态管理全在一个文件 |
| `src/ui/qml/MainWindow.qml` | 900 | 789 | 主窗口布局+状态管理+对话框逻辑 |
| `src/ui/qml/components/deprecated/ExportSettingsCard.qml` | 791 | 752 | 已标记 deprecated，可忽略 |
| `src/ui/qml/components/SidebarSettingsView.qml` | 636 | 583 | 侧边栏设置面板 |
| `src/ui/qml/components/ComponentListItem.qml` | 619 | 578 | 单个列表项组件过于复杂 |
| `src/ui/qml/components/SliderDialogBase.qml` | 608 | 530 | 滑动对话框基类 |
| `src/ui/qml/components/ExportSettingsBaseCard.qml` | 533 | 483 | 导出设置卡片基类 |

### 偏长（300-500 行）

| 文件 | 行数 | 说明 |
|------|------|------|
| `HeaderSection.qml` | 402 | |
| `ExitDialog.qml` | 400 | |
| `ResultListItem.qml` | 379 | |
| `tst_ExportFlow.qml` | 341 | 测试文件 |
| `ExportResultsCard.qml` | 306 | |

### 处理建议

QML 文件过长是当前最严重的问题区域（健康率仅 63%），建议按以下优先级处理：

1. **`ComponentListCard.qml`（1,128 行）**：拆分为 `ComponentToolbar`、`ComponentSearchBar`、`ComponentListView` 等子组件
2. **`MainWindow.qml`（900 行）**：提取 `MenuBar`、`StatusBar`、`DialogManager` 等独立 QML 组件
3. **`ComponentListItem.qml`（619 行）**：拆分渲染逻辑为更小的子委托组件
4. **`ExportSettingsCard.qml`（deprecated，791 行）**：确认无引用后直接删除
5. **`SidebarSettingsView.qml`（636 行）**：各设置区块提取为独立组件

---

## 四、工具脚本和 CMake

### Python 工具脚本

| 文件 | 行数 | 优先级 |
|------|------|--------|
| `tools/python/build_project.py` | 1,081 | 低（开发工具，不影响产品） |
| `tools/python/manage_version.py` | 997 | 低 |
| `tools/python/format_code.py` | 890 | 低 |

工具脚本过长但不影响产品质量，可在空闲时逐步拆分为子模块。

### CMake 文件

| 文件 | 行数 | 说明 |
|------|------|------|
| `tests/unit/CMakeLists.txt` | 536 | 测试目标过多，可按模块拆分为子 CMakeLists |
| `CMakeLists.txt` (根) | 424 | 项目配置复杂度所致，可接受 |

---

## 五、与 ADR 012 的关联

以下文件在 IR 重构中会被自然处理，无需单独拆分：

| 文件 | 行数 | IR 重构后的去向 |
|------|------|----------------|
| `src/models/SymbolData.h` | 491 | 拆分到 `ir/SymbolIR.h` + `importers/easyeda/EasyedaMetadata.h` |
| `src/models/FootprintData.h` | 403 | 拆分到 `ir/FootprintIR.h` + `importers/easyeda/EasyedaMetadata.h` |
| `src/models/SymbolDataSerializer.cpp` | 728 | 逻辑合并到 IR 层或删除 |
| `src/models/FootprintDataSerializer.cpp` | 677 | 同上 |
| `src/models/ComponentData.h` | 234 | 合并到 `ir/ComponentIR.h` |
| `src/models/Model3DData.h` | 124 | 合并到 `ir/Model3DIR.h` |

---

## 六、推荐实施顺序

| 阶段 | 内容 | 文件数 | 预估工时 |
|------|------|--------|---------|
| 1 | ADR 012 IR 重构（解决 6 个模型文件） | ~20 | 3 周 |
| 2 | QML 组件拆分（解决 7 个过长 QML） | ~15 | 1-2 周 |
| 3 | 服务层拆分（CacheService/ComponentService/ViewModel） | ~6 | 1 周 |
| 4 | main.cpp 瘦身 | 1 | 0.5 周 |
| 5 | 工具脚本模块化（可选） | 3 | 空闲时 |

---

## 附录：目录行数分布

| 目录 | 总行数 | 文件数 |
|------|--------|--------|
| `src/services` | 15,341 | 71 |
| `src/ui` | 14,708 | 61 |
| `src/core` | 14,626 | 78 |
| `tests/unit` | 7,912 | 38 |
| `tools/python` | 6,262 | 12 |
| `src/utils` | 6,054 | 44 |
| `src/models` | 4,578 | 20 |
| `src/workers` | 2,696 | 12 |
