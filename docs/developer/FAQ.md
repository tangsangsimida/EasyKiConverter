# 常见问题与解决方案 (FAQ)

本文档记录项目中的常见问题及其解决方案，防止回归。

---

## 预览图相关

### Q: 为什么导入已有缓存的 BOM 表反而会卡住 UI，而无缓存时没有明显卡顿？

**问题描述**：
用户导入 BOM 表时，如果相关元器件已经命中本地缓存，界面会阻塞几秒；删除缓存后重新导入，阻塞现象反而不明显。

**根本原因**：
这是一个典型的“缓存命中路径退化为主线程重活”的问题：

1. 缓存命中后，`ComponentService::loadComponentDataFromCacheAsync()` 曾在主线程执行预览图 Base64 编码。
2. `LcscImageService::loadCachedPreviewImagesAsync()` 曾逐张发 `imageReady`，导致 UI 层重复处理图片并触发多次刷新。
3. `ComponentListViewModel` 的 `m_bomImportComplete` 若没有在下一轮导入/刷新/重试前复位，会让后续状态机走偏，放大 UI 更新成本。

**修复方案**：
- 缓存预览图的读取和 Base64 编码全部放到后台线程完成。
- 缓存图片加载完成后只走批量 `previewImagesReady`，不再逐张回放 `imageReady`。
- 在单项添加、批量导入、清空列表、刷新、重试和验证收尾时，统一复位 BOM 导入状态。

**回归预防**：
- 不要因为“命中缓存”就把后处理逻辑放回主线程。
- 缓存路径优先使用批量信号，避免逐项 UI 刷新。
- 修改 `ComponentListViewModel` 的 BOM 导入状态机时，必须同时检查：
  - `m_bomImportMode`
  - `m_listUpdatePending`
  - `m_bomImportComplete`

**相关文件**：
- `src/services/ComponentService.cpp`
- `src/services/LcscImageService.cpp`
- `src/ui/viewmodels/ComponentListViewModel.cpp`
- `src/services/ComponentCacheService.cpp`

**状态**：✅ 已修复 (2026-04-19)

---

### Q: 预览图出现在错误的目录或使用了错误的图片

**问题描述**：
导出后在输出目录的根目录出现预览图文件，但这些预览图并不是对应元器件的图片。

**根本原因**：
`LcscImageService` 中存在多个 fallback 机制，可能从错误的来源获取预览图：

1. `performFallback()` 函数 - 从 `lcsc.com` 的 HTML 搜索页面抓取图片
2. `selectBestProductForComponent()` - 当没有精确匹配时，返回产品列表的第一个产品
3. 图片 URL 提取逻辑 - 当匹配的产品没有图片时，遍历所有产品获取第一张图片

**解决方案**：
- 移除 `performFallback()` 函数及其声明
- 修改 `selectBestProductForComponent()` - 如果没有精确匹配则返回空对象，不再使用第一个产品
- 修改图片 URL 提取逻辑 - 如果匹配产品没有图片则直接报告错误，不再从其他产品获取

**相关文件**：
- `src/services/LcscImageService.cpp` - 移除所有 fallback 逻辑
- `src/services/LcscImageService.h` - 移除 performFallback 函数声明

**状态**：✅ 已修复 (2026-04-17)

---

### Q: 删除缓存后导出不存在的预览图时，预览图出现在根目录

**问题描述**：
删除缓存后，导出一个本身没有预览图的元器件，预览图文件出现在导出根目录而不是 `.preview` 子目录。

**根本原因**：
`PreviewImagesExportWorker` 在没有预览图数据时（`previewCount <= 0`），`createTempPathsForComponent` 返回空 map，导致 `setTempPaths` 不会被调用。此时 `m_options.outputPath` 仍为根目录，而不是 `.preview` 子目录。文件被直接写入根目录。

**解决方案**：
修改 `PreviewImagesExportWorker::run()`，当没有使用临时路径时，构造正确的 `.preview` 子目录路径。

**相关文件**：
- `src/services/export/PreviewImagesExportWorker.cpp` - 修复输出目录路径逻辑

**状态**：✅ 已修复 (2026-04-17)

---

