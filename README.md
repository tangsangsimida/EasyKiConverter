# EasyKiConverter

[English](README_en.md) | 中文

[![Build Status](https://github.com/tangsangsimida/EasyKiConverter_QT/workflows/Build/badge.svg)](https://github.com/tangsangsimida/EasyKiConverter_QT/actions/workflows/build.yml)
[![Code Coverage](https://codecov.io/gh/tangsangsimida/EasyKiConverter_QT/branch/main/graph/badge.svg)](https://codecov.io/gh/tangsangsimida/EasyKiConverter_QT)
[![Security Scan](https://github.com/tangsangsimida/EasyKiConverter_QT/workflows/Security/badge.svg)](https://github.com/tangsangsimida/EasyKiConverter_QT/actions/workflows/security.yml)
[![Performance](https://github.com/tangsangsimida/EasyKiConverter_QT/workflows/Performance%20Testing/badge.svg)](https://github.com/tangsangsimida/EasyKiConverter_QT/actions/workflows/performance.yml)
[![Code Format](https://github.com/tangsangsimida/EasyKiConverter_QT/workflows/Code%20Format%20Check/badge.svg)](https://github.com/tangsangsimida/EasyKiConverter_QT/actions/workflows/clang-format.yml)

![GitHub release (latest by date)](https://img.shields.io/github/v/release/tangsangsimida/EasyKiConverter_QT)
![GitHub downloads](https://img.shields.io/github/downloads/tangsangsimida/EasyKiConverter_QT/total)
![License](https://img.shields.io/github/license/tangsangsimida/EasyKiConverter_QT)
![Stars](https://img.shields.io/github/stars/tangsangsimida/EasyKiConverter_QT)

**EasyKiConverter** 是一个基于 Qt 6 和 MVVM 架构的现代化 C++ 桌面工具，专为电子工程师设计，旨在将嘉立创 (LCSC) 和 EasyEDA 的元件数据高效转换为 KiCad 库文件。

## ✨ 主要特性

*   **全流程转换**：支持符号 (.kicad_sym)、封装 (.kicad_mod) 及 3D 模型 (STEP/WRL) 的完整导出。
*   **高效批量处理**：支持多线程并行转换与 BOM 文件导入，充分利用多核性能。
*   **现代化体验**：基于 Qt Quick 的流畅 UI，支持深色/浅色主题切换。
*   **智能辅助**：支持从剪贴板智能提取元件编号。

## 🚀 快速开始

### 安装
请前往 [Releases](https://github.com/tangsangsimida/EasyKiConverter_QT/releases) 页面下载适用于您平台的版本：

*   **Windows**: 推荐下载 `.exe` 安装程序（包含完整运行时），或下载 `.zip` 便携版。
*   **Linux**: 推荐下载 `.AppImage`（无需安装，赋予执行权限即可运行），或 `.tar.gz` 归档。
*   **macOS**: 下载 `.dmg` 镜像文件。

### 从源码构建
本项目已实现全平台 CI/CD 自动化构建。如果您是开发者或希望自行编译，请参考 [构建指南](docs/developer/BUILD.md)。

## 📚 文档中心

**用户指南**
*   [快速入门](docs/user/GETTING_STARTED.md) | [用户手册](docs/user/USER_GUIDE.md) | [常见问题 (FAQ)](docs/user/FAQ.md)
*   [详细功能特性](docs/user/FEATURES.md)

**开发者资源**
*   [贡献指南](docs/developer/CONTRIBUTING.md) | [架构设计](docs/developer/ARCHITECTURE.md) | [构建说明](docs/developer/BUILD.md)
*   [性能优化报告](docs/PERFORMANCE_OPTIMIZATION_REPORT.md) | [项目路线图](docs/project/ROADMAP.md)

## 📅 最新更新 (v3.0.0)

本次更新引入了全新的**三阶段流水线架构**，显著提升了转换效率：
*   **性能飞跃**：总耗时减少 54%，吞吐量提升 117%。
*   **资源优化**：内存占用降低 50%，CPU 利用率大幅提升。
*   **功能增强**：完整的 3D 模型支持与批量处理能力。

> 查看完整变更记录：[CHANGELOG.md](docs/developer/CHANGELOG.md)

## 🤝 贡献与致谢

本项目参考了 [easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py) 的核心算法，感谢原作者的贡献。
欢迎提交 Issue 或 Pull Request 参与改进！详细请阅 [贡献指南](docs/developer/CONTRIBUTING.md)。

## 📄 许可证

本项目采用 **GPL-3.0** 许可证。详情请参阅 [LICENSE](LICENSE) 文件。
