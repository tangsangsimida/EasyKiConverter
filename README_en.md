# EasyKiConverter

English | [中文](README.md)

![GitHub release (latest by date)](https://img.shields.io/github/v/release/tangsangsimida/EasyKiConverter_QT)
![GitHub downloads](https://img.shields.io/github/downloads/tangsangsimida/EasyKiConverter_QT/total)
![License](https://img.shields.io/github/license/tangsangsimida/EasyKiConverter_QT)
![Stars](https://img.shields.io/github/stars/tangsangsimida/EasyKiConverter_QT)

**EasyKiConverter** is a modern C++ desktop tool based on Qt 6 and MVVM architecture, designed for electronics engineers to efficiently convert component data from LCSC and EasyEDA into KiCad libraries.

## ✨ Key Features

*   **Complete Conversion**: Full export support for Symbols (.kicad_sym), Footprints (.kicad_mod), and 3D Models (STEP/WRL).
*   **Efficient Batch Processing**: Multi-threaded parallel conversion and BOM file import support to fully utilize multi-core performance.
*   **Modern Experience**: Fluid UI based on Qt Quick with support for Dark/Light theme switching.
*   **Smart Assistance**: Intelligent extraction of component numbers from the clipboard.

## 🚀 Quick Start

### Installation
Please visit the [Releases](https://github.com/tangsangsimida/EasyKiConverter_QT/releases) page to download the latest version for Windows, macOS, or Linux. simply extract and run.

### Build from Source
If you are a developer or wish to compile it yourself, please refer to the [Build Guide](docs/developer/BUILD_en.md).

## 📚 Documentation

**User Guide**
*   [Getting Started](docs/user/GETTING_STARTED_en.md) | [User Manual](docs/user/USER_GUIDE_en.md) | [FAQ](docs/user/FAQ_en.md)
*   [Detailed Features](docs/user/FEATURES_en.md)

**Developer Resources**
*   [Contributing Guide](docs/developer/CONTRIBUTING_en.md) | [Architecture](docs/developer/ARCHITECTURE_en.md) | [Build Guide](docs/developer/BUILD_en.md)
*   [Performance Report](docs/PERFORMANCE_OPTIMIZATION_REPORT_en.md) | [Roadmap](docs/project/ROADMAP_en.md)

## 📅 Latest Updates (v3.0.0)

This update introduces a brand new **Three-Stage Pipeline Architecture**, significantly improving conversion efficiency:
*   **Performance Leap**: Total time reduced by 54%, throughput increased by 117%.
*   **Resource Optimization**: Memory usage reduced by 50%, with significantly improved CPU utilization.
*   **Enhanced Features**: Complete 3D model support and batch processing capabilities.

> View full changelog: [CHANGELOG.md](docs/developer/CHANGELOG_en.md)

## 🤝 Contribution & Credits

This project references core algorithms from [easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py). Thanks to the original author for their contribution.
Issues and Pull Requests are welcome! Please see the [Contributing Guide](docs/developer/CONTRIBUTING_en.md) for details.

## 📄 License

This project is licensed under the **GPL-3.0** License. See the [LICENSE](LICENSE) file for details.
