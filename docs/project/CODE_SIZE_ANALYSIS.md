# 代码文件规模分析报告

> 分析日期：2026-09-18 | 工具：`python3 tools/python/analyze_project.py`

## 概述

本报告基于行数分析项目源码文件的规模分布，识别需要拆分或重构的过长文件。为后续 [ADR 012: IR 架构重构](adr/012-intermediate-representation-refactor.md) 和代码质量改进提供依据。

| 类型 | 文件数 | 过长 | 偏长 | 健康率 |
|------|--------|------|------|--------|
| 产品源码与资源 | 501 | 53 | 39 | -- |
| Python 工具 | 13 | 5 | 4 | -- |
| **合计** | **514** | **58** | **43** | -- |

当前统计工具按文件总行数使用统一阈值：高风险 >500 行，中风险 300-500 行，低风险 200-300 行。类型分项和代码/注释行数需要额外脚本才能精确拆分，因此本报告不再保留旧的推算健康率。

当前基线：`src` 441 个文件、77,645 行；`tests` 58 个文件、18,725 行；翻译资源 2 个文件、3,303 行；`tools/python` 13 个文件、6,640 行。项目工具的 `--all` 统计覆盖产品源码、测试和翻译资源，工具目录单独统计后合计 514 个文件、106,313 行。

---

## 一、头文件问题清单

### 过长（>500 行）

| 文件 | 总行数 | 代码行 | 注释行 | 问题分析 |
|------|--------|--------|--------|---------|
| `src/services/ComponentCacheService.h` | 584 | 122 | 366 | 注释占比 63%，Doxygen 文档过重或接口声明过多 |
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
| `src/services/ComponentCacheService.cpp` | 685 | -- | -- | 缓存生命周期和二级缓存接口仍集中，缓存路径、目录切换、预览图下载、元数据、3D 文件、CAD 数据、预览图写入、数据手册文件存储、数据手册下载、缓存维护、配额清理和一级内存缓存语义已提取 |
| `src/services/CacheDirectoryCoordinator.cpp` | 71 | -- | -- | 独立协调缓存目录切换、旧目录迁移、缓存代次失效、目录初始化和自修复，保持 ComponentCacheService 的公开接口与锁顺序 |
| `src/services/CachePathResolver.cpp` | 66 | -- | -- | 独立解析组件目录、元数据、预览图、数据手册和三维模型路径，并执行输入校验 |
| `src/services/PreviewImageDownloadService.cpp` | 119 | -- | -- | 独立协调预览图缓存命中、网络请求、取消、诊断和代次保护写入 |
| `src/services/ComponentCacheMetadataWriter.cpp` | 51 | -- | -- | 独立协调元数据快照合并、代次校验和 L1/L2 提交 |
| `src/services/ComponentCacheCadDataWriter.cpp` | 86 | -- | -- | 独立协调符号、封装和 CAD JSON 的校验、原子写入、配额维护及 L1 同步 |
| `src/services/ComponentCachePreviewImageWriter.cpp` | 52 | -- | -- | 独立协调预览图校验、代次/tombstone 检查、原子写入和配额维护 |
| `src/services/DatasheetCacheFileStore.cpp` | 88 | -- | -- | 独立承载数据手册缓存读取、格式校验、格式切换、旧文件清理和原子写入 |
| `src/services/ComponentCacheMaintenance.cpp` | 130 | -- | -- | 独立协调缓存删除、全量清理、L1 清理、有效组件枚举和磁盘大小统计 |
| `src/services/ComponentCacheQuotaEnforcer.cpp` | 40 | -- | -- | 独立协调磁盘缓存限制、冷却策略、目录清理和缓存大小信号 |
| `src/services/ComponentCacheMemoryStore.cpp` | 92 | -- | -- | 独立承载一级缓存复合键、元数据 JSON、CAD 数据校验、LRU 数据访问和容量统计 |
| `src/services/ComponentCacheModel3DCoordinator.cpp` | 91 | -- | -- | 独立协调三维模型缓存的锁边界、代次校验、文件读写、导出复制和磁盘配额触发 |
| `src/ui/viewmodels/ComponentListViewModel.cpp` | 779 | -- | -- | ViewModel 职责过多：列表管理+搜索+选择；批量导入、列表数据回调、预览更新缓冲、列表项更新缓冲、验证队列状态、验证错误分类、定时器初始化和服务信号连接已提取 |
| `src/ui/viewmodels/ComponentListTimerCoordinator.cpp` | 73 | -- | -- | 独立创建并连接预览图、批处理、列表统计和延迟获取定时器，保持防抖窗口及原有回调顺序 |
| `src/ui/viewmodels/ComponentListServiceConnectionCoordinator.cpp` | 91 | -- | -- | 独立连接验证完成、组件数据、预览图成功/失败和批量预览完成信号，保持图片编码、缓存更新和验证状态推进顺序 |
| `src/ui/viewmodels/ComponentValidationCoordinator.cpp` | 81 | -- | -- | 独立协调验证队列的启动、并发调度、完成回调和 BOM 导入状态推进 |
| `src/ui/viewmodels/ComponentListDataCoordinator.cpp` | 166 | -- | -- | 独立协调组件基础信息、CAD、LCSC、数据手册和错误回调到列表项状态的转换 |
| `src/ui/viewmodels/ComponentListBatchCoordinator.cpp` | 189 | -- | -- | 独立协调批量添加、模型行插入、BOM 解析回调和验证启动时序 |
| `src/services/ComponentService.cpp` | 487 | -- | -- | 数据获取和服务编排仍在服务内，基础信息字段解析、BOM 文本编号提取、缓存加载任务、预览图文件编码、CAD 结果收敛、媒体回调、API 回调、队列初始化、并行批量协调、CAD 响应协调和请求取消已提取 |
| `src/services/ComponentRequestCancellationCoordinator.cpp` | 58 | -- | -- | 独立协调全量请求和单器件请求的 API、网络客户端、图片服务取消及状态清理顺序 |
| `src/services/ComponentMediaCallbackCoordinator.cpp` | 274 | -- | -- | 独立协调预览图、LCSC 数据和数据手册回调，保持缓存代次校验、异步写入、并行队列完成通知和服务信号转发顺序 |
| `src/services/ComponentApiCallbackCoordinator.cpp` | 85 | -- | -- | 独立协调 EasyEDA API 基础信息和错误回调，保持缓存代次校验、失败状态清理、并行错误通知和服务信号转发顺序 |
| `src/services/ComponentCacheLoadCoordinator.cpp` | 145 | -- | -- | 独立协调后台缓存读取、缓存代次校验、网络回退、结果合并和缓存结果信号转发 |
| `src/services/ComponentCadFetchCoordinator.cpp` | 93 | -- | -- | 独立协调 CAD 响应的后台解析、缓存代次校验、结果提交和并行批处理通知 |

