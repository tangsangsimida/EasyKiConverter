# FAQ for Developers

> This document records common development issues, root cause analysis, fixes, and regression prevention notes. For user-facing FAQ, see ../user/FAQ_en.md

---

## Preview Images

### Q: Why does importing a cached BOM file block the UI, while without cache there's no obvious stuttering?

**Problem Description**:
When users import a BOM file, if the related components hit the local cache, the UI blocks for several seconds; after deleting the cache and re-importing, the blocking phenomenon is less noticeable.

**Root Cause**:
This is a typical "cache hit path degrades to main thread heavy work" issue:

1. After cache hit, `ComponentService::loadComponentDataFromCacheAsync()` used to perform preview image Base64 encoding on the main thread.
2. `LcscImageService::loadCachedPreviewImagesAsync()` used to send `imageReady` one by one, causing the UI layer to repeatedly process images and trigger multiple refreshes.
3. If `ComponentListViewModel`'s `m_bomImportComplete` is not reset before the next import/refresh/retry, the subsequent state machine goes astray, amplifying UI update costs.

**Fix**:
- Move reading and Base64 encoding of cached preview images to the background thread.
- After cached images are loaded, only use batch `previewImagesReady`,，不再逐张回放 `imageReady` (stop playing back `imageReady` one by one).
- Reset BOM import status uniformly during single item addition, batch import, list clearing, refresh, retry, and verification finalization.

**Regression Prevention**:
- Do not move post-processing logic back to the main thread just because of "cache hit".
- Cache path should preferentially use batch signals, avoid per-item UI refresh.
- When modifying `ComponentListViewModel`'s BOM import state machine, must simultaneously check:
  - `m_bomImportMode`
  - `m_listUpdatePending`
  - `m_bomImportComplete`

**Related Files**:
- `src/services/ComponentService.cpp`
- `src/services/LcscImageService.cpp`
- `src/ui/viewmodels/ComponentListViewModel.cpp`
- `src/services/ComponentCacheService.cpp`

**Status**: ✅ Fixed (2026-04-19)

---

### Q: Preview images appear in the wrong directory or use wrong images

**Problem Description**:
After export, preview image files appear in the root directory of the output directory, but these preview images are not the correct images for the components.

**Root Cause**:
There are multiple fallback mechanisms in `LcscImageService` that may fetch preview images from the wrong source:

1. `performFallback()` function - fetches images from `lcsc.com` HTML search page
2. `selectBestProductForComponent()` - when there's no exact match, returns the first product from the list
3. Image URL extraction logic - when the matched product has no image, iterates through all products to get the first image

**Solution**:
- Remove `performFallback()` function and its declaration
- Modify `selectBestProductForComponent()` - if no exact match, return empty object, do not use the first product
- Modify image URL extraction logic - if matched product has no image, report error directly, do not fetch from other products

**Related Files**:
- `src/services/LcscImageService.cpp` - remove all fallback logic
- `src/services/LcscImageService.h` - remove performFallback function declaration

**Status**: ✅ Fixed (2026-04-17)

---

### Q: After deleting cache, when exporting a component without preview images, preview images appear in the root directory

**Problem Description**:
After deleting cache, exporting a component that has no preview image, the preview image file appears in the export root directory instead of the `.preview` subdirectory.

**Root Cause**:
When `PreviewImagesExportWorker` has no preview image data (`previewCount <= 0`), `createTempPathsForComponent` returns an empty map, causing `setTempPaths` not to be called. At this point, `m_options.outputPath` is still the root directory, not the `.preview` subdirectory. Files are written directly to the root directory.

**Solution**:
Modify `PreviewImagesExportWorker::run()` to construct the correct `.preview` subdirectory path when not using temporary paths.

**Related Files**:
- `src/services/export/PreviewImagesExportWorker.cpp` - fix output directory path logic

**Status**: ✅ Fixed (2026-04-17)

---

### Q: Preview image download fails with "Image blocked (403)" error

**Problem Description**:
When downloading preview images, a 403 error is received and the image is blocked by the server.

**Possible Causes**:
- Server detected crawler behavior
- Request frequency too high
- Image URL has expired

**Solution**:
Use cached preview images (if available), or wait a while and retry.

---

## Export and Window Lifecycle

### Q: Why can't I see new symbols in KiCad after appending new components to the same `.kicad_sym` file for the second time?

