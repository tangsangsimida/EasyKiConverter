
---

# EasyKiConverter 🔄

> ⚠️ **重要提示：本项目（WebUI 版）已停止维护！**  
> 由于 Web 技术在桌面电子设计工具场景中的局限性，我们已对项目进行全面重构。  
> **新版本采用 PyQt6 原生桌面 UI，功能更强、体验更佳！**  
> 👉 请访问新项目：[https://github.com/tangsangsimida/EasyKiConverter_QT](https://github.com/tangsangsimida/EasyKiConverter_QT)

---

**[English](README_en.md)** | [中文](README.md)

一个强大的 Python 工具，用于将嘉立创（LCSC）和 EasyEDA 元件转换为 KiCad 格式，支持符号、封装和 3D 模型的完整转换。提供现代化的 Web UI 界面，让元件转换变得简单高效。

> 💡 **注意**：本文档仅适用于旧版 WebUI 版本。新项目使用 Qt 桌面应用，不再基于 Web 技术。

## ✨ 功能特性

### 🎯 核心功能
- **符号转换**：将 EasyEDA 符号转换为 KiCad 符号库（.kicad_sym）
- **封装生成**：从 EasyEDA 封装创建 KiCad 封装（.kicad_mod）
- **3D模型支持**：自动下载并转换 3D 模型（支持多种格式）
- **批量处理**：支持多个元件同时转换
- **多线程优化**：并行处理多个元件，显著提升转换效率
- **版本兼容**：支持 KiCad 5.x 和 6.x+ 版本

### 🌐 Web UI 界面
- **现代化界面**：美观的毛玻璃效果设计
- **实时进度**：转换过程可视化进度条，支持并行处理状态显示
- **灵活输入**：支持 LCSC 编号或嘉立创链接
- **选择性导出**：可选择导出符号、封装或 3D 模型
- **即时预览**：转换结果实时显示，包含处理时间和文件统计
- **智能配置**：自动保存导出配置，支持剪贴板快速输入

### 🛠️ 易用性设计
- **一键启动**：双击启动脚本即可运行 Web UI
- **无需配置**：开箱即用，无需复杂设置
- **跨平台支持**：支持 Windows、macOS 和 Linux 系统

## 🚀 快速开始

### 💻 安装与启动

```bash
# 克隆仓库
git clone https://github.com/tangsangsimida/EasyKiConverter.git
cd EasyKiConverter
```

> 💡 **提示**：启动脚本会自动检查并安装所需依赖，无需手动安装

### 🚀 启动 Web UI

```bash
# 使用启动脚本（推荐）
# Windows 用户
start_webui.bat

# Linux/macOS 用户
./start_webui.sh
```

启动后在浏览器中访问：**http://localhost:8000**

> ⚠️ **再次提醒**：此 WebUI 版本已不再更新。如需长期使用或获取新功能，请迁移到 [EasyKiConverter_QT](https://github.com/tangsangsimida/EasyKiConverter_QT)。

## 📚 详细文档

更多详细信息，请参阅 `docs` 目录下的文档：

- [项目结构](docs/project_structure.md) - 详细的项目结构和模块说明
- [开发指南](docs/development_guide.md) - 开发环境设置和开发流程
- [贡献指南](docs/contributing.md) - 如何参与项目贡献
- [性能优化](docs/performance.md) - 多线程并行处理和性能提升
- [系统要求](docs/system_requirements.md) - 系统要求和支持的元件类型

## 📄 许可证

本项目采用 **GNU General Public License v3.0 (GPL-3.0)** 许可证。

- ✅ 商业使用
- ✅ 修改
- ✅ 分发
- ✅ 专利使用
- ❌ 责任
- ❌ 保证

查看 [LICENSE](LICENSE) 文件了解完整许可证条款。

---

## 🙏 致谢

### 🌟 特别感谢

本项目基于 **[uPesy/easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py)** 项目衍生而来。感谢原作者提供的优秀基础框架和核心转换算法，为本项目的开发奠定了坚实的基础。

### 🤝 其他致谢

感谢 [GitHub](https://github.com/) 平台以及所有为本项目提供贡献的贡献者。

我们要向所有贡献者表示诚挚的感谢。

<a href="https://github.com/tangsangsimida/EasyKiConverter/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=tangsangsimida/EasyKiConverter" />
</a>

感谢 [EasyEDA](https://easyeda.com/) 和 [嘉立创](https://www.szlcsc.com/) 提供的开放 API。

感谢 [KiCad](https://www.kicad.org/) 开源电路设计软件。

---

**⭐ 如果这个项目对您有帮助，请给我们一个 Star！**  
（但更推荐您关注并 Star 新项目 👉 [EasyKiConverter_QT](https://github.com/tangsangsimida/EasyKiConverter_QT)）

---