### Q: 预览图下载失败，报错 "Image blocked (403)"

**问题描述**：
下载预览图时收到 403 错误，图片被服务器阻止。

**可能原因**：
- 服务器检测到爬虫行为
- 请求频率过高
- 图片 URL 已过期

**解决方案**：
使用缓存的预览图（如果可用），或等待一段时间后重试。

---

## 导出与窗口生命周期相关

### Q: 为什么第二次把新元器件追加导出到同一个 `.kicad_sym` 后，KiCad 里看不到新符号？

**问题描述**：
第一次导出后，符号库文件已经存在。随后用户选择新的元器件，继续导出到同一个符号库目录，导出流程显示成功，但在 KiCad 中重新打开该 `.kicad_sym` 时，看不到新追加的符号。

**根本原因**：
这是符号库“追加导出”路径失效导致的回归问题：

1. `SymbolExportStage` 曾在目标 `.kicad_sym` 已存在且 `overwriteExistingFiles == false` 时，直接整库 `skip` 返回，根本没有进入追加逻辑。
2. 即使后续调用了 `ExporterSymbol::exportSymbolLibrary(..., appendMode, updateMode)`，它操作的也是一个全新的临时文件，而不是现有库文件的副本。
3. 结果是 append/update 逻辑没有基于已有库内容做 merge，最终写回的内容无法包含“旧库 + 新符号”的正确组合。

**修复方案**：
- 删除“文件已存在且不覆盖就整库跳过”的早退逻辑。
- 在追加/更新模式下，先把现有 `.kicad_sym` 复制到临时文件。
- 在该临时文件上执行 `ExporterSymbol::exportSymbolLibrary()` 的 merge 逻辑。
- merge 完成后，再用临时文件提交覆盖最终库文件。

**回归预防**：
- 任何“追加到现有库”的导出逻辑，都不能只看最终文件是否存在后直接跳过。
- 如果 merge 逻辑依赖读取已有文件内容，执行对象必须是“现有文件副本”而不是空白临时文件。
- 修改 `SymbolExportStage`、`TempFileManager` 或 `ExporterSymbol` 时，必须同时验证：
  - 首次导出到空目录
  - 第二次追加新符号到同一 `.kicad_sym`
  - 更新已存在符号到同一 `.kicad_sym`

**相关文件**：
- `src/services/export/SymbolExportStage.cpp`
- `src/core/kicad/ExporterSymbol.cpp`
- `src/services/export/TempFileManager.cpp`

**状态**：✅ 已修复 (2026-04-20)

---

### Q: 为什么点击“停止导出”后再立即点“重试所有失败项”会导致未响应，甚至连关闭/强退流程也异常？

**问题描述**：
用户在导出过程中点击“停止导出”，UI 立即恢复；但如果马上点击“重试所有失败项”，程序会未响应。相关症状还可能连带出现：

- 导出中点关闭，选择“继续导出”后标题栏按钮失效
- 导出中选择“强制退出”后程序没有完全结束

**根本原因**：
这是导出会话生命周期管理不完整导致的回归问题：

1. 停止导出时，旧导出会话的 stage/worker 仍可能在后台收尾。
2. 如果新一轮重试马上开始，错误的清理逻辑会提前销毁这些“仍在运行的旧 stage”，导致悬空对象、事件循环卡死或未响应。
3. 关闭窗口与强制退出流程会复用同一批导出状态；一旦导出会话进入不一致状态，窗口控制也会连带异常。

**修复方案**：
- 停止导出时，立即结束**当前 UI 会话**，但旧后台任务只做隔离，不允许再回写当前会话。
- 使用导出会话代数（generation）隔离迟到回调，旧会话的 `itemStatusChanged/progressChanged/completed` 必须被丢弃。
- `cleanupExportStages()` 不能提前销毁仍在运行中的旧 stage，只能把它们从当前会话映射中移除，等其自然完成后再安全回收。
- 标题栏关闭按钮不能直接复用 `window.close()` 作为普通关闭入口，必须走统一的 `WindowController` 关闭决策逻辑。
- 强制退出时必须显式清理网络单例和后台线程，避免窗口已关但进程未退出。