**Problem Description**:
After the first export, the symbol library file already exists. The user then selects new components and continues exporting to the same symbol library directory. The export process shows success, but when reopening the `.kicad_sym` in KiCad, the newly appended symbols are not visible.

**Root Cause**:
This is a regression caused by the symbol library "append export" path being broken:

1. `SymbolExportStage` used to directly skip the entire library when the target `.kicad_sym` already existed and `overwriteExistingFiles == false`, without entering the append logic at all.
2. Even if `ExporterSymbol::exportSymbolLibrary(..., appendMode, updateMode)` was subsequently called, it operated on a brand new temporary file, not a copy of the existing library file.
3. As a result, the append/update logic was not based on merging with the existing library content. The final written content could not contain the correct combination of "old library + new symbols".

**Fix**:
- Remove the "exit early for entire library if file exists and overwrite is disabled" logic.
- In append/update mode, first copy the existing `.kicad_sym` to a temporary file.
- Execute the merge logic of `ExporterSymbol::exportSymbolLibrary()` on this temporary file.
- After merge is complete, use the temporary file to commit and overwrite the final library file.

**Regression Prevention**:
- Any export logic that "appends to an existing library" cannot just check if the final file exists and skip directly.
- If the merge logic depends on reading existing file content, the target must be "a copy of the existing file" rather than a blank temporary file.
- When modifying `SymbolExportStage`, `TempFileManager`, or `ExporterSymbol`, must verify:
  - First export to an empty directory
  - Second append new symbols to the same `.kicad_sym`
  - Update existing symbols in the same `.kicad_sym`

**Related Files**:
- `src/services/export/SymbolExportStage.cpp`
- `src/core/kicad/ExporterSymbol.cpp`
- `src/services/export/TempFileManager.cpp`

**Status**: ✅ Fixed (2026-04-20)

---

### Q: Why does clicking "Stop Export" and then immediately clicking "Retry All Failed Items" cause unresponsive behavior, and even the close/force quit process becomes abnormal?

**Problem Description**:
During export, the user clicks "Stop Export" and the UI immediately recovers; but if the user immediately clicks "Retry All Failed Items", the program becomes unresponsive. Related symptoms may also include:

- During export, clicking close, after selecting "Continue Export", the title bar buttons become unresponsive
- During export, selecting "Force Exit" and the program doesn't fully end

**Root Cause**:
This is a regression caused by incomplete export session lifecycle management:

1. When stopping export, the old export session's stages/workers may still be wrapping up in the background.
2. If a new round of retry starts immediately, incorrect cleanup logic will prematurely destroy these "still running old stages", leading to dangling objects, event loop freezes, or unresponsiveness.
3. Window close and force exit flows reuse the same set of export states; once the export session enters an inconsistent state, window control will also be affected.

**Fix**:
- When stopping export, immediately end the **current UI session**, but old background tasks are only isolated, not allowed to write back to the current session.
- Use export session generation (generation) to isolate late callbacks. Old session's `itemStatusChanged/progressChanged/completed` must be discarded.
- `cleanupExportStages()` cannot prematurely destroy old stages that are still running, can only remove them from the current session mapping, and wait for natural completion before safe recycling.
- The title bar close button cannot directly reuse `window.close()` as a normal close entry, must go through the unified `WindowController` close decision logic.
- Force exit must explicitly clean up the network singleton and background threads, avoiding the situation where the window is closed but the process hasn't exited.

**Regression Prevention**:
- When modifying export cancellation logic, do not synchronously wait for worker cleanup on the UI thread.
- When modifying retry logic, do not assume that "clicking stop" equals "all stages have completely exited".
- When modifying stage cleanup logic, must distinguish:
  - Active stages of the current session
  - Old stages still wrapping up in the background from previous sessions
- When modifying window close logic, must verify:
  - Retry immediately after stopping export
  - Close during export and select "Continue Export"
  - Select "Force Exit" during export

**Related Files**:
- `src/services/export/ParallelExportService.cpp`
- `src/services/export/ParallelExportService.h`
- `src/services/export/ExportTypeStage.cpp`
- `src/ui/qml/components/WindowController.qml`
- `src/ui/qml/Main.qml`
- `src/main.cpp`

**Status**: ✅ Fixed (2026-04-19)

---

### Q: After retrying failed components, the exported footprint library only contains retried components, previously successful components are lost

**Problem Description**:
After exporting a BOM, some components failed. Clicking "Retry Failed Components" results in the exported KiCad library only containing retried components, all previously successfully exported components are lost.

