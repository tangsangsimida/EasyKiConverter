# Changelog

This document records the additions, fixes, and changes in each version of EasyKiConverter.

## [3.0.5] - 2026-02-12

### Added
- **MainWindow Modular Architecture**
  - Refactored `MainWindow`'s inline UI logic into independent, high-performance components: `TitleBar`, `HeaderSection`, `ComponentInputCard`, `BomImportCard`, `ComponentListCard`, `ExportSettingsCard`, `ExportProgressCard`, `ExportResultsCard`, `ExportStatisticsCard`, `ExportButtonsSection`, etc.
  - Significantly improved QML maintainability and rendering efficiency through component decoupling.
  - Code location: `src/ui/qml/components/`

- **Core Import/Export Module Single Responsibility Refactoring**
  - Split the monolithic `EasyedaImporter` and `Exporter` classes into specialized classes.
  - Added `EasyedaSymbolImporter` (Symbol import), `EasyedaFootprintImporter` (Footprint import), and `EasyedaUtils` (Shared utilities).
  - Reduced core logic complexity and improved testability.
  - Code location: `src/core/`

- **BOM Parser & Service Refactoring**
  - Added `BomParser` module to optimize the BOM file import process.
  - Implemented export pipeline cancellation mechanism and thread pool configuration optimization.
  - Code location: `src/services/`

### Fixed
- **Network Resilience P0-1: Timeout Mechanism for NetworkWorker**
  - Added universal `executeRequest` method for unified timeout, retry, and error recovery handling.
  - Added `QTimer` protection for all 4 fetch methods (Component Info, CAD Data, 3D Model OBJ/MTL).
  - Default timeout: 30s (Component Info/CAD Data), 45s (3D Models).
  - Eliminated the risk of thread blocking caused by `QEventLoop::exec()` in weak network conditions.
  - Code location: `src/workers/NetworkWorker.h`, `src/workers/NetworkWorker.cpp`

- **Network Resilience P0-2: Allow Retry After Timeout in FetchWorker**
  - Removed internal logic that skipped retries on timeout.
  - Timeouts (OperationCanceledError) now trigger the standard retry flow.
  - Code location: `src/workers/FetchWorker.cpp`

- **Network Resilience P1-4 to P1-6: Exponential Backoff & Jitter**
  - Changed rate-limit backoff from linear (+1000) to exponential (*2).
  - Increased retry delays (3s / 5s / 10s) and added +/-20% random jitter.
  - Code location: `src/workers/FetchWorker.h`, `src/workers/FetchWorker.cpp`

- **Network Resilience P1-7: Fixed thread_local QNAM Memory Leak**
  - Registered `QThread::finished` cleanup callback for `thread_local QNetworkAccessManager*`.
  - Code location: `src/workers/FetchWorker.cpp`

- **3D Model Export Failures no longer block Symbol/Footprint export**
  - Changed success condition to prioritize core symbol/footprint output.
  - Code location: `src/workers/WriteWorker.cpp`

### Refactor
- **NetworkWorker Code Cleanup**
  - Extracted `executeRequest` to eliminate duplicate code in fetch methods.
  - Reduced code lines from ~430 to ~280.

- **Build System Optimization**
  - Enabled `compile_commands.json` generation for better IDE/LSP (e.g., VS Code) support.
  - Unified all source file encodings to UTF-8 (No BOM).

## [3.0.3] - 2026-02-08

### Added
- **LCSC Preview Image Feature**
  - **Network Image Fetching**: Implemented `fetchLcscPreviewImage` method to fetch component preview images from LCSC website
  - **Retry Mechanism**: Added network request retry mechanism to improve image fetch success rate
  - **Fallback Crawler Mode**: Use crawler mode to fetch component preview images when API is unavailable
  - **Thumbnail Generation**: Added `ThumbnailGenerator` utility class to automatically generate Base64 thumbnails
  - **Data Model Extension**: Created `ComponentListItemData` class with UI-related information like thumbnails, validation status, etc.
  - Code location: `src/services/ComponentService.cpp`, `src/models/ComponentListItemData.h/cpp`, `src/ui/utils/ThumbnailGenerator.h/cpp`