**回归预防**：
- 修改导出取消逻辑时，不要在 UI 线程同步等待 worker 收尾。
- 修改重试逻辑时，不要假设“点击停止”就等于“所有 stage 已经完全退出”。
- 修改 stage 清理逻辑时，必须区分：
  - 当前会话的活动 stage
  - 旧会话仍在后台收尾的 stage
- 修改窗口关闭逻辑时，必须同时验证：
  - 停止导出后立即重试
  - 导出中关闭后选择“继续导出”
  - 导出中选择“强制退出”

**相关文件**：
- `src/services/export/ParallelExportService.cpp`
- `src/services/export/ParallelExportService.h`
- `src/services/export/ExportTypeStage.cpp`
- `src/ui/qml/components/WindowController.qml`
- `src/ui/qml/Main.qml`
- `src/main.cpp`

**状态**：✅ 已修复 (2026-04-19)

---

### Q: 重试失败组件后，导出的封装库中只有重试的组件，之前成功的组件丢失

**问题描述**：
导出 BOM 后部分元器件失败，点击"重试失败组件"后，导出的 KiCad 库中仅包含重试的组件，之前已成功导出的组件全部丢失。

**根本原因**：
重试流程将**仅失败的组件 ID** 传递给整个导出流水线，库级别的导出阶段（Symbol、Footprint）只处理这些 ID，生成的输出仅包含重试的组件，然后替换磁盘上已有的库文件。

**封装丢失路径**：
1. `retryFailedComponents()` 将失败 ID 传给 `startPreload()` — `ExportProgressViewModel.cpp:728`
2. `ParallelExportService::startPreload()` 用子集覆盖 `m_componentIds` — `ParallelExportService.cpp:79`
3. `startExport()` 仅将这些 ID 传给 `FootprintExportStage::start()` — `ParallelExportService.cpp:326`
4. `FootprintExportStage::doLibraryExport()` 创建新的空临时目录，仅写入重试的封装 — `FootprintExportStage.cpp:428,448`
5. `TempFileManager::commitDirectory()` 删除整个已有 `.pretty` 目录（`removeRecursively()`），替换为仅含重试封装的临时目录 — `TempFileManager.cpp:205`

**符号丢失路径**：
- 当 `overwriteExistingFiles=true` 时，临时文件从空开始，仅写入重试的符号，已有符号全部丢失 — `SymbolExportStage.cpp:223`

**不受影响的类型**：
- 3D 模型（`.wrl`、`.step`）、预览图、数据手册：每个组件独立文件，仅替换重试的组件文件。

**修复方案**：
在 `ExportOptions` 中添加 `retryMode` 标志，告知库级别导出阶段在重试时保留已有输出。

1. `ExportProgress.h` — 添加 `bool retryMode = false`
2. `ExportProgressViewModel.cpp` — `retryComponent()` 和 `retryFailedComponents()` 中设置 `retryMode = true`
3. `FootprintExportStage.cpp` — 重试模式下，先将已有 `.kicad_mod` 复制到临时目录，再执行导出
4. `SymbolExportStage.cpp` — 重试模式下，始终复制已有 `.kicad_sym` 到临时文件
5. `ParallelExportService.cpp` — 导出启动后重置 `retryMode = false`

**关键陷阱**：
`TempFileManager::createTempDirectoryPath()` 仅返回路径字符串，**不会在磁盘上创建目录**。必须先调用 `QDir().mkpath(tempDirPath)` 创建目录，否则 `QFile::copy()` 会因目标目录不存在而静默失败。

**回归预防**：
修改以下文件时需验证重试场景：
- `FootprintExportStage.cpp` — 封装库写入逻辑
- `SymbolExportStage.cpp` — 符号库写入逻辑
- `TempFileManager.cpp` — 临时目录提交/替换逻辑
- `ParallelExportService.cpp` — 导出流水线编排
- `ExportProgressViewModel.cpp` — 重试入口

**相关文件**：
- `src/services/export/ExportProgress.h` — 添加 retryMode 字段
- `src/ui/viewmodels/ExportProgressViewModel.cpp` — 重试入口设置标志
- `src/services/export/FootprintExportStage.cpp` — 复制已有封装到临时目录
- `src/services/export/SymbolExportStage.cpp` — 复制已有符号库到临时文件
- `src/services/export/ParallelExportService.cpp` — 重置标志
- `src/services/export/TempFileManager.cpp` — 临时目录提交逻辑

