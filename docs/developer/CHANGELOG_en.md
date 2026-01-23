# Changelog

This document records the additions, fixes, and changes in each version of EasyKiConverter.

## [3.0.0] - 2026-01-17

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
- Overwrite file functionality
- Debug mode support

#### Architecture
- MVVM architecture implementation
- Service layer (ComponentService, ExportService, ConfigService)
- ViewModel layer (ComponentListViewModel, ExportSettingsViewModel, ExportProgressViewModel, ThemeSettingsViewModel)
- State machine pattern (ComponentDataCollector)
- MainController removal
- **Multi-stage pipeline parallelism architecture (ExportServicePipeline)**
  - **Fetch Stage**: I/O-intensive, 32 threads for concurrent downloads
  - **Process Stage**: CPU-intensive, CPU core count threads for concurrent processing
  - **Write Stage**: Disk I/O-intensive, 8 threads for concurrent writes
  - **Thread-safe bounded queues (BoundedThreadSafeQueue)** for inter-stage communication
  - **Real-time progress feedback** (three-stage progress bars: Fetch 30%, Process 50%, Write 20%)
  - **Detailed failure diagnostics** (precise identification of failed stage and cause)
  - **Zero-copy parsing optimization** (QByteArrayView)
  - **HTTP/2 support** (network request multiplexing)

#### Testing
- Complete testing framework
- Unit tests (8 test programs)
- Integration testing framework
- Performance testing framework
- **Pipeline architecture testing**
  - BoundedThreadSafeQueue concurrency tests
  - ExportServicePipeline integration tests
  - Multi-stage concurrent tests

#### Documentation
- Complete documentation system (14 technical documents)
- User manual
- Developer documentation
- Architecture documentation
- Build guide
- Contributing guide
- **ADR-002: Pipeline Parallelism Architecture Decision Record**

### Fixed
- Fixed footprint parsing Type judgment error
- Fixed 3D Model UUID omission
- Fixed Footprint BBox incompleteness
- Fixed layer mapping errors
- Fixed missing polygon pad processing
- Fixed incomplete elliptical arc calculation
- Fixed missing text layer processing logic
- Fixed 3D model offset parameter calculation error
- Fixed 3D model not displaying
- Fixed incomplete arc and solid area export
- Fixed non-ASCII text processing issues
- Fixed overly strict component ID validation rules
- Fixed consistency issues with Python version V6

### Changed
- Refactored from MVC architecture to MVVM architecture
- Removed MainController
- Removed silkscreen layer copy logic
- Removed special layer circle processing
- Optimized network request flow
- Optimized error handling mechanism
- Optimized configuration management

### Removed
- Removed MainController
- Removed silkscreen layer copy logic
- Removed special layer circle processing (layers 100 and 101)

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
- Basic infrastructure setup
- CMake build system
- Qt Quick application framework

## Version Notes

This project follows semantic versioning: MAJOR.MINOR.PATCH

- **MAJOR**: Incompatible API changes
- **MINOR**: New features (backwards compatible)
- **PATCH**: Bug fixes (backwards compatible)

## Update Type Explanations

- **Added**: New features
- **Fixed**: Bug fixes
- **Changed**: Changes to existing functionality
- **Removed**: Removed features
- **Security**: Security-related fixes
- **Deprecated**: Features to be removed

## How to Contribute

If you want to participate in project development, please refer to the [Contributing Guide](docs/CONTRIBUTING_en.md).

## Feedback

If you have any questions or suggestions, please submit them on [GitHub Issues](https://github.com/tangsangsimida/EasyKiConverter_QT/issues).