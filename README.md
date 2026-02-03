
<h1 align="center">EasyKiConverter</h1>
--- 
<p align="center">
  <a href="README_en.md">English</a> | 中文
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
--- 

**EasyKiConverter** 是一个基于 Qt 6 和 MVVM 架构的现代化 C++ 桌面工具，专为电子工程师设计，旨在将嘉立创 (LCSC) 和 EasyEDA 的元件数据高效转换为 KiCad 库文件。

## 主要特性

*   **全流程转换**：支持符号 (.kicad_sym)、封装 (.kicad_mod) 及 3D 模型 (STEP/WRL) 的完整导出。
*   **高效批量处理**：支持多线程并行转换与 BOM 文件导入，充分利用多核性能。
*   **现代化体验**：基于 Qt Quick 的流畅 UI，支持深色/浅色主题切换。
*   **智能配置**：配置项实时自动保存，支持断点记忆与调试模式状态恢复。
*   **智能辅助**：支持从剪贴板智能提取元件编号。

## 快速开始

### 安装
请前往 [Releases](https://github.com/tangsangsimida/EasyKiConverter_QT/releases) 页面下载适用于您平台的版本：

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
*   [性能优化报告](docs/PERFORMANCE_OPTIMIZATION_REPORT.md) | [项目路线图](docs/project/ROADMAP.md)


## 贡献与致谢

### 贡献者

感谢以下开发者对 EasyKiConverter 的贡献：

<a href="https://github.com/tangsangsimida/EasyKiConverter_QT/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=tangsangsimida/EasyKiConverter_QT&max=50" />
</a>

欢迎提交 Issue 或 Pull Request 参与改进！详细请阅 [贡献指南](docs/developer/CONTRIBUTING.md)。

## 许可证

本项目采用 **GPL-3.0** 许可证。详情请参阅 [LICENSE](LICENSE) 文件。