**状态**：✅ 已修复 (2026-05-08)

---

## 3D 模型相关

### Q: 3D 模型选择使用循环切换逻辑而非独立 toggle

**问题描述**：
用户点击 WRL 按钮时，希望只切换 WRL 的选中状态，不影响 STEP 的选中状态。

**解决方案**：
使用位掩码管理格式选择：
- `1` (bit 0) = WRL
- `2` (bit 1) = STEP
- `3` = Both
- `0` = None

点击每个按钮只切换对应的 bit，不影响其他位。

**相关文件**：
- `src/services/export/ExportProgress.h` - 定义 `MODEL_3D_FORMAT_*` 常量和辅助方法
- `src/ui/qml/components/ExportSettingsCard.qml` - 实现独立 toggle 逻辑

**状态**：✅ 已修复 (2026-04-17)

---

### Q: 取消勾选 3D 模型时子选项没有隐藏

**问题描述**：
当用户取消勾选"3D模型"主复选框时，WRL/STEP 子选项仍然显示。

**解决方案**：
将 WRL/STEP 按钮组的 `visible` 属性设置为 `model3dCheckbox.checked`，确保父级复选框状态联动。

**相关文件**：
- `src/ui/qml/components/ExportSettingsCard.qml`

**状态**：✅ 已修复 (2026-04-17)

### Q: 选择了单独的 3D 模型格式（WRL 或 STEP），但导出了两种格式

**问题描述**：
用户在导出设置中选择了只导出 WRL 格式，但实际导出了 WRL 和 STEP 两种格式。

**根本原因**：
`ExportProgressViewModel::startExport()` 方法创建 `ExportOptions` 时没有设置 `exportModel3DFormat` 参数，导致该参数保持默认值 3（Both）。

调用链分析：
1. 用户在 QML 中选择格式 → `ExportSettingsViewModel::setExportModel3DFormat()` 正确保存
2. 点击导出按钮 → `ExportButtonsSection.qml` 调用 `progressController.startExport()`
3. **问题所在**：`startExport()` 签名缺少 `exportModel3DFormat` 参数，创建 options 时该字段使用默认值

**解决方案**：
1. 在 `ExportProgressViewModel::startExport()` 添加 `exportModel3DFormat` 参数
2. 在方法内部将参数传递到 `ExportOptions`
3. 在 QML 调用处传递 `settingsController.exportModel3DFormat`

**相关文件**：
- `src/ui/viewmodels/ExportProgressViewModel.h` - 添加参数声明
- `src/ui/viewmodels/ExportProgressViewModel.cpp` - 传递格式值到 options
- `src/ui/qml/components/ExportButtonsSection.qml` - 传递格式参数

**调试方法**：
启用调试模式后，日志中 `Model3DExportWorker::run()` 会输出：
- 正确情况（选择 WRL）：`exportModel3DFormat: 1 needWrl: true needStep: false`
- 错误情况：`exportModel3DFormat: 3 needWrl: true needStep: true`

**状态**：✅ 已修复 (2026-04-19)

---

### Q: 有缓存的情况下导出 3D 模型仍会发起网络请求

**问题描述**：
批量导出时，即使所有 3D 模型（STEP/OBJ）都已缓存到本地磁盘，导出过程仍会发起大量网络请求，导致弱网环境下导出缓慢甚至超时失败。

**根本原因**：
3D 模型缓存复用存在多个设计缺陷：

1. **STEP 导出先网络后缓存**（最严重）：`Model3DExportWorker::run()` 中 STEP 的处理顺序是先调用 `downloadStepDataSync()` 发起网络请求，只有网络失败后才检查磁盘缓存。批量导出 100 个组件时，即使所有 STEP 都已缓存，仍会发起 100 次网络请求。

2. **预加载不加载 OBJ 二进制数据**：`ComponentService::loadComponentDataFromCacheAsync()` 加载了 symbol、footprint、preview、datasheet，但不加载 OBJ 二进制数据，导致 `m_data->model3DObjRaw()` 总是为空，导出阶段需要重新从磁盘读取或网络下载。

