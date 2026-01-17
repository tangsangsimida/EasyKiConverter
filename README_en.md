# EasyKiConverter 🔄

**[English](README_en.md)** | [中文](README.md)

A powerful C++ desktop application based on Qt 6 Quick and MVVM architecture for converting LCSC and EasyEDA components to KiCad format. Supports complete conversion of symbols, footprints, and 3D models with modern user interface and efficient conversion performance.

## ✨ Features

### 🎯 Core Functions
- **Symbol Conversion**: Convert EasyEDA symbols to KiCad symbol libraries (.kicad_sym)
- **Footprint Generation**: Create KiCad footprints from EasyEDA packages (.kicad_mod)
- **3D Model Support**: Automatically download and convert 3D models (supports WRL, STEP, and other formats)
- **Batch Processing**: Support simultaneous conversion of multiple components
- **Network Retry Mechanism**: Automatic retry on network request failures to improve conversion success rate
- **GZIP Decompression**: Automatically decompress GZIP-encoded response data to reduce data transfer

### 🚀 Performance Optimization
- **Parallel Conversion**: Support multi-threaded parallel processing to fully utilize multi-core CPUs
- **Two-Stage Export**: Parallel data collection, serial data export for optimized batch conversion performance
- **State Machine Pattern**: Async data collection for improved response speed
- **Memory Optimization**: Smart pointer management to reduce memory leaks

### 🎨 User Interface
- **Modern Interface**: Fluent user interface based on Qt 6 Quick
- **Dark Mode**: Support dark/light theme switching
- **Card-Based Layout**: Clear interface organization, easy to use
- **Smooth Animations**: Button hover, card entry, state transition animations
- **Real-time Progress**: Real-time display of conversion progress and status

### 🔧 Advanced Features
- **Layer Mapping System**: Complete EasyEDA to KiCad layer mapping (50+ layers)
- **Polygon Pad Support**: Correct export of custom-shaped pads
- **Elliptical Arc Calculation**: Precise arc calculation supporting complex geometric shapes
- **Text Layer Processing**: Support for type "N" special processing and mirrored text processing
- **Overwrite File Function**: Support overwriting existing KiCad V9 format files
- **Smart Extraction**: Support intelligent extraction of component numbers from clipboard text
- **BOM Import**: Support importing BOM files for batch component conversion

## 🏗️ Project Architecture

This project adopts the **MVVM (Model-View-ViewModel)** architecture pattern, providing clear separation of concerns and efficient code organization.

### Four-Layer Architecture

```
┌─────────────────────────────────────────┐
│              View Layer                  │
│         (QML Components)                 │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│          ViewModel Layer                │
│  ┌──────────────────────────────────┐   │
│  │ ComponentListViewModel          │   │
│  │ ExportSettingsViewModel         │   │
│  │ ExportProgressViewModel         │   │
│  │ ThemeSettingsViewModel          │   │
│  └──────────────────────────────────┘   │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│           Service Layer                  │
│  ┌──────────────────────────────────┐   │
│  │ ComponentService                 │   │
│  │ ExportService                    │   │
│  │ ConfigService                    │   │
│  └──────────────────────────────────┘   │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│            Model Layer                   │
│  ┌──────────────────────────────────┐   │
│  │ ComponentData                    │   │
│  │ SymbolData                       │   │
│  │ FootprintData                    │   │
│  │ Model3DData                      │   │
│  └──────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

### Design Patterns

- **MVVM Pattern**: Clear separation of concerns, View handles UI, ViewModel handles state, Model handles data
- **State Machine Pattern**: Async data collection for improved response speed
- **Two-Stage Export Strategy**: Parallel data collection, serial data export for optimized batch conversion performance

## 💻 Tech Stack

- **Programming Language**: C++17
- **UI Framework**: Qt 6.10.1 (Qt Quick + Qt Quick Controls 2)
- **Build System**: CMake 3.16+
- **Compiler**:
  - Windows: MinGW 13.10 (recommended) or MSVC 2019+
  - macOS: Clang (Xcode 12+)
  - Linux: GCC 9+ or Clang 10+
- **Architecture Pattern**: MVVM (Model-View-ViewModel)
- **Network Library**: Qt Network (with retry mechanism and GZIP support)
- **Multi-threading**: QThreadPool + QRunnable + QMutex
- **Compression Library**: zlib (for GZIP decompression)

## 🚀 Quick Start

### Requirements

- **Operating System**: Windows 10/11 (recommended), macOS, Linux
- **Qt Version**: Qt 6.8 or higher (recommended Qt 6.10.1)
- **CMake Version**: CMake 3.16 or higher
- **Compiler**:
  - Windows: MinGW 13.10 (recommended) or MSVC 2019+
  - macOS: Clang (Xcode 12+)
  - Linux: GCC 9+ or Clang 10+

### Build Instructions

#### Windows + MinGW

```bash
# Create build directory
mkdir build
cd build

# Configure project
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64"

# Build project
cmake --build . --config Debug

# Run application
./bin/EasyKiConverter.exe
```

#### macOS

```bash
# Create build directory
mkdir build
cd build

# Configure project
cmake .. -DCMAKE_PREFIX_PATH="/usr/local/Qt-6.10.1"

# Build project
cmake --build . --config Debug

# Run application
./bin/EasyKiConverter
```

#### Linux

```bash
# Create build directory
mkdir build
cd build

# Configure project
cmake .. -DCMAKE_PREFIX_PATH="/opt/Qt/6.10.1/gcc_64"

# Build project
cmake --build . --config Debug

