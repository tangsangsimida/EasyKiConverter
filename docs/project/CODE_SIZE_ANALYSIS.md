# 代码文件规模分析报告

> 分析日期：2026-09-18 | 工具：`python3 tools/python/analyze_project.py`

## 概述

本报告基于行数分析项目源码文件的规模分布，识别需要拆分或重构的过长文件。为后续 [ADR 012: IR 架构重构](adr/012-intermediate-representation-refactor.md) 和代码质量改进提供依据。

| 类型 | 文件数 | 过长 | 偏长 | 健康率 |
|------|--------|------|------|--------|
| 产品源码与资源 | 423 | 54 | 37 | -- |
| Python 工具 | 13 | 5 | 4 | -- |
| **合计** | **436** | **59** | **41** | -- |

当前统计工具按文件总行数使用统一阈值：高风险 >500 行，中风险 300-500 行，低风险 200-300 行。类型分项和代码/注释行数需要额外脚本才能精确拆分，因此本报告不再保留旧的推算健康率。

当前基线：`src` 363 个文件、75,038 行；`tests` 58 个文件、18,588 行；翻译资源 2 个文件、3,303 行；`tools/python` 13 个文件、6,642 行。项目工具的 `--all` 统计覆盖产品源码、测试和翻译资源，工具目录单独统计后合计 436 个文件、103,571 行。

---

## 一、头文件问题清单

### 过长（>500 行）

| 文件 | 总行数 | 代码行 | 注释行 | 问题分析 |
|------|--------|--------|--------|---------|
| `src/services/ComponentCacheService.h` | 577 | 122 | 366 | 注释占比 63%，Doxygen 文档过重或接口声明过多 |
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
| `src/services/ComponentCacheService.cpp` | 1,371 | -- | -- | 缓存读写/过期/迁移逻辑仍集中，已通过辅助职责拆分逐步收敛 |
| `src/ui/viewmodels/ComponentListViewModel.cpp` | 1,296 | 1,005 | 155 | ViewModel 职责过多：列表管理+搜索+选择+批量操作；预览更新缓冲和验证队列状态已提取 |
| `src/services/ComponentService.cpp` | 1,201 | -- | -- | 数据获取+缓存+错误处理仍在服务内，基础信息字段解析、BOM 文本编号提取和缓存加载任务已提取 |

### 偏长（500-1000 行，按行数排序）

| 文件 | 行数 | 说明 |
|------|------|------|
| `src/ui/viewmodels/ExportProgressViewModel.cpp` | 950 | 导出进度管理 |
| `src/services/export/ParallelExportService.cpp` | 917 | 并行导出协调 |
| `src/main.cpp` | 859 | 入口文件混入了 CLI/GUI 切换逻辑 |
| `src/workers/WriteWorker.cpp` | 846 | 文件写入工作线程 |
| `src/core/altium/writers/AltiumPcbLibWriter.cpp` | 1,193 | PcbLib 二进制写入 |
| `src/core/altium/writers/AltiumSchLibWriter.cpp` | 1,793 | SchLib 主记录写入；来源顺序调度、文本记录、图片记录、字体表和图片 Storage 编码已提取 |
| `src/core/altium/writers/AltiumSchTextRecordWriter.cpp` | 142 | 独立承载 SchLib 文本与文本框记录编码 |
| `src/core/altium/writers/AltiumSchImageRecordWriter.cpp` | 54 | 独立承载 SchLib 图片记录编码 |
| `src/models/SymbolDataSerializer.cpp` | 842 | IR 重构后自然解决 |
| `src/services/export/TempFileManager.cpp` | 804 | 临时文件管理 |
| `src/core/kicad/SymbolGraphicsGenerator.cpp` | 443 | KiCad 符号图形生成 |
| `src/core/kicad/ExporterSymbol.cpp` | 768 | KiCad 符号导出 |
| `src/core/kicad/Exporter3DModel.cpp` | 702 | 3D 模型导出 |
| `src/models/FootprintDataSerializer.cpp` | 697 | IR 重构后自然解决 |
| `src/core/kicad/FootprintGraphicsGenerator.cpp` | 543 | KiCad 封装图形生成 |
| `src/ui/viewmodels/ExportSettingsViewModel.cpp` | 685 | 导出设置 ViewModel |
| `src/core/altium/compound/OLECompoundWriter.cpp` | 918 | OLE 二进制写入 |
| `src/core/network/AsyncNetworkRequest.cpp` | 669 | 异步网络请求 |
| `src/services/export/FootprintExportStage.cpp` | 758 | 封装导出阶段 |
| 其余高风险文件 | -- | 请以 `analyze_project.py --all --json` 的当前输出为准 |