3. **缓存验证不完整**：`ComponentCacheService::hasModel3DCached()` 只检查文件是否存在，不验证文件大小是否大于 0，可能将损坏的空文件误判为有效缓存。

**修复方案**：

1. **STEP 缓存优先**：修改 `Model3DExportWorker.cpp`，将 STEP 导出逻辑从"先网络后缓存"改为"先缓存后网络"：
   ```
   之前：downloadStepDataSync() → 失败后才查缓存
   之后：cache->loadModel3D(uuid, "step") → 无缓存时才 downloadStepDataSync()
   ```

2. **预加载 OBJ 数据**：在 `ComponentService::loadComponentDataFromCacheAsync()` 中添加 OBJ 二进制数据的加载，设置到 `ComponentData::model3DObjRaw`，避免导出阶段重复磁盘 I/O。

3. **缓存完整性检查**：增强 `hasModel3DCached()`，同时检查文件存在且大小大于 0。

**回归预防**：
- 新增缓存使用逻辑时，必须遵循"先缓存后网络"的优先级顺序
- 预加载阶段应尽可能加载所有需要的数据到内存，减少导出阶段的 I/O
- 缓存存在性检查应包含基本的完整性验证（文件大小 > 0）
- 修改 `Model3DExportWorker` 缓存逻辑时，必须验证：
  - 有缓存时不发起网络请求
  - 无缓存时正常下载并保存到缓存
  - 网络失败时有缓存兜底

**相关文件**：
- `src/services/export/Model3DExportWorker.cpp` - STEP 缓存优先逻辑
- `src/services/ComponentService.cpp` - 预加载 OBJ 数据
- `src/services/ComponentService.h` - CacheLoadResult 添加 objData 字段
- `src/services/ComponentCacheService.cpp` - 缓存完整性验证

**状态**：✅ 已修复 (2026-05-08)

---

### Q: 导出的 3D 模型文件名与封装内记录的 3D 模型路径不一致

**问题描述**：
导出的 3D 模型文件名为 `C0603.step`，但封装内记录的 3D 模型路径却是 `../lib.3dmodels/C0603_L1.6-W0.8-H0.8.step`，导致 KiCad 无法找到对应的 3D 模型文件。

**根本原因**：
`Model3DData` 对象中的 `name` 字段在数据流传递过程中被意外丢失，由两个独立的 bug 共同导致：

**Bug 1 - ProcessWorker 重建 Model3DData 丢失属性**：
`ProcessWorker::parse3DModelData()` 收到 3D 模型 OBJ 数据后，会**重新创建** `Model3DData` 对象来存放原始数据，但旧代码只恢复了 `uuid`，`name`、`translation`、`rotation` 全部丢失。

**Bug 2 - ComponentService 缓存加载覆盖已有 model3DData**：
`ComponentCacheService::loadComponentData()` 已经从 `metadata.json` 正确加载了包含 `name` 的完整 `model3DData`，但 `ComponentService::loadComponentDataFromCacheAsync()` 会**重新创建一个只有 UUID 的 `model3DData`**，并通过 `setModel3DData()` 覆盖掉已有的完整数据。

**数据流分析**：
```
EasyEDA API -> FootprintData.model3D().name() = "C0603_L1.6-W0.8-H0.8"
                   |
              【Bug 2】缓存加载 -> model3DData()->name() 被覆盖为空
                   |
              3D 模型导出 -> 回退到封装名 -> C0603.step
              封装导出   -> 使用原始名称 -> C0603_L1.6-W0.8-H0.8.step（路径）
```

**修复方案**：
1. `ProcessWorker::parse3DModelData()`：重建 `Model3DData` 前，先保存并恢复 `name`、`translation`、`rotation`
2. `ComponentService::loadComponentDataFromCacheAsync()`：只在 `cachedData->model3DData()` 不存在或 UUID 缺失时才创建/补充，不再调用 `setModel3DData()` 覆盖已有数据