### 偏长（500-1000 行，按行数排序）

| 文件 | 行数 | 说明 |
|------|------|------|
| `src/ui/viewmodels/ExportProgressViewModel.cpp` | 868 | 导出进度管理；结果列表状态计算、过滤统计和重试状态初始化已提取 |
| `src/ui/viewmodels/ExportProgressResultsCoordinator.cpp` | 179 | 独立协调导出结果列表的状态计算、过滤、类型成功统计和重试重置 |
| `src/services/export/ParallelExportService.cpp` | 843 | 并行导出协调；导出计划计算和进度状态聚合已提取 |
| `src/services/export/ExportRunPlan.cpp` | 57 | 独立计算导出类型、可导出元件和缺失缓存数据，保持服务启动阶段无副作用 |
| `src/services/export/ExportProgressAggregator.cpp` | 65 | 独立合并阶段状态、重算阶段计数和汇总最终元件结果 |
| `src/main.cpp` | 859 | 入口文件混入了 CLI/GUI 切换逻辑 |
| `src/workers/WriteWorker.cpp` | 846 | 文件写入工作线程 |
| `src/core/altium/writers/AltiumPcbLibWriter.cpp` | 1,193 | PcbLib 二进制写入 |
| `src/core/altium/writers/AltiumSchLibWriter.cpp` | 761 | SchLib 主记录写入；来源顺序调度、文本记录、图片记录、引脚记录、几何图元、文件级头部、字体表、参数、图片 Storage、组件 Data 流、几何校验和部件归属校验已提取 |
| `src/core/altium/writers/AltiumSchPinRecordWriter.cpp` | 61 | 独立承载 SchLib `RECORD=2` 二进制引脚记录编码，并复用主写入器的 Owner 校验与内容序号状态 |
| `src/core/altium/writers/AltiumSchPrimitiveRecordWriter.cpp` | 273 | 独立承载矩形、弧线、多边形、折线、Bezier 和 IEEE 等文本参数图元记录编码 |
| `src/core/altium/writers/AltiumSchComponentRecordWriter.cpp` | 265 | 独立承载 Designator、参数字段、实现关系、引脚映射和实现参数记录编码 |
| `src/core/altium/writers/AltiumSchTextRecordWriter.cpp` | 142 | 独立承载 SchLib 文本与文本框记录编码 |
| `src/core/altium/writers/AltiumSchImageRecordWriter.cpp` | 54 | 独立承载 SchLib 图片记录编码 |
| `src/core/altium/writers/AltiumSchLibraryHeaderWriter.cpp` | 105 | 独立承载 SchLib FileHeader 和 SectionKeys 文件级流编码 |
| `src/core/altium/writers/AltiumSchImageStorageWriter.cpp` | 101 | 独立承载嵌入图片名称校验、重复命名消解和 Storage 流编码 |
| `src/core/altium/writers/AltiumSchComponentStorageWriter.cpp` | 145 | 独立承载单个组件 Data 流的记录顺序协调和 OLE 存储写入 |
| `src/core/altium/writers/AltiumSchOwnershipValidator.cpp` | 48 | 独立承载 SchLib 图元和参数记录的 OWNERPARTID 范围校验与诊断 |
| `src/core/altium/writers/AltiumSchGeometryValidator.cpp` | 192 | 独立承载 SchLib 图元的坐标、尺寸、方向、字符串和控制点约束校验 |
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
- `ComponentCacheService.cpp`（685 行）：已提取缓存文件布局、目录迁移、目录切换、元数据存储、组件数据读取、二进制文件读取、CAD 数据写入、预览图读取/下载/写入、数据手册文件存储、数据手册下载、缓存维护、磁盘配额清理、写入代次策略、数据校验、tombstone、L1 内存缓存语义、元数据写入协调和三维模型缓存协调职责，后续继续按锁边界拆分管理流程
- `CacheDirectoryCoordinator.cpp`（71 行）：独立承载缓存目录切换、旧目录迁移、缓存代次失效、目录初始化和自修复，ComponentCacheService 继续负责公开接口、信号转发和协调器生命周期
- `PreviewImageDownloadService.cpp`（119 行）：独立承载预览图缓存命中、网络重试、取消检查、诊断回写和代次保护写入，ComponentCacheService 继续保留公开接口
- `CachePathResolver.cpp`（66 行）：独立承载缓存路径构造、旧目录大小写兼容和模型路径安全校验，ComponentCacheService 继续负责锁和目录生命周期
- `ComponentCacheCadDataWriter.cpp`（86 行）：统一承载符号、封装和 CAD JSON 的磁盘写入流程，保持目录迁移锁、代次校验、配额维护和内存缓存同步顺序
- `Model3DCacheFileStore.cpp`（64 行）：独立承载三维模型文件的校验读取、原子写入和导出复制，ComponentCacheService 继续负责锁、路径、代次和配额策略
- `ComponentCacheModel3DCoordinator.cpp`（91 行）：独立协调三维模型缓存的锁边界、代次校验、文件读写、导出复制和磁盘配额触发，ComponentCacheService 继续保留公开接口和路径策略
- `ComponentListViewModel.cpp`（779 行）：已提取列表状态统计、编号索引、批量导入、组件服务数据回调、预览更新缓冲、列表项更新缓冲、验证队列状态、验证错误分类、定时器初始化、服务信号连接和验证队列协调职责，并移除失效的预览图编码线程池，后续继续拆分搜索和选择
- `ComponentListTimerCoordinator.cpp`（73 行）：独立创建并连接预览图、批处理、列表统计和延迟获取定时器，保持防抖窗口及原有回调顺序
- `ComponentListServiceConnectionCoordinator.cpp`（91 行）：独立连接验证完成、组件数据、预览图成功/失败和批量预览完成信号，保持图片编码、缓存更新和验证状态推进顺序
- `ComponentValidationCoordinator.cpp`（81 行）：独立协调验证队列启动、并发调度、完成回调和 BOM 导入状态推进，保持 ViewModel 的状态更新与服务请求顺序
- `ComponentListDataCoordinator.cpp`（166 行）：独立协调基础信息、CAD、LCSC、数据手册和错误回调，保持列表项状态更新、验证完成通知和批量刷新顺序
- `ComponentListBatchCoordinator.cpp`（189 行）：独立协调批量编号去重、分批插入、BOM 异步解析和验证启动，保持模型通知与状态计数顺序
- `ComponentListItemUpdateBuffer.cpp`（约 31 行）：独立收集异步基础信息更新涉及的列表项，负责去重、线程安全转移和清理
- `ComponentCachePreviewImageWriter.cpp`（52 行）：独立承载预览图二级缓存写入，保持校验、目录迁移锁、代次策略、原子替换和配额维护顺序
- `DatasheetCacheFileStore.cpp`（88 行）：独立承载数据手册缓存读取、格式校验、格式切换、旧文件清理和原子写入
- `DatasheetDownloadService.cpp`（约 150 行）：独立承载数据手册缓存命中、同步网络下载、取消处理、网络诊断和代次保护写入
- `ComponentCacheMaintenance.cpp`（130 行）：独立承载缓存删除、全量清理、L1 清理、有效组件枚举和磁盘大小统计，保持代次、tombstone、锁和信号顺序