**Root Cause**:
The retry process passes **only failed component IDs** to the entire export pipeline. Library-level export stages (Symbol, Footprint) only process these IDs, generating output containing only retried components, then replacing the existing library file on disk.

**Footprint Loss Path**:
1. `retryFailedComponents()` passes failed IDs to `startPreload()` — `ExportProgressViewModel.cpp:728`
2. `ParallelExportService::startPreload()` overwrites `m_componentIds` with the subset — `ParallelExportService.cpp:79`
3. `startExport()` only passes these IDs to `FootprintExportStage::start()` — `ParallelExportService.cpp:326`
4. `FootprintExportStage::doLibraryExport()` creates a new empty temporary directory, only writes retried footprints — `FootprintExportStage.cpp:428,448`
5. `TempFileManager::commitDirectory()` deletes the entire existing `.pretty` directory (`removeRecursively()`), replaces with temporary directory containing only retried footprints — `TempFileManager.cpp:205`

**Symbol Loss Path**:
- When `overwriteExistingFiles=true`, the temporary file starts from empty, only writes retried symbols, all existing symbols are lost — `SymbolExportStage.cpp:223`

**Unaffected Types**:
- 3D models (`.wrl`, `.step`), preview images, datasheets: each component is an independent file, only replaces retried component files.

**Fix**:
Add `retryMode` flag in `ExportOptions` to inform library-level export stages to preserve existing output during retry.

1. `ExportProgress.h` — add `bool retryMode = false`
2. `ExportProgressViewModel.cpp` — set `retryMode = true` in `retryComponent()` and `retryFailedComponents()`
3. `FootprintExportStage.cpp` — in retry mode, first copy existing `.kicad_mod` to temporary directory, then perform export
4. `SymbolExportStage.cpp` — in retry mode, always copy existing `.kicad_sym` to temporary file
5. `ParallelExportService.cpp` — reset `retryMode = false` after export starts

**Key Trap**:
`TempFileManager::createTempDirectoryPath()` only returns a path string, **does not create the directory on disk**. Must first call `QDir().mkpath(tempDirPath)` to create the directory, otherwise `QFile::copy()` will silently fail because the target directory doesn't exist.

**Regression Prevention**:
When modifying the following files, must verify retry scenarios:
- `FootprintExportStage.cpp` — footprint library write logic
- `SymbolExportStage.cpp` — symbol library write logic
- `TempFileManager.cpp` — temporary directory commit/replace logic
- `ParallelExportService.cpp` — export pipeline orchestration
- `ExportProgressViewModel.cpp` — retry entry points

**Related Files**:
- `src/services/export/ExportProgress.h` — add retryMode field
- `src/ui/viewmodels/ExportProgressViewModel.cpp` — retry entry sets flag
- `src/services/export/FootprintExportStage.cpp` — copy existing footprints to temp directory
- `src/services/export/SymbolExportStage.cpp` — copy existing symbol library to temp file
- `src/services/export/ParallelExportService.cpp` — reset flag
- `src/services/export/TempFileManager.cpp` — temp directory commit logic

**Status**: ✅ Fixed (2026-05-08)

---

## 3D Model Related

### Q: 3D model selection uses cyclic switching logic instead of independent toggle

**Problem Description**:
When the user clicks the WRL button, they want to only toggle WRL's selected state without affecting STEP's selected state.

**Solution**:
Use bitmask to manage format selection:
- `1` (bit 0) = WRL
- `2` (bit 1) = STEP
- `3` = Both
- `0` = None

Clicking each button only toggles the corresponding bit, without affecting other bits.

**Related Files**:
- `src/services/export/ExportProgress.h` - define `MODEL_3D_FORMAT_*` constants and helper methods
- `src/ui/qml/components/ExportSettingsCard.qml` - implement independent toggle logic

**Status**: ✅ Fixed (2026-04-17)

---

### Q: Unchecking 3D model doesn't hide sub-options

**Problem Description**:
When the user unchecks the "3D Model" main checkbox, the WRL/STEP sub-options are still displayed.

**Solution**:
Set the `visible` property of the WRL/STEP button group to `model3dCheckbox.checked`, ensuring parent checkbox state linkage.

**Related Files**:
- `src/ui/qml/components/ExportSettingsCard.qml`

**Status**: ✅ Fixed (2026-04-17)