# Run application
./bin/EasyKiConverter
```

### Using Qt Creator

1. Open `CMakeLists.txt` with Qt Creator
2. Configure build kit:
   - Select Qt version (Qt 6.10.1 or higher)
   - Select compiler (MinGW, Clang, or GCC)
3. Click "Build" button (or press Ctrl+B)
4. Click "Run" button (or press F5) to launch the application

## 📊 Project Status

- **Current Version**: 3.0.0
- **Development Status**: Refactoring complete, entering optimization phase
- **Completion**: ~95% (core features implemented, architecture refactoring complete)
- **Architecture Pattern**: MVVM (Model-View-ViewModel)
- **Last Updated**: January 17, 2026

### Completed Features

- ✅ Basic infrastructure (CMake, Qt Quick framework)
- ✅ Core conversion engine (EasyEDA API, KiCad exporters)
- ✅ Modern UI interface (card-based layout, dark mode, animations)
- ✅ MVVM architecture refactoring
- ✅ Service layer implementation
- ✅ ViewModel layer implementation
- ✅ State machine pattern implementation
- ✅ Two-stage export strategy
- ✅ Parallel conversion support
- ✅ Network request optimization (retry mechanism, GZIP decompression)
- ✅ Layer mapping system (50+ layers)
- ✅ Polygon pad support
- ✅ Elliptical arc calculation
- ✅ Text layer processing logic
- ✅ Overwrite file function
- ✅ Smart extraction feature
- ✅ BOM file import
- ✅ Complete testing framework

### In Progress

- ⏳ Integration testing (complete conversion workflow)
- ⏳ Performance testing and optimization
- ⏳ Compatibility testing

## 🤝 Contributing Guidelines

Contributions are welcome! Before submitting code, please ensure:

### Code Quality

- Code follows Qt coding standards
- Add necessary comments and documentation
- Follow the project's MVVM architecture design
- Pass code style checks

### Testing Requirements

- Test code functionality
- Ensure no new bugs are introduced
- Add unit tests (if applicable)
- Ensure test coverage

### Documentation Updates

- Update relevant documentation
- Add change descriptions
- Update API documentation

### Design Reference

- Reference the project's design patterns
- Maintain consistent code style
- Follow existing architecture design
- Ensure consistency with Python version conversion results

### Commit Guidelines

- Use clear commit messages
- Follow Git commit conventions
- Use semantic commit messages
- Each commit should contain only one logical change

## 📚 Detailed Documentation

For more detailed information, please refer to the documentation in the `docs` directory:

### Project Documentation
- [Project Overview](IFLOW.md) - Project overview and development status
- [Architecture Documentation](docs/ARCHITECTURE.md) - MVVM architecture design documentation
- [Layer Mapping](docs/LAYER_MAPPING.md) - EasyEDA to KiCad layer mapping description

### Refactoring Documentation
- [Refactoring Plan](docs/REFACTORING_PLAN.md) - MVVM refactoring plan
- [Refactoring Summary](docs/REFACTORING_SUMMARY.md) - Refactoring summary and results
- [MainController Migration Plan](docs/MAINCONTROLLER_MIGRATION_PLAN.md) - MainController migration steps
- [MainController Cleanup Plan](docs/MAINCONTROLLER_CLEANUP_PLAN.md) - MainController cleanup steps
- [QML Migration Guide](docs/QML_MIGRATION_GUIDE.md) - QML file migration guide
- [Documentation Update Guide](docs/DOCUMENTATION_UPDATE_GUIDE.md) - Documentation update guide

### Development Documentation
- [Debug Data Export](docs/DEBUG_EXPORT_GUIDE.md) - Debug data export feature usage guide
- [Footprint Parsing Fix](docs/FIX_FOOTPRINT_PARSING.md) - Footprint parsing fix description

### Testing Documentation
- [Unit Testing Guide](tests/TESTING_GUIDE.md) - Unit testing guide
- [Integration Testing Guide](tests/INTEGRATION_TEST_GUIDE.md) - Integration testing guide
- [Performance Testing Guide](tests/PERFORMANCE_TEST_GUIDE.md) - Performance testing guide

## 🔧 Dependency Management

EasyKiConverter uses a layered dependency management strategy:
- **Core Dependencies**: Base dependencies shared by all platforms
- **Platform-Specific Dependencies**: Installed on-demand based on platform and package format

## 📄 License

This project is licensed under **GNU General Public License v3.0 (GPL-3.0)**.

- ✅ Commercial use
- ✅ Modification
- ✅ Distribution
- ✅ Patent use
- ❌ Liability
- ❌ Warranty

See [LICENSE](LICENSE) file for complete license terms.

---

## 🙏 Acknowledgments

### 🌟 Special Thanks

This project references the design and algorithms from **[uPesy/easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py)**. We thank the original author for providing an excellent foundation framework and core conversion algorithms, which laid a solid foundation for the development of this project.

**Note**: This project is an independent C++ implementation and does not contain Python code. The Python version is only referenced for design and algorithms.

### 🤝 Other Acknowledgments

Thanks to [GitHub](https://github.com/) platform and all contributors who have contributed to this project.

We would like to express our sincere gratitude to all the contributors.

<a href="https://github.com/tangsangsimida/EasyKiConverter_QT/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=tangsangsimida/EasyKiConverter_QT" />
</a>

Thanks to [EasyEDA](https://easyeda.com/) and [LCSC](https://www.szlcsc.com/) for providing open APIs.

Thanks to [KiCad](https://www.kicad.org/) open source circuit design software.

Thanks to [Qt](https://www.qt.io/) for providing a powerful cross-platform development framework.

---

**⭐ If this project helps you, please give us a Star!**
