# ADR-011: 重试导出丢失已有库内容修复

## 状态
已接受 (Accepted)

## 日期
2026-05-08

## 上下文

在导出 BOM 后，部分元器件可能失败（如网络超时、服务器无 3D 模型等）。用户点击"重试失败组件"后，导出的 KiCad 库中**仅包含重试的组件**，之前已成功导出的组件全部丢失。

### 问题根因

重试流程将**仅失败的组件 ID** 传递给整个导出流水线，库级别的导出阶段（Symbol、Footprint）只处理这些 ID，生成的输出仅包含重试的组件，然后替换磁盘上已有的库文件。

**封装（Footprint）丢失路径**：
1. `retryFailedComponents()` 将失败 ID 传给 `startPreload()` — `ExportProgressViewModel.cpp:728`
2. `ParallelExportService::startPreload()` 用子集覆盖 `m_componentIds` — `ParallelExportService.cpp:79`
3. `startExport()` 仅将这些 ID 传给 `FootprintExportStage::start()` — `ParallelExportService.cpp:326`
4. `FootprintExportStage::doLibraryExport()` 创建**新的空临时目录**，仅写入重试的封装 — `FootprintExportStage.cpp:428,448`
5. `TempFileManager::commitDirectory()` **删除整个已有 `.pretty` 目录**（`removeRecursively()`），替换为仅含重试封装的临时目录 — `TempFileManager.cpp:205`

**符号（Symbol）丢失路径**：
- 当 `overwriteExistingFiles=true` 时，临时文件从空开始，仅写入重试的符号，已有符号全部丢失 — `SymbolExportStage.cpp:223`

**不受影响的类型**：
- 3D 模型（`.wrl`、`.step`）、预览图、数据手册：每个组件独立文件，仅替换重试的组件文件。

### 关键代码位置

| 文件 | 行号 | 说明 |
|------|------|------|
| `ExportProgressViewModel.cpp` | 693-730 | `retryFailedComponents()` 仅传递失败 ID |
| `ExportProgressViewModel.cpp` | 667-691 | `retryComponent()` 仅传递单个 ID |
| `ParallelExportService.cpp` | 79 | `m_componentIds = componentIds` 覆盖组件列表 |
| `FootprintExportStage.cpp` | 427-449 | 临时目录从空开始，仅写入当前批次封装 |
| `TempFileManager.cpp` | 203-209 | `commitDirectory()` 调用 `removeRecursively()` |
| `SymbolExportStage.cpp` | 223 | 条件复制已有符号库 |

## 决策

在 `ExportOptions` 中添加 `retryMode` 标志，告知库级别导出阶段在重试时保留已有输出。

### 修复 1：添加 `retryMode` 标志

**文件**：`src/services/export/ExportProgress.h`

```cpp
struct ExportOptions {
    // ...
    bool overwriteExistingFiles = false;
    bool retryMode = false;  // 重试模式：保留已有库内容，仅更新重试的组件
    // ...
};
```

### 修复 2：重试入口设置标志

**文件**：`src/ui/viewmodels/ExportProgressViewModel.cpp`

在 `retryComponent()` 和 `retryFailedComponents()` 中，启动预加载前设置标志：

```cpp
if (!m_isExporting && m_exportService) {
    ExportOptions opts = m_exportService->options();
    opts.retryMode = true;
    m_exportService->setOptions(opts);
    // ... startPreload ...
}
```

### 修复 3：封装导出阶段 — 重试时复制已有封装

**文件**：`src/services/export/FootprintExportStage.cpp`

在创建临时目录之后、调用 `exportFootprintLibrary()` 之前，重试模式下将已有封装复制到临时目录：

