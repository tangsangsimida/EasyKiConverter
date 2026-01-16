# EasyKiConverter 🔄

**[English](README_en.md)** | [中文](README.md)

A powerful Python tool for converting LCSC and EasyEDA components to KiCad format, supporting complete conversion of symbols, footprints, and 3D models. Features a modern PyQt6 desktop interface with card-based layout that makes component conversion simple and efficient.

## ✨ Features

### 🎯 Core Functions
- **Symbol Conversion**: Convert EasyEDA symbols to KiCad symbol libraries (.kicad_sym)
- **Footprint Generation**: Create KiCad footprints from EasyEDA packages (.kicad_mod)
- **3D Model Support**: Automatically download and convert 3D models (multiple formats supported)
- **Batch Processing**: Support simultaneous conversion of multiple components
- **Multi-threading Optimization**: Parallel processing of multiple components for significantly improved efficiency

### 🖥️ PyQt6 Desktop Interface
- **Modern Interface**: Beautiful modern design with card-based layout
- **Real-time Progress**: Visual progress bar for conversion process with parallel processing status
- **Flexible Input**: Support LCSC part numbers
- **Selective Export**: Choose to export symbols, footprints, or 3D models
- **Instant Preview**: Real-time display of conversion results with processing time and file statistics
- **Smart Configuration**: Auto-save export settings with BOM file parsing support
- **Responsive Layout**: Adaptive interface for different screen sizes

### 🛠️ User-Friendly Design
- **Intuitive Layout**: Clear top-to-bottom workflow with card-based interface

- **One-Click Launch**: Start desktop app with a simple double-click

- **Zero Configuration**: Ready to use out of the box

- **Cross-Platform**: Supports Windows, macOS, and Linux systems

- **Multi-Architecture**: Supports x86, x64, Intel, and Apple Silicon architectures

- **Multiple Package Formats**: Available as EXE, binary, DEB, RPM, and Tarball distribution formats

- **Distribution-Specific Packages**: Provides dedicated packages for Ubuntu, Fedora, Arch Linux, and other major distributions

  

## 📚 Detailed Documentation

For more detailed information, please refer to the documentation in the `docs` directory:

- [Project Structure](docs/project_structure.md) - Detailed project structure and module descriptions
- [Development Guide](docs/development_guide.md) - Development environment setup and workflow
- [Contributing Guide](docs/contributing.md) - How to contribute to the project
- [Performance Optimization](docs/performance.md) - Multi-threading parallel processing and performance improvements
- [System Requirements](docs/system_requirements.md) - System requirements and supported component types

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

This project is derived from **[uPesy/easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py)**. We thank the original author for providing an excellent foundation framework and core conversion algorithms, which laid a solid foundation for the development of this project.

### 🤝 Other Acknowledgments

Thanks to [GitHub](https://github.com/) platform and all contributors who have contributed to this project.

We would like to express our sincere gratitude to all the contributors.

<a href="https://github.com/tangsangsimida/EasyKiConverter/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=tangsangsimida/EasyKiConverter" />
</a>

Thanks to [EasyEDA](https://easyeda.com/) and [LCSC](https://www.szlcsc.com/) for providing open APIs.

Thanks to [KiCad](https://www.kicad.org/) open source circuit design software.

---

**⭐ If this project helps you, please give us a Star!**