### Q: Selected a single 3D model format (WRL or STEP), but both formats were exported

**Problem Description**:
The user selected to export only WRL format in export settings, but actually both WRL and STEP formats were exported.

**Root Cause**:
`ExportProgressViewModel::startExport()` method creates `ExportOptions` without setting the `exportModel3DFormat` parameter, causing the parameter to keep its default value of 3 (Both).

Call chain analysis:
1. User selects format in QML → `ExportSettingsViewModel::setExportModel3DFormat()` correctly saves it
2. Click export button → `ExportButtonsSection.qml` calls `progressController.startExport()`
3. **Problem**: `startExport()` signature lacks `exportModel3DFormat` parameter, the field uses default value when creating options

**Solution**:
1. Add `exportModel3DFormat` parameter to `ExportProgressViewModel::startExport()`
2. Pass the parameter to `ExportOptions` inside the method
3. At the QML call site, pass `settingsController.exportModel3DFormat`

**Related Files**:
- `src/ui/viewmodels/ExportProgressViewModel.h` - add parameter declaration
- `src/ui/viewmodels/ExportProgressViewModel.cpp` - pass format value to options
- `src/ui/qml/components/ExportButtonsSection.qml` - pass format parameter

**Debug Method**:
After enabling debug mode, `Model3DExportWorker::run()` will output in logs:
- Correct case (WRL selected): `exportModel3DFormat: 1 needWrl: true needStep: false`
- Incorrect case: `exportModel3DFormat: 3 needWrl: true needStep: true`

**Status**: ✅ Fixed (2026-04-19)

---

### Q: Exporting 3D models with cache still发起网络请求 (still makes network requests)

**Problem Description**:
During batch export, even if all 3D models (STEP/OBJ) are cached to local disk, the export process still发起大量网络请求 (still makes a large number of network requests), causing slow exports or timeout failures in weak network environments.

**Root Cause**:
There are multiple design flaws in 3D model cache reuse:

1. **STEP export prioritizes network over cache** (most severe): In `Model3DExportWorker::run()`, STEP processing order first calls `downloadStepDataSync()` to make network requests, only checks disk cache after network failure. When batch exporting 100 components, even if all STEPs are cached, 100 network requests are still发起 (still made).

2. **Preload doesn't load OBJ binary data**: `ComponentService::loadComponentDataFromCacheAsync()` loads symbol, footprint, preview, datasheet, but doesn't load OBJ binary data, causing `m_data->model3DObjRaw()` to always be empty, requiring the export stage to re-read from disk or download from network.

3. **Incomplete cache validation**: `ComponentCacheService::hasModel3DCached()` only checks if file exists, doesn't verify if file size is greater than 0, may incorrectly treat corrupted empty files as valid cache.

**Fix**:

1. **STEP cache priority**: Modify `Model3DExportWorker.cpp`, change STEP export logic from "network first then cache" to "cache first then network":
   ```
   Before: downloadStepDataSync() → check cache only after failure
   After: cache->loadModel3D(uuid, "step") → only downloadStepDataSync() when no cache
   ```

2. **Preload OBJ data**: Add OBJ binary data loading in `ComponentService::loadComponentDataFromCacheAsync()`, set to `ComponentData::model3DObjRaw`, to avoid duplicate disk I/O in export stage.

3. **Cache integrity check**: Enhance `hasModel3DCached()` to check both file exists and size > 0.

**Regression Prevention**:
- When adding new cache usage logic, must follow "cache first then network" priority order
- Preload stage should load all required data into memory as much as possible, reduce I/O in export stage
- Footprint stage calculating STEP offset must not depend on parallel 3D model export stage writing cache first; when STEP data is missing, must obtain via "memory → disk cache → network download and save to cache"
- Cache existence check should include basic integrity verification (file size > 0)
- When modifying `Model3DExportWorker` cache logic, must verify:
  - No network request when cache exists
  - Normal download and save to cache when no cache
  - Cache as fallback when network fails
- When modifying `FootprintExportStage` STEP offset logic, must verify:
  - First export and STEP not cached can still calculate Z offset
  - When 3D model export stage and footprint export stage run in parallel, footprint stage doesn't depend on the former's completion order

**Related Files**:
- `src/services/export/Model3DExportWorker.cpp` - STEP cache priority logic
- `src/services/export/FootprintExportStage.cpp` - STEP offset calculation data acquisition logic
- `src/services/ComponentService.cpp` - preload OBJ data
- `src/services/ComponentService.h` - CacheLoadResult adds objData field
- `src/services/ComponentCacheService.cpp` - cache integrity verification

