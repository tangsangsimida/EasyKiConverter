
<h1 align="center">EasyKiConverter</h1>
<p align="center">
  <a href="README_en.md">English</a> | 中文
</p>


<p align="center">
    <a href="https://flathub.org/apps/details/io.github.tangsangsimida.easykiconverter"><img width="200" alt="Download on Flathub" src="https://flathub.org/assets/badges/flathub-badge-i-en.svg"/></a>
    <br/>
    <img src="https://github.com/tangsangsimida/EasyKiConverter/workflows/Build/badge.svg" alt="Build Status" />
    <img src="https://github.com/tangsangsimida/EasyKiConverter/workflows/Security/badge.svg" alt="Security Scan" />
    <img src="https://img.shields.io/github/v/release/tangsangsimida/EasyKiConverter" alt="GitHub release" />
    <img src="https://img.shields.io/github/downloads/tangsangsimida/EasyKiConverter/total" alt="GitHub downloads (total)" />
    <img src="https://img.shields.io/github/license/tangsangsimida/EasyKiConverter" alt="License" />
    <img src="https://img.shields.io/github/stars/tangsangsimida/EasyKiConverter" alt="Stars" />
    <img src="https://img.shields.io/github/issues/tangsangsimida/EasyKiConverter" alt="Issues" />
</p>



**EasyKiConverter** 是一个基于 Qt 6 和 MVVM 架构的现代化 C++ 桌面工具，专为电子工程师设计，旨在将嘉立创 (LCSC) 和 EasyEDA 的元件数据高效转换为 KiCad 库文件。支持 GUI 和 CLI 两种运行模式。

## 主要特性

*   **全流程转换**：支持符号 (.kicad_sym)、封装 (.kicad_mod) 及 3D 模型 (STEP/WRL) 的完整导出。
*   **分体式符号**：支持分体式符号转换
*   **高效批量处理**：支持多线程并行转换与 BOM 文件导入，充分利用多核性能。
*   **现代化体验**：基于 Qt Quick 的流畅 UI，支持深色/浅色主题切换。
*   **智能配置**：配置项实时自动保存，支持断点记忆与调试模式状态恢复。
*   **智能辅助**：支持从剪贴板智能提取元件编号。
*   **LCSC 预览图**：自动获取 LCSC 元件预览图，支持缩略图显示和悬停预览。
*   **CLI 模式**：支持纯命令行模式，适用于批量处理和自动化脚本。

## 快速开始

### 安装
请前往 [Releases](https://github.com/tangsangsimida/EasyKiConverter/releases) 页面下载适用于您平台的版本：

*   **Windows**: 推荐下载 `.exe` 安装程序（包含完整运行时），或下载 `.zip` 便携版。
*   **Linux**: 推荐下载 `.AppImage`（无需安装，赋予执行权限即可运行），或 `.tar.gz` 归档。
*   **macOS**: 下载 `.dmg` 镜像文件。

### 从源码构建
本项目已实现全平台 CI/CD 自动化构建。如果您是开发者或希望自行编译，请参考 [构建指南](docs/developer/BUILD.md)。

## 文档中心

**用户指南**
*   [快速入门](docs/user/GETTING_STARTED.md) | [用户手册](docs/user/USER_GUIDE.md) | [常见问题 (FAQ)](docs/user/FAQ.md)
*   [详细功能特性](docs/user/FEATURES.md)

**开发者资源**

*   [贡献指南](docs/developer/CONTRIBUTING.md) | [架构设计](docs/developer/ARCHITECTURE.md) | [构建说明](docs/developer/BUILD.md)


## 贡献与致谢

### 贡献者

感谢以下开发者对 EasyKiConverter 的贡献：

<a href="https://github.com/tangsangsimida/EasyKiConverter/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=tangsangsimida/EasyKiConverter&max=50" />
</a>

欢迎提交 Issue 或 Pull Request 参与改进！详细请阅 [贡献指南](docs/developer/CONTRIBUTING.md)。

## 许可证

本项目采用 **GPL-3.0** 许可证。详情请参阅 [LICENSE](LICENSE) 文件。
