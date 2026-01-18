# EasyKiConverter

[English](README_en.md) | 中文

![GitHub release (latest by date)](https://img.shields.io/github/v/release/tangsangsimida/EasyKiConverter_QT)![GitHub all releases](https://img.shields.io/github/downloads/tangsangsimida/EasyKiConverter_QT/total)![GitHub](https://img.shields.io/github/license/tangsangsimida/EasyKiConverter_QT)![GitHub stars](https://img.shields.io/github/stars/tangsangsimida/EasyKiConverter_QT)![GitHub forks](https://img.shields.io/github/forks/tangsangsimida/EasyKiConverter_QT)

A powerful C++ desktop application based on Qt 6 Quick and MVVM architecture for converting LCSC and EasyEDA components to KiCad format.

## Interface Preview

![EasyKiConverter Interface](https://via.placeholder.com/800x500?text=EasyKiConverter+Interface+Preview)

> Add application screenshots or GIF animations to showcase core features and usage flow.

## Introduction

EasyKiConverter provides complete conversion of symbols, footprints, and 3D models with a modern user interface and efficient conversion performance.

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

For detailed features, please refer to: [Features Documentation](docs/user/FEATURES_en.md)

## Quick Start

### Installation

#### Windows

1. Download the latest version from [GitHub Releases](https://github.com/tangsangsimida/EasyKiConverter_QT/releases)
2. Extract the downloaded archive
3. Double-click `EasyKiConverter.exe` to run the application

#### macOS

1. Download the latest version from [GitHub Releases](https://github.com/tangsangsimida/EasyKiConverter_QT/releases)
2. Extract the downloaded archive
3. Double-click `EasyKiConverter.app` to run the application

#### Linux

1. Download the latest version from [GitHub Releases](https://github.com/tangsangsimida/EasyKiConverter_QT/releases)
2. Extract the downloaded archive
3. Run `./EasyKiConverter` to launch the application

### Building from Source

For detailed build instructions, please refer to: [Build Guide](docs/developer/BUILD_en.md)

## Documentation

### For Users

- [User Guide](docs/user/USER_GUIDE_en.md) - Detailed usage instructions
- [Getting Started](docs/user/GETTING_STARTED_en.md) - Quick start guide
- [FAQ](docs/user/FAQ_en.md) - Frequently asked questions
- [Features](docs/user/FEATURES_en.md) - Detailed feature descriptions

### For Developers

- [Build Guide](docs/developer/BUILD_en.md) - Build from source
- [Contributing Guide](docs/developer/CONTRIBUTING_en.md) - How to contribute code
- [Architecture Documentation](docs/developer/ARCHITECTURE_en.md) - Project architecture design

### Project Planning

- [Changelog](CHANGELOG_en.md) - Version update records
- [Roadmap](docs/project/ROADMAP_en.md) - Future development directions
- [Architecture Decision Records](docs/project/adr/) - Technical decision records

## Contributing

Contributions are welcome! Please read the [Contributing Guide](docs/developer/CONTRIBUTING_en.md) to learn how to participate in project development.

## License

This project is licensed under GNU General Public License v3.0 (GPL-3.0).

See [LICENSE](LICENSE) file for complete license terms.

## Acknowledgments

### Reference Project

This project references the design and algorithms from [uPesy/easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py). Thank you to the original author for providing an excellent foundation framework and core conversion algorithms.

Note: This project is an independent C++ implementation and does not contain Python code. The Python version is only referenced for design and algorithms.

### Contributors

Thank you to all developers who have contributed to the EasyKiConverter project!

![Contributors](https://contrib.rocks/image?repo=tangsangsimida/EasyKiConverter_QT&max=100)

View all contributors: [Contributors List](https://github.com/tangsangsimida/EasyKiConverter_QT/graphs/contributors)

## Contact

If you have any questions or suggestions, please contact the project maintainers through GitHub Issues.

## Project Homepage

[GitHub Repository](https://github.com/tangsangsimida/EasyKiConverter_QT)