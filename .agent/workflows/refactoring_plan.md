# C++ 高耦合文件重构计划

## 概述

通过代码行数分析，识别出 14 个超过 500 行的高耦合 C++ 文件。
本计划按优先级分批进行重构，**只做代码拆分，不改变现有功能和接口**。

---

## 第一批：Exporter 导出器（按图形元素类型拆分）

### 1. `ExporterSymbol.cpp` (1298 行 → 目标 ~350 行)

**分析**：25 个函数，其中可按职责分为 4 类：
- **导出入口** (36-343行)：`exportSymbol`, `exportSymbolLibrary`, `generateHeader`, `generateSymbolContent`, `generateSubSymbol` × 2
- **图形生成器** (680-1148行)：`generatePin`, `generateRectangle`, `generateCircle`, `generateArc`, `generateEllipse`, `generatePolygon`, `generatePolyline`, `generatePath`, `generateText`
- **单位转换** (1150-1213行)：`pxToMil`, `pxToMm`, `pinTypeToKicad`, `pinStyleToKicad`, `rotationToKicadOrientation`
- **工具函数** (1215-1296行)：`calculatePartBBox`

**拆分方案**：
| 新文件 | 职责 | 预估行数 |
|--------|------|---------|
| `ExporterSymbol.cpp` | 导出入口 + 内容组装 | ~350 |
| `SymbolGraphicsGenerator.cpp/.h` | 所有图形元素生成 (Pin/Rect/Circle/Arc/Ellipse/Polygon/Polyline/Path/Text) | ~500 |
| `SymbolExportUtils.cpp/.h` | 单位转换 + 类型映射 + BBox 计算 | ~200 |

### 2. `ExporterFootprint.cpp` (1104 行 → 目标 ~350 行)

**分析**：26 个函数，结构与 ExporterSymbol 类似：
- **导出入口** (13-144行)：`exportFootprint` × 2, `exportFootprintLibrary`, `generateHeader`
- **内容生成** (146-388行)：`generateFootprintContent` × 2
- **图形生成器** (390-962行)：`generatePad`, `generateTrack`, `generateHole`, `generateCircle`, `generateRectangle`, `generateArc`, `generateText`, `generateModel3D`, `generateSolidRegion`, `generateCourtyardFromBBox`
- **工具函数** (874-1102行)：`pxToMm`, `pxToMmRounded`, `padShapeToKicad`, `padTypeToKicad`, `padLayersToKicad`, `layerIdToKicad`

**拆分方案**：
| 新文件 | 职责 | 预估行数 |
|--------|------|---------|
| `ExporterFootprint.cpp` | 导出入口 + 内容组装 | ~350 |
| `FootprintGraphicsGenerator.cpp/.h` | 图形元素生成 | ~400 |
| `FootprintExportUtils.cpp/.h` | 单位转换 + 层映射 + Pad 类型映射 | ~200 |

---

## 第二批：Importer 导入器（按数据类型拆分）

### 3. `EasyedaImporter.cpp` (1015 行 → 目标 ~300 行)

**分析**：29 个函数，可按导入的数据类型分组：
- **符号导入** (14-265行)：`importSymbolData` + 符号图形解析 (`importPinData`, `importRectangleData`, `importCircleData`, `importArcData`, `importEllipseData`, `importPolylineData`, `importPolygonData`, `importPathData`, `importTextData`)
- **封装导入** (266-439行)：`importFootprintData` + 封装图形解析 (`importPadData`, `importTrackData`, `importHoleData`, `importFootprintCircleData`, `importFootprintRectangleData`, `importFootprintArcData`, `importFootprintTextData`, `importSolidRegionData`, `importSvgNodeData`)
- **工具函数** (795-1014行)：`parseDataString`, `parsePinDataString`, `parseLayerDefinition`, `parseObjectVisibility`, `stringToBool`

**拆分方案**：
| 新文件 | 职责 | 预估行数 |
|--------|------|---------|
| `EasyedaImporter.cpp` | 入口协调 + 工具函数 | ~300 |
| `EasyedaSymbolImporter.cpp/.h` | 符号数据解析 | ~350 |
| `EasyedaFootprintImporter.cpp/.h` | 封装数据解析 | ~350 |