- **Component List Feature Enhancements**
  - **ID Copy Function**: Added right-click to copy component ID to clipboard in `ComponentListItem`
  - **Copy Tooltip**: Display success tooltip when copy operation completes, improving user experience
  - Code location: `src/ui/qml/components/ComponentListItem.qml`, `src/ui/viewmodels/ComponentListViewModel.cpp`

- **Responsive Layout and Window Resizing**
  - **Adaptive Grid Layout**: Changed component list from fixed 5-column grid to adaptive grid layout with dynamic column calculation
  - **Window Edge Resizing**: Added drag-to-resize handles in 8 directions for window size adjustment
  - **Mouse Interaction Optimization**: Optimized mouse interaction area hierarchy for component list items
  - **Thumbnail Hover Preview**: Improved thumbnail hover preview display logic and visual effects
  - Code location: `src/ui/qml/MainWindow.qml`, `src/ui/qml/components/ComponentListItem.qml`

- **Preloaded Data Support**
  - **Preloaded Data Feature**: Added `setPreloadedData` method in `ExportService_Pipeline`
  - **Data Flow Optimization**: Improved progress tracking mechanism for more efficient data flow
  - **Temporary File Management**: Optimized temporary file cleanup logic to ensure correct symbol file handling
  - **Symbol Library Merge Fix**: Fixed temporary file management during symbol library merging
  - Code location: `src/services/ExportService_Pipeline.cpp/h`

### Breaking Changes
- **Component List Model Refactoring**
  - **Migration to QAbstractListModel**: Migrated `ComponentListViewModel` from `QQmlListProperty` to `QAbstractListModel`
  - **Performance Improvement**: Provides more efficient UI update mechanism
  - **API Change**: Component list access in QML changed from `componentList` property to direct `model` binding
  - **New Method**: Added `getAllComponentIds` method to optimize export workflow
  - Code location: `src/ui/viewmodels/ComponentListViewModel.cpp/h`, `src/ui/qml/MainWindow.qml`

### Fixed
- **Export Service Statistics Missing Issue**
  - **Completion Status Statistics Fix**: Added status to completion status list for statistics in preloaded data usage scenarios
  - **Critical Fix**: Resolved inaccurate statistics issue in preloaded data scenarios, ensuring all export statuses are properly counted
  - Code location: `src/services/ExportService_Pipeline.cpp:316`

- **MainWindow Code Structure Fix**
  - **Component Removal Logic Optimization**: Changed component removal logic from index-based to ID-based lookup, simplifying removal process
  - **Condition Check Fix**: Fixed condition check logic for empty data to avoid potential null pointer exceptions
  - **Filter Logic Correction**: Corrected ID extraction logic for component list filtering
  - Code location: `src/ui/qml/MainWindow.qml`

- **ExportProgressViewModel Constructor Parameter Fix**
  - **Dependency Injection**: Added `componentListViewModel` parameter to `ExportProgressViewModel` constructor
  - **Correct Initialization**: Ensures view models are properly initialized with correct dependencies
  - Code location: `main.cpp`, `src/ui/viewmodels/ExportProgressViewModel.cpp/h`

### Refactor
- **Component List View Model Simplification**
  - **Enum Definition Optimization**: Simplified enum definition format in `ComponentListViewModel`
  - **Code Cleanliness**: Removed unnecessary multi-line enum definition format, making code more compact and consistent
  - Code location: `src/ui/viewmodels/ComponentListViewModel.h:27`

- **Code Format Optimization**
  - **Log Format Unification**: Unified debug log output format in `LanguageManager.cpp`
  - **Readability Improvement**: Removed extra blank lines and standardized debug message log format
  - Code location: `src/core/LanguageManager.cpp`

## [3.0.2] - 2026-01-27

