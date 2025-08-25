# EasyKiConverter 🔄

[English](README_en.md) | 中文

EasyKiConverter 是一个功能强大的工具，用于将电子元件从 EasyEDA/LCSC 格式转换为 KiCad 格式。它支持转换符号、封装和 3D 模型，使您能够直接在 KiCad 项目中使用 LCSC 的元件。

## 🌟 功能特点

- 将 EasyEDA/LCSC 元件转换为 KiCad 格式
- 支持符号、封装和 3D 模型转换
- 批处理功能
- 用户友好的图形界面
- 命令行界面，便于自动化
- 跨平台支持（Windows、Linux、macOS）

## 🚀 快速开始

```bash
# 克隆仓库
git clone <repository-url>
cd EasyKiConverter

# 构建项目
mkdir build
cd build
cmake ..
make

# 运行命令行工具
cd bin
./easykiconverter --help

# 运行图形界面工具
./easykiconverter-gui
```

## 📦 打包与分发

### Windows
- 使用 NSIS 创建安装程序。
- 确保安装了 Visual Studio 和 Qt。
- 清理和构建项目：
  ```bash
  mkdir build
  cd build
  cmake ..
  cmake --build . --config Release
  ```

### Linux
- 使用 `.deb` 包管理器格式。
- 确保安装了 GCC 和 Qt。
- 清理和构建项目：
  ```bash
  mkdir build
  cd build
  cmake ..
  make
  ```

### macOS
- 使用 `.dmg` 格式。
- 确保安装了 Xcode 和 Qt。
- 清理和构建项目：
  ```bash
  mkdir build
  cd build
  cmake ..
  make
  ```

## 📖 文档

有关详细文档，请参阅 [docs](docs) 目录中的文件：

- [项目结构](docs/project_structure.md) - 详细的项目结构和模块说明
- [开发指南](docs/development_guide.md) - 开发环境设置和工作流程
- [贡献指南](docs/contributing.md) - 如何为项目做贡献
- [性能指南](docs/performance.md) - 性能优化技术
- [系统要求](docs/system_requirements.md) - 系统要求和支持的元件类型

## 🤝 贡献

欢迎贡献！请阅读我们的[贡献指南](docs/contributing.md)了解如何为该项目做贡献。

## 📄 许可证

本项目采用 GPL-3.0 许可证 - 有关详细信息，请参阅 [LICENSE](LICENSE) 文件。