**Status**: ✅ Fixed (2026-05-08), with supplementary footprint stage STEP offset regression fix (2026-05-09)

---

### Q: Exported 3D model filename doesn't match the 3D model path recorded in the footprint

**Problem Description**:
The exported 3D model filename is `C0603.step`, but the 3D model path recorded in the footprint is `../lib.3dmodels/C0603_L1.6-W0.8-H0.8.step`, causing KiCad to fail to find the corresponding 3D model file.

**Root Cause**:
The `name` field in the `Model3DData` object was accidentally lost during data flow transmission, caused by two independent bugs:

**Bug 1 - ProcessWorker reconstructing Model3DData loses attributes**:
When `ProcessWorker::parse3DModelData()` receives 3D model OBJ data, it **recreates** the `Model3DData` object to store the raw data, but the old code only restored `uuid`, while `name`, `translation`, `rotation` were all lost.

**Bug 2 - ComponentService cache loading overwrites existing model3DData**:
`ComponentCacheService::loadComponentData()` has already correctly loaded the complete `model3DData` with `name` from `metadata.json`, but `ComponentService::loadComponentDataFromCacheAsync()` **recreates a model3DData with only UUID**, and uses `setModel3DData()` to overwrite the existing complete data.

**Data Flow Analysis**:
```
EasyEDA API -> FootprintData.model3D().name() = "C0603_L1.6-W0.8-H0.8"
                   |
              【Bug 2】Cache loading -> model3DData()->name() gets overwritten to empty
                   |
              3D model export -> fallback to footprint name -> C0603.step
              Footprint export -> uses original name -> C0603_L1.6-W0.8-H0.8.step (path)
```

**Fix**:
1. `ProcessWorker::parse3DModelData()`: Before reconstructing `Model3DData`, first save and restore `name`, `translation`, `rotation`
2. `ComponentService::loadComponentDataFromCacheAsync()`: Only create/supplement when `cachedData->model3DData()` doesn't exist or UUID is missing, no longer call `setModel3DData()` to overwrite existing data

**Regression Prevention**:
- When reconstructing objects, must backup all attributes of the old object, do not assume the new object only needs partial fields
- Cache loading path should not re-parse/overwrite, supplement logic should be "only supplement when missing", not "unconditionally reconstruct"
- When testing cache scenarios, must verify both: first acquisition (no cache) and cache hit, the behavior should be completely consistent

**Related Files**:
- `src/workers/ProcessWorker.cpp` - restore Model3DData complete attributes
- `src/services/ComponentService.cpp` - no longer overwrite existing model3DData

**Status**: ✅ Fixed (2026-04-29)

---

## Network Related

### Q: High export failure rate in weak network environments

**Problem Description**:
When the network is unstable, many component exports fail.

**Solution**:
Enable "Weak Network Support" option (`weakNetworkSupport`), which can:
- Reduce concurrency
- Increase retry count
- Extend timeout

**Related Configuration**:
- `src/services/ConfigService.cpp` - `getWeakNetworkSupport()` / `setWeakNetworkSupport()`

---

### Q: Non-existent components are incorrectly marked as verification success, and error responses are cached

**Problem Description**:
When requesting a non-existent component (such as C2041), the EasyEDA API returns a business-level 404 error:
```json
{"code":404,"message":"Component not found","success":false}
```

But the system incorrectly treats this response as verification success, causing:
- Component displays as "verification successful" but has no preview image
- Error response is cached to local disk
- Subsequent requests load from cache, continuously showing as success

**Root Cause**:
`src/services/CadDataLoader.cpp:fetchAndParseCadData()` only checks:
- Whether HTTP/network layer succeeded
- Whether JSON can be parsed

But doesn't check business-level error markers in the response body:
- `success: false`
- `code: 404`
- `result` missing or empty

**Fix**:
Add business-level error check in `CadDataLoader::fetchAndParseCadData()`:
1. Check `success == false` → return error (return "Component does not exist (404)" on 404)
2. Check `result` missing or null → return error
3. Check `result` is empty object → return error

**Regression Prevention**:
- Any code that parses API responses must check business-level `success` field
- Do not assume business success just because HTTP succeeds
- When `result` field is missing or empty, must treat as failure