---

## 第三批：数据模型（按结构体拆分到独立文件）

### 4. `SymbolData.cpp` (984 行) + `SymbolData.h` (522 行)

**分析**：62 个函数，全部是 `toJson()/fromJson()` 序列化方法，分属 15+ 个结构体。

**拆分方案**：保持单文件不变（这些是纯数据结构的序列化代码，耦合度低，按结构体组织清晰，不建议拆分）。

### 5. `FootprintData.cpp` (711 行) + `FootprintData.h` (416 行)

**分析**：同上，47 个函数，全部是数据模型序列化。

**拆分方案**：同上，保持不变。

---

## 第四批：服务层

### 6. `ExportService_Pipeline.cpp` (776 行)

**分析**：20 个函数，职责明确：
- 管线管理：`executeExportPipelineWithStages`, `cleanupPipeline`, `cancelExport`
- 阶段处理：`startFetchStage`, `startProcessStage`, `startWriteStage`
- 完成回调：`handleFetchCompleted`, `handleProcessCompleted`, `handleWriteCompleted`, `checkPipelineCompletion`
- 统计报告：`generateStatistics`, `saveStatisticsReport`, `mergeSymbolLibrary`

**拆分方案**：
| 新文件 | 职责 | 预估行数 |
|--------|------|---------|
| `ExportService_Pipeline.cpp` | 管线管理 + 阶段调度 | ~450 |
| `ExportStatisticsGenerator.cpp/.h` | 统计生成 + 报告保存 + 符号库合并 | ~250 |

### 7. `ExportService.cpp` (703 行)

**分析**：24 个函数，有两套导出逻辑（串行和并行），存在一定的重复：
- 串行导出：`executeExportPipelineWithData`, `processNextExport`
- 并行导出：`executeExportPipelineWithDataParallel`, `handleParallelExportTaskFinished`

**拆分方案**：暂不拆分（两套逻辑互为对照，拆分反而降低可读性）。

### 8. `ExportProgressViewModel.cpp` (716 行)

**分析**：25 个函数，但包含系统托盘初始化（87行）和通知逻辑（87行），与进度管理无关。

**拆分方案**：
| 新文件 | 职责 | 预估行数 |
|--------|------|---------|
| `ExportProgressViewModel.cpp` | 进度管理 + 导出控制 | ~540 |
| `SystemTrayManager.cpp/.h` | 系统托盘初始化 + 通知 | ~180 |

---

## 第五批：工具类

### 9. `GeometryUtils.cpp` (678 行)

**分析**：纯工具函数集合，耦合度低，按功能分组合理。暂不拆分。

### 10. `WriteWorker.cpp` (663 行) / `FetchWorker.cpp` (608 行)

**分析**：Worker 类本身封装性好，各函数职责单一。暂不拆分。

---

## 执行优先级

| 优先级 | 文件 | 预期收益 | 复杂度 |
|--------|------|---------|--------|
| P0 | `ExporterSymbol.cpp` | 🔴 最大文件，拆分后结构清晰 | 中 |
| P0 | `ExporterFootprint.cpp` | 🔴 结构同上，可复用方案 | 中 |
| P1 | `EasyedaImporter.cpp` | 🟡 按数据类型拆分，逻辑独立 | 中 |
| P1 | `ExportProgressViewModel.cpp` | 🟡 分离系统托盘，降低职责 | 低 |
| P2 | `ExportService_Pipeline.cpp` | 🟢 分离统计生成 | 低 |
| -- | `SymbolData/FootprintData` | 不拆分（纯序列化代码） | -- |
| -- | `ExportService/Workers/Utils` | 不拆分（已封装良好） | -- |

---

## 注意事项

1. **不改变公共 API**：所有拆分都是内部实现重组，头文件中的公共接口不变
2. **CMakeLists.txt 更新**：每次拆分后需要将新 .cpp 文件添加到 `SOURCES` 列表
3. **增量验证**：每拆分完一个文件就编译验证，确保无编译错误
4. **命名规范**：新文件遵循项目现有命名风格（PascalCase）