**CLI 重复逻辑收敛**：
- `CliExportWaiter.cpp`：统一 BatchConverter 和 BomConverter 的预加载/导出事件循环等待逻辑，转换器自身继续负责输入读取、结果统计和错误处理
- `ComponentCacheQuotaEnforcer.cpp`（40 行）：独立承载磁盘缓存配额的冷却判断、目标容量计算、清理执行和大小变化通知
- `ComponentCacheMemoryStore.cpp`（92 行）：独立承载一级缓存复合键、JSON 编解码、CAD 数据校验、LRU 访问和容量统计；ComponentCacheService 继续负责信号、代次和磁盘流程
- `ComponentValidationErrorPolicy.cpp`（65 行）：集中维护 CAD 数据失败、不可重试错误和预览图错误分类规则，保持 ViewModel 的验证状态与界面提示行为一致
- `ComponentService.cpp`（487 行）：已提取组件数据内存缓存、基础信息字段解析、BOM 文本编号提取、后台缓存加载、预览图文件编码、CAD 结果收敛、媒体回调、API 回调、队列初始化、并行批量协调、CAD 响应协调和请求取消职责，后续继续按异步请求状态边界拆分获取与错误处理
- `ComponentRequestCancellationCoordinator.cpp`（58 行）：独立协调全量请求和单器件请求的 API、网络客户端、图片服务取消及状态清理顺序，保持 ComponentService 的公开取消接口不变
- `ComponentMediaCallbackCoordinator.cpp`（274 行）：独立协调预览图、LCSC 数据和数据手册的异步回调，保持缓存代次校验、异步写入、并行队列完成通知和服务信号转发顺序
- `ComponentApiCallbackCoordinator.cpp`（85 行）：独立协调 EasyEDA API 基础信息和错误回调，保持缓存代次校验、失败状态清理、并行错误通知和服务信号转发顺序
- `ComponentCacheLoadCoordinator.cpp`（145 行）：独立协调后台缓存读取、缓存代次校验、网络回退、结果合并和缓存结果信号转发
- `ComponentParallelFetchCoordinator.cpp`（约 100 行）：独立协调并行上下文完成/失败回调、队列槽位释放、超时通知和批量状态重置
- `ComponentValidationCoordinator.cpp`（81 行）：独立协调列表验证队列的并发调度、完成状态、BOM 导入更新和后续队列推进
- `ComponentCadFetchCoordinator.cpp`（93 行）：独立协调 CAD 响应解析、请求代次校验、缓存提交和并行批处理通知
- `PreviewImageDataEncoder.cpp`（47 行）：独立承载预览图文件序号解析、文件读取和 Base64 编码，避免 ComponentService 同时承担文件处理细节
- `main.cpp`（859 行）：CLI 入口逻辑已迁移到 `CliConverter`，剩余 GUI 初始化可提取为 `ApplicationSetup` 类