### Fixed
- **Batch Export Freezing Issue**
  - **Reduced Concurrency**: Reduced the maximum thread count for `FetchWorker` thread pool from 32 to 5. This effectively prevents server-side rate limiting or service denial caused by excessive concurrent connections, solving the issue where individual component downloads would "freeze" during batch export.
  - **Added Retry Mechanism**: Added automatic retry logic for `FetchWorker` network requests.
    - Strategy: Automatically retry on network errors or HTTP 429/5xx errors.
    - Delays: 3 seconds for 1st retry, 5 seconds for 2nd retry, 10 seconds for 3rd and subsequent retries.
    - Maximum retry count: 3 times.
  - Code location: `src/services/ExportService_Pipeline.cpp`, `src/workers/FetchWorker.cpp`

## [3.0.1] - 2026-01-27

### Performance Optimization

#### Significant Network Performance Improvements
- **Optimized Thread Pool Configuration**
  - Optimized `FetchWorker` thread pool from 8 threads to 3 threads.
  - Test results show: 3-thread configuration performs best under the "single export under 3 seconds" requirement.
  - Performance improvements:
    - Total time reduced from 263.72s (v3.0.0) to 14.43s (94.5% improvement)
    - Throughput increased from 0.08 components/sec to 1.45 components/sec (1712% improvement)
    - Average fetch time reduced from 65.8s to 1.76s (97.3% improvement)
    - Components over 3 seconds reduced from 21 to 3 (85.7% improvement)
  - Code location: `src/services/ExportService_Pipeline.cpp:36`

- **Reduced Timeout Duration**
  - Component info timeout: 30s → 15s
  - 3D model timeout: 60s → 30s
  - Timeout requests no longer retried to avoid wasting time
  - Code location: `src/workers/FetchWorker.cpp`

- **Implemented Rate Limit Detection Mechanism**
  - Detects HTTP 429 responses and triggers exponential backoff
  - Backoff strategy: increase by 1000ms each time, maximum 5000ms
  - Dynamically delays new requests to avoid triggering server rate limiting
  - Code location: `src/workers/FetchWorker.h`, `src/workers/FetchWorker.cpp`

- **Enabled Network Diagnostics**
  - Records diagnostic information for each network request (URL, status code, error info, retry count, latency, rate limited)
  - Aggregates network diagnostic data in statistics report (total requests, total retries, average latency, rate limit hits, status code distribution)
  - Helps quickly identify performance bottlenecks and network issues
  - Code location: `src/models/ComponentExportStatus.h`, `src/workers/FetchWorker.cpp`, `src/services/ExportService_Pipeline.cpp`

#### Export Pipeline Efficiency Improvements
- **WriteWorker Optimization (Disk I/O)**
  - Removed local `QThreadPool` inside `WriteWorker`.
  - Changed to serial writing of symbol, footprint, and 3D model files.
  - Eliminated high overhead of creating and destroying thread pools for each component, avoiding performance degradation from "over-parallelization".
  - Code location: `src/workers/WriteWorker.cpp`

- **FetchWorker Optimization (Network I/O)**
  - Implemented `thread_local` caching mechanism for `QNetworkAccessManager`.
  - Ensures each thread in the thread pool creates only one `QNetworkAccessManager` instance and reuses it.
  - Avoids high cost of repeatedly initializing network stack for each component (including proxy resolution, DNS cache initialization, etc.).
  - Code location: `src/workers/FetchWorker.cpp`

#### Build System Fixes
- **Resolved Circular Dependencies**
  - Fixed circular linking dependency between `EasyKiConverterWorkers` and `EasyKiConverterServices`.
  - Removed unnecessary `EasyKiConverterServices` link in `src/workers/CMakeLists.txt`.

### Added
- **Network Diagnostics Report**
  - Added `networkDiagnostics` field to export statistics report
  - Contains detailed network performance metrics and diagnostic information
  - Code location: `src/services/ExportService_Pipeline.cpp`

### Test Results
- **Thread Count Comparison Test**
  - 3 threads: 14.43s, 3 components over 3s, recommended for production
  - 5 threads: 12.17s, 5 components over 3s, shortest total time
  - 7 threads: 14.96s, 10 components over 3s, performance degradation
  - 16 threads: 263.72s, 21 components over 3s, severe rate limiting