**回归预防**：
- 重建对象时必须备份旧对象的所有属性，不要假设新对象只需要部分字段
- 缓存加载路径不要重复解析/覆盖，补充逻辑应该是"只在缺失时补充"，而不是"无条件重建"
- 测试缓存场景时，必须同时验证：首次获取（无缓存）和缓存命中，两者的行为应当完全一致

**相关文件**：
- `src/workers/ProcessWorker.cpp` - 恢复 Model3DData 完整属性
- `src/services/ComponentService.cpp` - 不再覆盖已有的 model3DData

**状态**：✅ 已修复 (2026-04-29)

---

## 网络相关

### Q: 在弱网络环境下导出失败率很高

**问题描述**：
网络不稳定时，大量元器件导出失败。

**解决方案**：
启用"弱网络支持"选项（`weakNetworkSupport`），可以：
- 降低并发数
- 增加重试次数
- 延长超时时间

**相关配置**：
- `src/services/ConfigService.cpp` - `getWeakNetworkSupport()` / `setWeakNetworkSupport()`

---

### Q: 不存在的元器件被误判为验证成功，且错误响应被缓存

**问题描述**：
当请求不存在的元器件（如 C2041）时，EasyEDA API 返回业务层 404 错误：
```json
{"code":404,"message":"Component not found","success":false}
```

但系统将此响应误判为验证成功，导致：
- 元器件显示为"验证成功"但没有预览图
- 错误响应被缓存到本地磁盘
- 后续请求从缓存加载，持续显示为成功

**根本原因**：
`src/services/CadDataLoader.cpp:fetchAndParseCadData()` 只检查了：
- HTTP/网络层是否成功
- JSON 是否能解析

但没有检查返回体里的业务层错误标记：
- `success: false`
- `code: 404`
- `result` 缺失或为空

**修复方案**：
在 `CadDataLoader::fetchAndParseCadData()` 中添加业务层错误检查：
1. 检查 `success == false` → 返回错误（404时返回"元器件不存在（404）"）
2. 检查 `result` 缺失或为 null → 返回错误
3. 检查 `result` 为空对象 → 返回错误

**回归预防**：
- 任何解析 API 响应的代码都必须检查业务层 `success` 字段
- 不要因为 HTTP 成功就认为业务成功
- `result` 字段缺失或为空时必须视为失败

**相关文件**：
- `src/services/CadDataLoader.cpp:117` - fetchAndParseCadData()
- `src/workers/FetchWorker.cpp:203` - 参考实现（已有正确判断）

**状态**：✅ 已修复 (2026-04-21)

---

### Q: 请求被取消时未标记为验证失败

**问题描述**：
当网络请求被取消时（如用户删除元器件时），系统未将此标记为验证失败。

**根本原因**：
`handleFetchError()` 的 `isCadDataFailure` 检查中没有包含 "Request cancelled" 错误信息。

**修复方案**：
在 `isCadDataFailure` 检查中添加 `error.contains("Request cancelled")`。

**相关文件**：
- `src/ui/viewmodels/ComponentListViewModel.cpp:850` - isCadDataFailure 检查

**状态**：✅ 已修复 (2026-04-21)

---

### Q: 删除元器件时未取消网络请求，导致缓存被保存

**问题描述**：
删除元器件时没有取消该元器件的网络请求，导致请求完成后数据被缓存。

**修复方案**：
1. 在 `ComponentService` 中添加 `cancelRequestForComponent()` 方法
2. 在 `LcscImageService` 中添加 `cancelRequestForComponent()` 方法
3. 在 `ComponentListViewModel::removeComponent()` 中调用 `cancelRequestForComponent()`

**相关文件**：
- `src/services/ComponentService.cpp` - cancelRequestForComponent()
- `src/services/LcscImageService.cpp` - cancelRequestForComponent()
- `src/ui/viewmodels/ComponentListViewModel.cpp:340` - removeComponent() 调用

**状态**：✅ 已修复 (2026-04-21)

---

## 构建相关

### Q: CMake 配置失败，提示生成器不匹配

**问题描述**：
运行 `cmake --build` 时报错：
```
CMake Error: generator Ninja Does not match the generator used previously: Unix Makefiles
```

**解决方案**：
清理构建目录后重新配置：
```bash
rm -rf build
python tools/python/build_project.py
```

