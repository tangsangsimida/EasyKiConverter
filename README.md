# EasyKiConverter

[English](README_en.md) | 中文

![GitHub release (latest by date)](https://img.shields.io/github/v/release/tangsangsimida/EasyKiConverter_QT)![GitHub all releases](https://img.shields.io/github/downloads/tangsangsimida/EasyKiConverter_QT/total)![GitHub](https://img.shields.io/github/license/tangsangsimida/EasyKiConverter_QT)![GitHub stars](https://img.shields.io/github/stars/tangsangsimida/EasyKiConverter_QT)![GitHub forks](https://img.shields.io/github/forks/tangsangsimida/EasyKiConverter_QT)

一个基于 Qt 6 Quick 和 MVVM 架构的 C++ 桌面应用程序，用于将嘉立创（LCSC）和 EasyEDA 元件导出为Kicad库。

## 简介

EasyKiConverter 提供符号、封装和 3D 模型的完整转换功能，具有现代化的用户界面和高效的转换性能。

## 主要特性

- 符号转换：将 EasyEDA 符号转换为 KiCad 符号库（.kicad_sym）
- 封装生成：从 EasyEDA 封装创建 KiCad 封装（.kicad_mod）
- 3D 模型支持：自动下载并转换 3D 模型（支持 WRL、STEP 等格式）
- 批量处理：支持多个元件同时转换
- 现代化界面：基于 Qt 6 Quick 的流畅用户界面
- 深色模式：支持深色/浅色主题切换
- 并行转换：支持多线程并行处理，充分利用多核 CPU
- 智能提取：支持从剪贴板文本中智能提取元件编号
- BOM 导入：支持导入 BOM 文件批量转换元件

详细功能特性请参考：[功能特性文档](docs/user/FEATURES.md)

## 快速开始

### 安装

#### Windows

1. 从 [GitHub Releases](https://github.com/tangsangsimida/EasyKiConverter_QT/releases) 下载最新版本
2. 解压下载的压缩包
3. 双击 `EasyKiConverter.exe` 运行应用程序

#### macOS

1. 从 [GitHub Releases](https://github.com/tangsangsimida/EasyKiConverter_QT/releases) 下载最新版本
2. 解压下载的压缩包
3. 双击 `EasyKiConverter.app` 运行应用程序

#### Linux

1. 从 [GitHub Releases](https://github.com/tangsangsimida/EasyKiConverter_QT/releases) 下载最新版本
2. 解压下载的压缩包
3. 运行 `./EasyKiConverter` 启动应用程序

### 从源代码构建

详细的构建说明请参考：[构建指南](docs/developer/BUILD.md)

## 文档

### 面向用户

- [用户手册](docs/user/USER_GUIDE.md) - 详细的使用说明
- [快速开始](docs/user/GETTING_STARTED.md) - 快速入门指南
- [常见问题](docs/user/FAQ.md) - 常见问题解答
- [功能特性](docs/user/FEATURES.md) - 详细的功能特性说明

### 面向开发者

- [构建指南](docs/developer/BUILD.md) - 从源代码构建
- [贡献指南](docs/developer/CONTRIBUTING.md) - 如何贡献代码
- [架构文档](docs/developer/ARCHITECTURE.md) - 项目架构设计

### 项目规划

- [更新日志](docs/developer/CHANGELOG.md) - 版本更新记录
- [项目路线图](docs/project/ROADMAP.md) - 未来发展方向
- [架构决策记录](docs/project/adr/) - 技术决策记录

## 贡献

欢迎贡献！请阅读 [贡献指南](docs/developer/CONTRIBUTING.md) 了解如何参与项目开发。

## 许可证

本项目采用 GNU General Public License v3.0 (GPL-3.0) 许可证。

查看 [LICENSE](LICENSE) 文件了解完整许可证条款。

## 致谢

### 参考项目

本项目参考了 [uPesy/easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py) 项目的设计和算法。感谢原作者提供的优秀基础框架和核心转换算法。

注意：本项目是一个独立的 C++ 实现，不包含 Python 代码。Python 版本仅作为设计和算法的参考。

### 贡献者

感谢所有为 EasyKiConverter 项目做出贡献的开发者！

![Contributors](https://contrib.rocks/image?repo=tangsangsimida/EasyKiConverter_QT&max=30)

查看所有贡献者：[贡献者列表](https://github.com/tangsangsimida/EasyKiConverter_QT/graphs/contributors)

## 联系方式

如有问题或建议，请通过 GitHub Issues 联系项目维护者。

## 项目主页

[GitHub Repository](https://github.com/tangsangsimida/EasyKiConverter_QT)