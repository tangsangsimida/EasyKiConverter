# EasyKiConverter

English | [中文](README.md)

A C++ desktop application based on Qt 6 Quick and MVVM architecture for converting LCSC and EasyEDA components to KiCad format.

## Introduction

EasyKiConverter provides complete conversion functionality for symbols, footprints, and 3D models with a modern user interface and efficient conversion performance.

## Key Features

- Symbol Conversion: Convert EasyEDA symbols to KiCad symbol libraries (.kicad_sym)
- Footprint Generation: Create KiCad footprints from EasyEDA packages (.kicad_mod)
- 3D Model Support: Automatically download and convert 3D models (supports WRL, STEP, and other formats)
- Batch Processing: Support simultaneous conversion of multiple components
- Modern Interface: Fluent user interface based on Qt 6 Quick
- Dark Mode: Support dark/light theme switching
- Parallel Conversion: Support multi-threaded parallel processing to fully utilize multi-core CPUs
- Smart Extraction: Support intelligent extraction of component numbers from clipboard text
- BOM Import: Support importing BOM files for batch component conversion

For detailed features, see: [Features Documentation](docs/FEATURES.md)

## Quick Start

```bash
# Clone repository
git clone https://github.com/tangsangsimida/EasyKiConverter_QT.git
cd EasyKiConverter_QT

# Create build directory
mkdir build
cd build

# Configure project (Windows + MinGW)
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64"

# Build project
cmake --build . --config Debug

# Run application
./bin/EasyKiConverter.exe
```

For detailed build instructions, see: [Getting Started Guide](docs/GETTING_STARTED.md)

## Documentation

### Project Documentation

- [Project Overview](IFLOW.md) - Project overview and development status
- [Features](docs/FEATURES.md) - Detailed feature descriptions
- [Getting Started](docs/GETTING_STARTED.md) - Quick start guide
- [Architecture Documentation](docs/ARCHITECTURE.md) - MVVM architecture design documentation
- [Layer Mapping](docs/LAYER_MAPPING.md) - EasyEDA to KiCad layer mapping description

### Development Documentation

- [Contributing Guidelines](docs/CONTRIBUTING.md) - How to contribute
- [Debug Data Export](docs/DEBUG_EXPORT_GUIDE.md) - Debug data export feature usage guide
- [Footprint Parsing Fix](docs/FIX_FOOTPRINT_PARSING.md) - Footprint parsing fix description

### Refactoring Documentation

- [Refactoring Plan](docs/REFACTORING_PLAN.md) - MVVM refactoring plan
- [Refactoring Summary](docs/REFACTORING_SUMMARY.md) - Refactoring summary and results
- [MainController Migration Plan](docs/MAINCONTROLLER_MIGRATION_PLAN.md) - MainController migration steps
- [MainController Cleanup Plan](docs/MAINCONTROLLER_CLEANUP_PLAN.md) - MainController cleanup steps
- [QML Migration Guide](docs/QML_MIGRATION_GUIDE.md) - QML file migration guide

### Testing Documentation

- [Unit Testing Guide](tests/TESTING_GUIDE.md) - Unit testing guide
- [Integration Testing Guide](tests/INTEGRATION_TEST_GUIDE.md) - Integration testing guide
- [Performance Testing Guide](tests/PERFORMANCE_TEST_GUIDE.md) - Performance testing guide

## Contributing

Contributions are welcome! Please read the [Contributing Guidelines](docs/CONTRIBUTING.md) to learn how to participate in project development.

## License

This project is licensed under the GNU General Public License v3.0 (GPL-3.0).

See the [LICENSE](LICENSE) file for complete license terms.

## Acknowledgments

This project references the design and algorithms from [uPesy/easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py). We thank the original author for providing an excellent foundation framework and core conversion algorithms.

Note: This project is an independent C++ implementation and does not contain Python code. The Python version is only referenced for design and algorithms.

## Contact

For questions or suggestions, please contact the project maintainers through GitHub Issues.

## Project Homepage

[GitHub Repository](https://github.com/tangsangsimida/EasyKiConverter_QT)