### 处理建议

**ADR 012 IR 重构自动解决的**（6 个文件）：
- `SymbolDataSerializer.cpp`、`FootprintDataSerializer.cpp` -- 序列化逻辑合并到 IR 层
- `SymbolData.h` 相关的模型文件 -- 拆分为 IR + Importer

**需要独立拆分的**：
- `ComponentCacheService.cpp`（1,371 行）：已提取缓存文件布局、目录迁移、元数据存储、组件数据读取、二进制文件读取、写入代次策略、数据校验、tombstone 和 L1 存储职责，后续继续按锁边界拆分读写协调逻辑
- `ComponentListViewModel.cpp`（1,296 行）：已提取列表状态统计、编号索引、预览更新缓冲和验证队列状态职责，并移除失效的预览图编码线程池，后续继续拆分搜索、选择和批量操作
- `ComponentService.cpp`（1,201 行）：已提取组件数据内存缓存、基础信息字段解析、BOM 文本编号提取和后台缓存加载职责，后续继续按异步请求状态边界拆分获取与错误处理
- `main.cpp`（857 行）：CLI 入口逻辑已迁移到 `CliConverter`，剩余 GUI 初始化可提取为 `ApplicationSetup` 类

**可接受但需关注的**（导出器和写入器）：
- `AltiumSchLibWriter.cpp`（1,793 行）：二进制格式写入天然较长，来源顺序调度、文本记录、图片记录、字体表和图片 Storage 编码已提取，后续可按记录类型继续拆分
- `AltiumPcbLibWriter.cpp`（1,193 行）：二进制格式写入天然较长，可按原语类型拆分方法但收益有限
- KiCad 导出器系列（各 660-690 行）：与 IR 重构后的导出器接口调整一并处理

---

## 三、QML 文件问题清单（问题最集中）

### 已完成拆分的列表组件

| 文件 | 总行数 | 代码行 | 问题分析 |
|------|--------|--------|---------|
| `src/ui/qml/components/ComponentListCard.qml` | 371 | -- | 卡片状态协调、过滤触发和弹窗定位；预览弹窗、工具栏和列表视图已提取 |
| `src/ui/qml/components/ComponentToolbar.qml` | 427 | -- | 独立承载数量、筛选、搜索和批量操作 |
| `src/ui/qml/components/ComponentListView.qml` | 187 | -- | 独立承载 DelegateModel、网格委托和可见区域预取 |
| `src/ui/qml/components/ComponentPreviewPopup.qml` | 320 | -- | 独立承载预览图展示、缩略图切换和延迟隐藏 |
| `src/ui/qml/MainWindow.qml` | 911 | -- | 主窗口布局+状态管理+对话框逻辑 |
| `src/ui/qml/components/deprecated/ExportSettingsCard.qml` | 791 | 752 | 已标记 deprecated，可忽略 |
| `src/ui/qml/components/SidebarSettingsView.qml` | 724 | -- | 侧边栏设置面板 |
| `src/ui/qml/components/ComponentListItem.qml` | 629 | -- | 单个列表项组件过于复杂 |
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

QML 文件过长是当前最严重的问题区域，建议按以下优先级处理：

1. **`ComponentListCard.qml`（371 行）**：已提取预览弹窗、工具栏和列表视图，后续重点转向 ViewModel 与服务层拆分
2. **`MainWindow.qml`（911 行）**：提取 `MenuBar`、`StatusBar`、`DialogManager` 等独立 QML 组件
3. **`ComponentListItem.qml`（629 行）**：拆分渲染逻辑为更小的子委托组件
4. **`ExportSettingsCard.qml`（deprecated，791 行）**：确认无引用后直接删除
5. **`SidebarSettingsView.qml`（724 行）**：各设置区块提取为独立组件

---

## 四、工具脚本和 CMake

### Python 工具脚本

| 文件 | 行数 | 优先级 |
|------|------|--------|
| `tools/python/manage_version.py` | 1,190 | 低（开发工具，不影响产品） |
| `tools/python/build_project.py` | 1,092 | 低 |
| `tools/python/format_code.py` | 926 | 低 |

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
| `src` | 74,915 | 359 |
| `tests` | 18,588 | 58 |
| `resources/translations` | 3,303 | 2 |
| `tools/python` | 6,642 | 13 |

> 目录级统计以上述项目工具的当前输出为准；旧版按子目录手工估算的数据已移除，避免与总量不一致。当前输出由 `python3 tools/python/analyze_project.py --all --json` 生成。