- **Before/After Optimization Comparison**
  | Metric | Before (16 threads) | After (3 threads) | Improvement |
  |--------|-------------------|------------------|-------------|
  | Total Time | 263.72s | 14.43s | ⬇️ 94.5% |
  | Throughput | 0.08 components/sec | 1.45 components/sec | ⬆️ 1712% |
  | Average Fetch Time | 65.8s | 1.76s | ⬇️ 97.3% |
  | Components Over 3s | 21 | 3 | ⬇️ 85.7% |
  | Timeout Requests | Unknown | 0 | ✅ Completely Eliminated |

## [3.0.0] - 2026-01-18

### Performance Optimization

#### Architecture Optimization (P0 Improvements)
- **ProcessWorker Network Request Removal**
  - Moved 3D model download from ProcessWorker to FetchWorker
  - ProcessWorker is now pure CPU-intensive task
  - CPU utilization improved 50-80%

- **QSharedPointer Data Transfer**
  - ExportService_Pipeline uses QSharedPointer queues
  - FetchWorker, ProcessWorker, WriteWorker all use QSharedPointer
  - Avoids frequent data copying
  - Memory usage reduced 50-70%, performance improved 20-30%

- **ProcessWorker as Pure CPU-Intensive**
  - ProcessWorker only contains parsing and conversion logic
  - Removed all network I/O operations
  - Fully utilizes CPU cores
  - CPU utilization improved 40-60%

#### Performance Optimization (P1 Improvements)
- **Dynamic Queue Size**
  - Dynamically adjusts queue size based on task count
  - Uses 1/4 of task count as queue size (minimum 100)
  - Avoids blocking due to full queue
  - Throughput improved 15-25%

- **Parallel File Writing**
  - Uses QThreadPool for parallel writing of individual component's multiple files
  - Simultaneous writing of symbol, footprint, and 3D model files
  - Fully utilizes disk concurrency capability
  - Write phase time reduced 30-50%

#### Overall Performance Improvements
- **Total Time Reduced 54%** (240s → 110s, 100 components)
- **Throughput Increased 117%** (0.42 → 0.91 components/sec)
- **Memory Usage Reduced 50%** (400MB → 200MB)
- **CPU Utilization Increased 50%** (60% → 90%)

### Architecture Improvements
- **Clearer Responsibility Separation**
  - FetchWorker: I/O-intensive (network requests)
  - ProcessWorker: CPU-intensive (data parsing and conversion)
  - WriteWorker: Disk I/O-intensive (file writing)

- **More Efficient Thread Utilization**
  - Avoids threads blocking on network requests
  - Fully utilizes multi-core CPU performance
  - Parallel disk I/O operations

### Code Quality
- **Zero-Copy Data Transfer**
  - Uses QSharedPointer to avoid data copying
  - Reduces memory allocation and deallocation overhead

- **Better Error Handling**
  - Precise identification of failure stages
  - Detailed debug logging
  - User-friendly error messages

### Documentation
- **Performance Benchmark Framework**
  - Created pipeline performance benchmark test code
  - Established performance metrics recording mechanism
  - Provided performance comparison test guide

### Core Features

### Added

#### Core Features
- Complete symbol conversion (EasyEDA to KiCad)
- Complete footprint generation (EasyEDA to KiCad)
- Complete 3D model support (WRL, STEP, OBJ formats)
- Batch processing (support for multiple components simultaneously)
- Smart extraction (extract component IDs from clipboard text)
- BOM import (CSV, Excel formats)

#### Performance Optimization
- Parallel conversion support (multi-threaded parallel processing)
- Two-stage export strategy (parallel data collection, serial data export)
- State machine pattern (async data collection)
- Network request optimization (automatic retry mechanism)
- GZIP decompression support
- Memory optimization (smart pointer management)