```cpp
// 重试模式：将已有封装复制到临时目录，保留之前成功的组件
if (m_options.retryMode && QDir(finalDir).exists()) {
    if (!QDir().mkpath(tempDirPath)) {
        // 错误处理...
        return;
    }
    const QStringList existingFiles = QDir(finalDir).entryList({"*.kicad_mod"}, QDir::Files);
    for (const QString& file : existingFiles) {
        if (!QFile::copy(finalDir + QDir::separator() + file, tempDirPath + QDir::separator() + file)) {
            qWarning() << "FootprintExportStage: Failed to copy existing footprint:" << file;
        }
    }
}
```

**关键细节**：`createTempDirectoryPath()` 仅返回路径字符串，**不会在磁盘上创建目录**。必须先调用 `QDir().mkpath(tempDirPath)` 创建目录，否则 `QFile::copy()` 会因目标目录不存在而静默失败。

`exportFootprintLibrary()` 内部会检测临时目录中已有的 `.kicad_mod` 文件，对同名封装覆盖、不同名封装保留，因此无需额外合并逻辑。

### 修复 4：符号导出阶段 — 重试时始终复制已有符号库

**文件**：`src/services/export/SymbolExportStage.cpp`

将条件从：
```cpp
if (finalFileExists && (!m_options.overwriteExistingFiles || m_options.updateMode)) {
```
改为：
```cpp
if (finalFileExists && (!m_options.overwriteExistingFiles || m_options.updateMode || m_options.retryMode)) {
```

确保重试时已有符号库被复制到临时文件，`ExporterSymbol::exportSymbolLibrary()` 的合并逻辑会保留已有符号并添加/更新重试的符号。

### 修复 5：导出启动后重置标志

**文件**：`src/services/export/ParallelExportService.cpp`

在 `startExport()` 中，所有阶段创建并启动之后，重置 `retryMode`：

```cpp
// 重试模式仅对本次导出生效，避免影响后续正常导出
m_options.retryMode = false;
```

## 后果

### 正面影响

1. **重试不再丢失已有内容** — 封装和符号库在重试时正确保留之前成功导出的组件
2. **改动范围小** — 仅添加一个标志位和三处条件判断，不影响正常导出流程
3. **3D 模型等无需修改** — 每组件独立文件的导出类型天然支持部分替换

### 负面影响

1. **重试时额外文件复制** — 封装阶段需复制已有 `.kicad_mod` 文件到临时目录，符号阶段需复制已有 `.kicad_sym` 文件。对于大库（数百个封装）可能有微量 I/O 开销
2. **仅修复库级别导出** — 如果未来添加新的库级别导出类型，需要同样处理 `retryMode`

### 回归预防

修改以下文件时需验证重试场景：

| 文件 | 原因 |
|------|------|
| `FootprintExportStage.cpp` | 封装库写入逻辑 |
| `SymbolExportStage.cpp` | 符号库写入逻辑 |
| `TempFileManager.cpp` | 临时目录提交/替换逻辑 |
| `ParallelExportService.cpp` | 导出流水线编排 |
| `ExportProgressViewModel.cpp` | 重试入口 |

## 验证方法

1. 构建并运行测试：`python3 tools/python/build_project.py --test`
2. 全量测试：`ctest --output-on-failure`
3. CLI 集成验证：
   ```bash
   # 首次完整导出
   OUT=/tmp/test_export
   rm -rf "$OUT"
   ./build/bin/easykiconverter --sync-logging --log-level warn \
     convert bom -i testbom.xlsx -o "$OUT" --lib-name test --3d-model --3d-model-format both

   # 记录封装数量
   find "$OUT/test.pretty" -name '*.kicad_mod' | wc -l

   # 删除一个封装模拟失败
   rm "$OUT/test.pretty/某个封装.kicad_mod"

   # 重试该组件，验证已有封装未丢失
   ```

## 相关文档

- [ADR-002: 流水线并行架构](002-pipeline-parallelism-for-export.md)
- [ADR-004: 符号库更新导出修复](004-symbol-library-update-fix.md)
- [导出流水线架构](../../developer/ARCHITECTURE.md)