**状态**：✅ 已修复 (2026-04-17)

---

## 窗口 / QML 启动相关

### Q: 应用进程正常启动但主窗口不显示，也没有任务栏条目

**问题描述**：
启动后日志显示 QML 已加载完成、图标已设置、事件循环继续运行，甚至更新检查也开始执行，但桌面上没有主窗口，也看不到任务栏条目。

典型症状：
- `QQmlApplicationEngine` 没有报错
- `rootObjects()` 不为空
- 日志出现“QML 加载完成，开始设置图标”
- 应用不会立刻退出，但窗口没有真正映射到桌面

**根本原因**：
这是一次窗口启动职责拆分后的回归，触发条件是以下因素叠加：

1. 主窗口是 `ApplicationWindow`，并使用了 `Qt.FramelessWindowHint`
2. 启动期把窗口显示逻辑完全交给 QML 的 `WindowStartupManager`
3. `Main.qml` 中将 `visible` 绑定为常量或启动门控值（例如 `visible: false` / `visible: startupReady`）
4. QML 侧又试图通过 `show()` / `visible = true` 在运行时显示窗口

在这种情况下，`visible` 绑定和运行时显示调用可能互相覆盖。结果是：
- QML/Qt 对象存在
- 应用逻辑继续运行
- 但顶层原生窗口没有稳定映射到桌面环境

另外，这次拆分还暴露了两个高风险点：

1. 新增 QML 组件如果没有同时加入 `CMakeLists.txt` 和 `tests/ui/CMakeLists.txt`，会出现“编译通过但运行时 QML 组件找不到”的问题
2. 新增组件如果声明 `required property ApplicationWindow window`，必须显式导入 `QtQuick.Controls`

**解决方案**：

1. 不要把主窗口显示完全绑定到 QML 常量属性上
2. 启动阶段允许 QML 设置几何和状态，但由 C++ 在引擎加载后提供一次可见性兜底
3. 若根窗口在启动后仍是隐藏态，由 `main.cpp` 执行 `show()` / `raise()` / `requestActivate()`
4. 所有新增 QML 组件必须同步更新：
   - `src/ui/qml/components/qmldir`
   - 根 `CMakeLists.txt` 中的 `QML_FILES`
   - `tests/ui/CMakeLists.txt` 中的 `QML_FILES`
5. 使用 `ApplicationWindow` 作为类型时，QML 文件必须导入 `QtQuick.Controls`

**相关文件**：
- `src/main.cpp` - 根窗口加载后的可见性兜底
- `src/ui/qml/Main.qml` - 主窗口初始可见性策略
- `src/ui/qml/components/WindowController.qml` - 启动/运行时窗口编排
- `src/ui/qml/components/WindowStartupManager.qml` - 启动阶段窗口恢复
- `src/ui/qml/components/WindowPersistenceManager.qml` - 运行时窗口状态持久化
- `CMakeLists.txt` - 应用 QML 资源清单
- `tests/ui/CMakeLists.txt` - UI 测试 QML 清单

**防回归规则**：
- 不要对主窗口使用“常量 `visible` 绑定 + 运行时 `show()`”的混合控制
- `FramelessWindowHint` 主窗口的首次映射必须有 C++ 侧兜底
- 拆分 QML 组件时，资源注册、`qmldir` 注册、测试清单必须一起改
- 启动恢复与运行时持久化可以拆模块，但“首次显示”的最终责任必须明确，不能在 QML/C++ 两边各管一半

**建议回归测试**：
- 普通窗口启动后应立即出现在桌面和任务栏
- 最大化关闭后重启，应恢复为最大化状态
- 从最大化点击窗口化，应恢复到上次 normal geometry
- 删除窗口配置后重启，应以默认尺寸居中显示
- 在 Linux/X11 与 Linux/Wayland 下分别验证主窗口是否真正映射

**状态**：✅ 已修复 (2026-04-19)

---

## UI 滚动位置相关

### Q: 导出结果列表在转换过程中更新后自动回到顶部

**问题描述**：
用户在导出过程中向下滚动结果列表查看进度，但每当有元器件状态更新（如从"导出中"变为"成功"）时，列表会自动滚动回顶部，导致用户难以持续跟踪底部的元器件状态。