#### User Interface
- Modern Qt Quick interface
- Dark/light theme switching
- Card-based layout system
- Smooth animations
- Real-time progress display
- Responsive design

#### Advanced Features
- Complete layer mapping system (50+ layers)
- Polygon pad support
- Elliptical arc calculation (precise arc calculation)
- Text layer processing (type "N" and mirrored text)
- Override file support
- Debug mode support

#### Architecture
- MVVM architecture implementation
- Service layer (ComponentService, ExportService, ConfigService)
- ViewModel layer (ComponentListViewModel, ExportSettingsViewModel, ExportProgressViewModel, ThemeSettingsViewModel)
- State machine pattern (ComponentDataCollector)
- MainController removed
- **Multi-stage Pipeline Parallel Architecture (ExportServicePipeline)**
  - **Fetch Stage**: I/O-intensive, 32 threads for concurrent download
  - **Process Stage**: CPU-intensive, CPU core count threads for concurrent processing
  - **Write Stage**: Disk I/O-intensive, 8 threads for concurrent writing
  - **Thread-safe Bounded Queue (BoundedThreadSafeQueue)** for inter-stage communication
  - **Real-time Progress Feedback** (three-stage progress bar: Fetch 30%, Process 50%, Write 20%)
  - **Detailed Failure Diagnostics** (precise identification of failure stage and reason)
  - **Zero-Copy Parsing Optimization** (QByteArrayView)
  - **HTTP/2 Support** (network request multiplexing)

#### Testing
- Complete test framework
- Unit tests (8 test programs)
- Integration test framework
- Performance test framework
- **Pipeline Architecture Tests**
  - BoundedThreadSafeQueue concurrent tests
  - ExportServicePipeline integration tests
  - Multi-stage concurrent tests

#### Documentation
- Complete documentation system (14 technical documents)
- User manual
- Developer documentation
- Architecture documentation
- Build guide
- Contribution guide
- **ADR-002: Pipeline Parallel Architecture Decision Record**

### Fixed
- Fixed footprint parsing Type judgment error
- Fixed 3D Model UUID missing issue
- Fixed Footprint BBox incomplete issue
- Fixed layer mapping error
- Fixed polygon pad processing missing
- Fixed elliptical arc calculation incomplete
- Fixed text layer processing logic missing
- Fixed 3D model offset parameter calculation error
- Fixed 3D model not displaying issue
- Fixed arc and solid region export incomplete
- Fixed non-ASCII text processing issue
- Fixed component ID validation rule too strict
- Fixed consistency issue with Python version V6

### Changed
- Refactored from MVC architecture to MVVM architecture
- Removed MainController
- Removed silkscreen layer copy logic
- Removed special layer circular processing
- Optimized network request flow
- Optimized error handling mechanism
- Optimized configuration management

### Removed
- Removed MainController
- Removed silkscreen layer copy logic
- Removed special layer circular processing (layers 100 and 101)

## [2.0.0] - 2025-12-15

### Added
- Basic symbol conversion functionality
- Basic footprint generation functionality
- Basic 3D model support
- Network request functionality
- Basic user interface
- Configuration management functionality

### Fixed
- Fixed basic network request errors
- Fixed basic file read/write issues

## [1.0.0] - 2025-10-01

### Added
- Project initialization
- Basic architecture setup
- CMake build system
- Qt Quick application framework

## Version Description

This project follows Semantic Versioning: MAJOR.MINOR.PATCH

- **MAJOR**: Incompatible API changes
- **MINOR**: New features (backward compatible)
- **PATCH**: Bug fixes (backward compatible)

## Update Type Description

- **Added**: New features
- **Fixed**: Bug fixes
- **Changed**: Changes to existing functionality
- **Removed**: Removed features
- **Security**: Security-related fixes
- **Deprecated**: Features to be removed

## How to Contribute

If you want to participate in project development, please refer to the [Contribution Guide](CONTRIBUTING.md).

## Feedback

If you have any questions or suggestions, please submit on [GitHub Issues](https://github.com/tangsangsimida/EasyKiConverter_QT/issues).