**Related Files**:
- `src/services/CadDataLoader.cpp:117` - fetchAndParseCadData()
- `src/workers/FetchWorker.cpp:203` - reference implementation (already has correct judgment)

**Status**: ✅ Fixed (2026-04-21)

---

### Q: Requests not marked as verification failure when cancelled

**Problem Description**:
When network requests are cancelled (such as when user deletes a component), the system doesn't mark this as verification failure.

**Root Cause**:
`isCadDataFailure` check in `handleFetchError()` doesn't include "Request cancelled" error message.

**Fix**:
Add `error.contains("Request cancelled")` to `isCadDataFailure` check.

**Related Files**:
- `src/ui/viewmodels/ComponentListViewModel.cpp:850` - isCadDataFailure check

**Status**: ✅ Fixed (2026-04-21)

---

### Q: Network requests not cancelled when deleting components, causing cache to be saved

**Problem Description**:
When deleting components, the network requests for those components are not cancelled, causing requests to complete and data to be cached.

**Fix**:
1. Add `cancelRequestForComponent()` method in `ComponentService`
2. Add `cancelRequestForComponent()` method in `LcscImageService`
3. Call `cancelRequestForComponent()` in `ComponentListViewModel::removeComponent()`

**Related Files**:
- `src/services/ComponentService.cpp` - cancelRequestForComponent()
- `src/services/LcscImageService.cpp` - cancelRequestForComponent()
- `src/ui/viewmodels/ComponentListViewModel.cpp:340` - removeComponent() call

**Status**: ✅ Fixed (2026-04-21)

---

## Build Related

### Q: CMake configuration fails with mismatched generator

**Problem Description**:
When running `cmake --build`, error:
```
CMake Error: generator Ninja Does not match the generator used previously: Unix Makefiles
```

**Solution**:
Clean build directory and reconfigure:
```bash
rm -rf build
python tools/python/build_project.py
```

**Status**: ✅ Fixed (2026-04-17)

---

## Window / QML Startup Related

### Q: Application process starts normally but main window doesn't display, no taskbar entry

**Problem Description**:
After startup, logs show QML has loaded, icon has been set, event loop continues, even update check starts executing, but there's no main window on the desktop and no taskbar entry visible.

Typical symptoms:
- `QQmlApplicationEngine` reports no error
- `rootObjects()` is not empty
- Logs show "QML loaded, starting icon setup"
- Application doesn't exit immediately, but window isn't truly mapped to desktop

**Root Cause**:
This is a regression after splitting window startup responsibilities, triggered by the combination of the following factors:

1. The main window is `ApplicationWindow` and uses `Qt.FramelessWindowHint`
2. During startup, window display logic is completely handed over to QML's `WindowStartupManager`
3. `Main.qml` binds `visible` to a constant or startup gate value (e.g., `visible: false` / `visible: startupReady`)
4. QML side tries to display the window at runtime via `show()` / `visible = true`

In this situation, `visible` binding and runtime display calls may override each other. Result:
- QML/Qt objects exist
- Application logic continues
- But the top-level native window isn't stably mapped to the desktop environment

Additionally, this split exposed two high-risk points:

1. New QML components added without simultaneously updating `CMakeLists.txt` and `tests/ui/CMakeLists.txt` will cause "compilation passes but runtime QML component not found" issues
2. New components declaring `required property ApplicationWindow window` must explicitly import `QtQuick.Controls`

**Solution**:

1. Do not completely bind main window display to QML constant properties
2. Allow QML to set geometry and state during startup phase, but provide one-time visibility fallback from C++ after engine loads
3. If root window is still hidden after startup, execute `show()` / `raise()` / `requestActivate()` from `main.cpp`
4. All new QML components must simultaneously update:
   - `src/ui/qml/components/qmldir`
   - `QML_FILES` in root `CMakeLists.txt`
   - `QML_FILES` in `tests/ui/CMakeLists.txt`
5. When using `ApplicationWindow` as type, QML file must import `QtQuick.Controls`

**Related Files**:
- `src/main.cpp` - root window visibility fallback after loading
- `src/ui/qml/Main.qml` - main window initial visibility strategy
- `src/ui/qml/components/WindowController.qml` - startup/runtime window orchestration
- `src/ui/qml/components/WindowStartupManager.qml` - startup phase window restoration
- `src/ui/qml/components/WindowPersistenceManager.qml` - runtime window state persistence
- `CMakeLists.txt` - application QML resource manifest
- `tests/ui/CMakeLists.txt` - UI test QML manifest

