<h1 align="center">EasyKiConverter</h1>
<p align="center">
  English | <a href="README.md">中文</a>
</p>

<p align="center">
  <a href="https://github.com/tangsangsimida/EasyKiConverter_QT/actions/workflows/build.yml">
    <img src="https://github.com/tangsangsimida/EasyKiConverter_QT/workflows/Build/badge.svg" alt="Build Status" />
  </a>
  <a href="https://github.com/tangsangsimida/EasyKiConverter_QT/actions/workflows/security.yml">
    <img src="https://github.com/tangsangsimida/EasyKiConverter_QT/workflows/Security/badge.svg" alt="Security Scan" />
  </a>
  <a href="https://github.com/tangsangsimida/EasyKiConverter_QT/actions/workflows/clang-format.yml">
    <img src="https://github.com/tangsangsimida/EasyKiConverter_QT/workflows/Code%20Format%20Check/badge.svg" alt="Code Format" />
  </a>
  <img src="https://img.shields.io/badge/dynamic/json?url=https://api.github.com/repos/tangsangsimida/EasyKiConverter_QT&query=$.created_at&label=Active%20Days&color=blue" alt="Active Days" />
</p>
<p align="center">
  <a href="https://github.com/tangsangsimida/EasyKiConverter_QT/releases/latest">
    <img src="https://img.shields.io/github/v/release/tangsangsimida/EasyKiConverter_QT" alt="GitHub release" />
  </a>
  <a href="https://github.com/tangsangsimida/EasyKiConverter_QT/releases/latest">
    <img src="https://img.shields.io/github/downloads/tangsangsimida/EasyKiConverter_QT/latest/total" alt="GitHub downloads (latest)" />
  </a>
  <a href="https://github.com/tangsangsimida/EasyKiConverter_QT/releases/latest">
    <img src="https://img.shields.io/github/downloads/tangsangsimida/EasyKiConverter_QT/total" alt="GitHub downloads (total)" />
  </a>
  <img src="https://img.shields.io/github/license/tangsangsimida/EasyKiConverter_QT" alt="License" />
  <img src="https://img.shields.io/github/stars/tangsangsimida/EasyKiConverter_QT" alt="Stars" />
  <img src="https://img.shields.io/github/forks/tangsangsimida/EasyKiConverter_QT" alt="Forks" />
  <img src="https://img.shields.io/badge/PRs-welcome-brightgreen.svg" alt="PR Welcome" />
  <img src="https://img.shields.io/github/contributors/tangsangsimida/EasyKiConverter_QT" alt="Contributors" />
</p>
<p align="center">
  <img src="https://img.shields.io/github/created-at/tangsangsimida/EasyKiConverter_QT" alt="Created at" />
  <img src="https://img.shields.io/github/issues/tangsangsimida/EasyKiConverter_QT" alt="Issues" />
  <img src="https://img.shields.io/github/issues-pr/tangsangsimida/EasyKiConverter_QT" alt="Pull requests" />
  <img src="https://img.shields.io/github/repo-size/tangsangsimida/EasyKiConverter_QT" alt="Repo size" />
  <img src="https://img.shields.io/badge/made%20with-C%2B%2B17-blue" alt="Made with C++17" />
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey" alt="Platform" />
</p>

**EasyKiConverter** is a modern C++ desktop tool based on Qt 6 and MVVM architecture, designed for electronics engineers to efficiently convert component data from LCSC and EasyEDA into KiCad libraries.

## Key Features

*   **Complete Conversion**: Full export support for Symbols (.kicad_sym), Footprints (.kicad_mod), and 3D Models (STEP/WRL).
*   **Multi-unit Symbols**: Support for multi-unit symbol conversion.
*   **Efficient Batch Processing**: Multi-threaded parallel conversion and BOM file import support to fully utilize multi-core performance.
*   **Modern Experience**: Fluid UI based on Qt Quick, supporting dark/light theme switching.
*   **Smart Configuration**: Real-time auto-saving of settings, with memory for last-used state and debug mode restoration.
*   **Smart Assistance**: Intelligent extraction of component IDs from the clipboard.
*   **LCSC Preview Images**: Automatically fetch LCSC component preview images with thumbnail display and hover preview support.

## Quick Start

### Installation
Please visit the [Releases](https://github.com/tangsangsimida/EasyKiConverter_QT/releases) page to download the version for your platform:

*   **Windows**: Recommended to download `.exe` installer (includes complete runtime), or download `.zip` portable version.
*   **Linux**: Recommended to download `.AppImage` (no installation required, just grant execute permission and run), or `.tar.gz` archive.
*   **macOS**: Download `.dmg` image file.

### Build from Source
This project has implemented full-platform CI/CD automated building. If you are a developer or wish to compile it yourself, please refer to the [Build Guide](docs/developer/BUILD_en.md).

## System Architecture

#### Workflow Diagram

![Project Workflow Diagram](docs/diagrams/EasyKiConverter_Workflow.svg)

## Documentation

**User Guide**
*   [Getting Started](docs/user/GETTING_STARTED.md) | [User Manual](docs/user/USER_GUIDE.md) | [FAQ](docs/user/FAQ.md)
*   [Detailed Features](docs/user/FEATURES.md)

**Developer Resources**
*   [Contributing Guide](docs/developer/CONTRIBUTING.md) | [Architecture](docs/developer/ARCHITECTURE.md) | [Build Guide](docs/developer/BUILD.md)

## Contribution & Credits

### Contributors

We would like to thank the following contributors to EasyKiConverter:

<a href="https://github.com/tangsangsimida/EasyKiConverter_QT/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=tangsangsimida/EasyKiConverter_QT&max=50" />
</a>

Issues and Pull Requests are welcome! Please see the [Contributing Guide](docs/developer/CONTRIBUTING.md) for details.

## License

This project is licensed under the **GPL-3.0** License. See the [LICENSE](LICENSE) file for details.