**根本原因**：
`ExportResultsCard.qml` 中的 GridView 直接绑定到 `filteredResultsList` 属性，而该属性的 getter 方法每次调用都返回一个新的 `QVariantList`：

```cpp
QVariantList ExportProgressViewModel::filteredResultsList() const {
    // 每次创建新的列表
    QVariantList filtered;
    for (const auto& item : m_resultsList) {
        // 过滤逻辑...
        filtered.append(item);
    }
    return filtered;
}
```

每次 `filteredResultsListChanged()` 信号触发时：
1. QML 调用 getter 获取新列表
2. GridView 收到一个全新的列表对象
3. 滚动位置被重置到顶部

而元器件列表使用 `DelegateModel` 绑定到原始模型，通过 `DelegateModelGroup` 的 `inDisplay` 属性控制显示/隐藏，避免位置重置。

**解决方案**：
参考 `ComponentListCard.qml` 的实现方式，使用 `DelegateModel` 替代直接绑定：

1. 添加 `DelegateModel` 绑定到原始 `resultsList`
2. 使用 `DelegateModelGroup` 控制显示/隐藏
3. 添加防抖定时器（50ms）避免频繁过滤
4. 监听 `filterModeChanged` 和 `resultsListChanged` 信号触发过滤
5. 在 `updateFilter()` 函数中通过 `item.inDisplay` 控制每项是否显示

```qml
DelegateModel {
    id: visualModel
    model: resultsLoader.exportProgressController.resultsList
    groups: [
        DelegateModelGroup {
            id: displayGroup
            includeByDefault: true
            name: "display"
        }
    ]
    filterOnGroup: "display"
    delegate: ResultListItem { ... }
    
    function updateFilter() {
        for (var i = 0; i < items.count; i++) {
            var item = items.get(i);
            var dataObj = item.model.modelData || item.model;
            var status = dataObj.status || "pending";
            var passFilter = false;
            // 过滤逻辑...
            item.inDisplay = passFilter;
        }
    }
}
```

**回归预防**：
- 任何新增的列表视图，如果需要在运行时更新数据并保持滚动位置，必须使用 `DelegateModel`
- 不要直接绑定到每次返回新对象的属性
- 列表过滤/筛选必须通过 `DelegateModelGroup` 的 `inDisplay` 属性控制，而不是重新创建列表
- 使用 `cacheBuffer: 500` 和 `reuseItems: true` 启用虚拟化和 Item 回收

**相关文件**：
- `src/ui/qml/components/ExportResultsCard.qml` - 使用 DelegateModel 替代直接绑定
- `src/ui/qml/components/ComponentListCard.qml` - 参考实现

**状态**：✅ 已修复 (2026-04-19)

---

## 回归测试清单

每次发布前请确认以下功能正常：

- [ ] 3D 模型格式选择（WRL/STEP/Both）
- [ ] 3D 模型主复选框与子选项联动
- [ ] 3D 模型格式选择后导出实际只导出选择格式（不是 Both）
- [ ] 预览图只从 API 固定位置获取
- [ ] 弱网络模式下的导出成功率
- [ ] 符号库、封装库导出功能
- [ ] 已有 `.kicad_sym` 上追加导出新符号后，KiCad 中可见新符号
- [ ] 主窗口启动后可见，并出现在任务栏
- [ ] 最大化关闭后重启仍为最大化
- [ ] 最大化恢复到窗口化时保留上次 normal geometry
- [ ] 新增 QML 组件时，`qmldir`、CMake 资源清单、UI 测试清单已同步更新
- [ ] 导出结果列表在转换过程中保持滚动位置，不会自动回到顶部
- [ ] 有缓存时导出 3D 模型不发起网络请求（STEP 缓存优先）
- [ ] 重试失败元器件后，导出的库中同时包含已成功项和重试项
- [ ] 重试失败组件后，已有封装/符号未丢失

---

## 如何贡献

如果你发现了新的常见问题并找到了解决方案，请更新本文档。

格式要求：
1. 问题描述（清晰说明症状）
2. 根本原因
3. 解决方案
4. 相关文件和状态
5. 更新日期