**Anti-Regression Rules**:
- Do not use hybrid control of "constant `visible` binding + runtime `show()`" for the main window
- `FramelessWindowHint` main window's first mapping must have C++ side fallback
- When splitting QML components, resource registration, `qmldir` registration, and test manifest must all be changed together
- Startup restoration and runtime persistence can be split into modules, but the final responsibility for "first display" must be clear, cannot be managed half in QML and half in C++

**Recommended Regression Tests**:
- Normal window startup should immediately appear on desktop and taskbar
- After maximizing and closing, restart should restore to maximized state
- From maximized, clicking to windowed should restore to last normal geometry
- After deleting window configuration and restarting, should display centered with default size
- Verify main window truly maps on both Linux/X11 and Linux/Wayland

**Status**: ✅ Fixed (2026-04-19)

---

## UI Scroll Position Related

### Q: Export results list automatically scrolls back to top after updates during conversion

**Problem Description**:
During export, the user scrolls down the results list to view progress, but whenever a component status updates (such as from "exporting" to "success"), the list automatically scrolls back to the top, making it difficult for the user to continuously track components at the bottom.

**Root Cause**:
`ExportResultsCard.qml`'s GridView is directly bound to the `filteredResultsList` property, and the getter method of this property returns a new `QVariantList` on each call:

```cpp
QVariantList ExportProgressViewModel::filteredResultsList() const {
    // Creates new list each time
    QVariantList filtered;
    for (const auto& item : m_resultsList) {
        // filtering logic...
        filtered.append(item);
    }
    return filtered;
}
```

Each time `filteredResultsListChanged()` signal triggers:
1. QML calls getter to get new list
2. GridView receives a brand new list object
3. Scroll position resets to top

Component list uses `DelegateModel` bound to the original model, controlling display/hide through `DelegateModelGroup`'s `inDisplay` property, avoiding position reset.

**Solution**:
Reference `ComponentListCard.qml`'s implementation, use `DelegateModel` instead of direct binding:

1. Add `DelegateModel` bound to original `resultsList`
2. Use `DelegateModelGroup` to control display/hide
3. Add debounce timer (50ms) to avoid frequent filtering
4. Listen to `filterModeChanged` and `resultsListChanged` signals to trigger filtering
5. In `updateFilter()` function, control each item's display through `item.inDisplay`

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
            // filtering logic...
            item.inDisplay = passFilter;
        }
    }
}
```

**Regression Prevention**:
- Any new list view that needs to update data at runtime while maintaining scroll position must use `DelegateModel`
- Do not directly bind to properties that return new objects each time
- List filtering must be controlled through `DelegateModelGroup`'s `inDisplay` property, not by recreating the list
- Use `cacheBuffer: 500` and `reuseItems: true` to enable virtualization and Item recycling

**Related Files**:
- `src/ui/qml/components/ExportResultsCard.qml` - use DelegateModel instead of direct binding
- `src/ui/qml/components/ComponentListCard.qml` - reference implementation

**Status**: ✅ Fixed (2026-04-19)

---

## Regression Test Checklist

Please confirm the following work correctly before each release:

- [ ] 3D model format selection (WRL/STEP/Both)
- [ ] 3D model main checkbox and sub-options linkage
- [ ] After selecting 3D model format, export actually only exports selected format (not Both)
- [ ] Preview images only fetched from fixed API location
- [ ] Export success rate in weak network mode
- [ ] Symbol library, footprint library export functionality
- [ ] After appending new symbols to existing `.kicad_sym`, new symbols visible in KiCad
- [ ] Main window visible after startup, appears in taskbar
- [ ] Maximized after closing and restarting
- [ ] Restores to windowed from maximized, retains last normal geometry
- [ ] When adding new QML components, `qmldir`, CMake resource manifest, UI test manifest are synchronized
- [ ] Export results list maintains scroll position during conversion, doesn't automatically return to top
- [ ] Exporting 3D models with cache doesn't发起网络请求 (don't make network requests) (STEP cache priority)
- [ ] After retrying failed components, exported library contains both successful and retried items
- [ ] After retrying failed components, existing footprints/symbols are not lost

---

## How to Contribute

If you discover new common problems and find solutions, please update this document.

Format requirements:
1. Problem description (clearly describe symptoms)
2. Root cause
3. Solution
4. Related files and status
5. Update date