**可接受但需关注的**（导出器和写入器）：
- `AltiumSchLibWriter.cpp`（761 行）：二进制格式写入天然较长，来源顺序调度、文本记录、图片记录、引脚记录、几何图元、文件级头部、字体表、参数、图片 Storage、组件 Data 流、几何校验和部件归属校验已提取，后续可按组件级文件流程继续拆分
- `AltiumSchPinRecordWriter.cpp`（61 行）：独立编码二进制引脚记录，保持引脚方向、可见性、名称/编号和连接属性的原有写入顺序
- `AltiumSchPrimitiveRecordWriter.cpp`（273 行）：独立编码几何图元文本参数，复用主写入器的坐标、Owner、唯一标识和诊断策略
- `AltiumSchComponentRecordWriter.cpp`（265 行）：独立编码参数和实现关系记录，复用主写入器的字体、编号、库名和诊断状态
- `AltiumSchLibraryHeaderWriter.cpp`（105 行）：独立承载 SchLib FileHeader 和 SectionKeys 流，保持字体表、组件计数、记录权重和存储键映射的一致性
- `AltiumSchImageStorageWriter.cpp`（101 行）：独立承载嵌入图片名称校验、大小写不敏感的重复命名消解、255 字节限制和 OLE Storage 流编码
- `AltiumSchComponentStorageWriter.cpp`（145 行）：独立承载单个组件 Data 流的默认/来源顺序选择、补充图元以及参数和实现记录的协调
- `AltiumSchOwnershipValidator.cpp`（48 行）：独立校验所有 SchLib 记录的 OWNERPARTID 范围并复用主写入器诊断通道，保持拒绝写入行为不变
- `AltiumSchGeometryValidator.cpp`（192 行）：独立校验图元几何边界、编码字符串和枚举范围，并复用主写入器诊断通道，保持拒绝写入行为不变
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
| `src` | 76,693 | 415 |
| `tests` | 18,632 | 58 |
| `resources/translations` | 3,303 | 2 |
| `tools/python` | 6,642 | 13 |

> 目录级统计以上述项目工具的当前输出为准；旧版按子目录手工估算的数据已移除，避免与总量不一致。当前输出由 `python3 tools/python/analyze_project.py --all --json` 生成。
