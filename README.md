# EasyKiConverter 🔄

**[English](README_en.md)** | [中文](README.md)

一个强大的 Python 工具，用于将嘉立创（LCSC）和 EasyEDA 元件转换为 KiCad 格式，支持符号、封装和 3D 模型的完整转换。提供现代化的 PyQt6 桌面界面，让元件转换变得简单高效。

## ✨ 功能特性

### 🎯 核心功能
- **符号转换**：将 EasyEDA 符号转换为 KiCad 符号库（.kicad_sym）
- **封装生成**：从 EasyEDA 封装创建 KiCad 封装（.kicad_mod）
- **3D模型支持**：自动下载并转换 3D 模型（支持多种格式）
- **批量处理**：支持多个元件同时转换
- **网络重试机制**：网络请求失败时自动重试，提高转换成功率

## 📚 详细文档

更多详细信息，请参阅 `docs` 目录下的文档：

- [项目结构](docs/project_structure.md) - 详细的项目结构和模块说明
- [开发指南](docs/development_guide.md) - 开发环境设置和开发流程
- [贡献指南](docs/contributing.md) - 如何参与项目贡献
- [性能优化](docs/performance.md) - 多线程并行处理和性能提升
- [系统要求](docs/system_requirements.md) - 系统要求和支持的元件类型

## 📄 许可证

本项目采用 **GNU General Public License v3.0 (GPL-3.0)** 许可证。

查看 [LICENSE](LICENSE) 文件了解完整许可证条款。

---

## 🙏 致谢

### 🌟 特别感谢

本项目基于 **[uPesy/easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py)** 项目衍生而来。感谢原作者提供的优秀基础框架和核心转换算法，为本项目的开发奠定了坚实的基础。

### 🤝 其他致谢

感谢 [GitHub](https://github.com/) 平台以及所有为本项目提供贡献的贡献者。

我们要向所有贡献者表示诚挚的感谢。

<a href="https://github.com/tangsangsimida/EasyKiConverter_QT/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=tangsangsimida/EasyKiConverter_QT" />
</a>

感谢 [EasyEDA](https://easyeda.com/) 和 [嘉立创](https://www.szlcsc.com/) 提供的开放 API。

感谢 [KiCad](https://www.kicad.org/) 开源电路设计软件。

---

**⭐ 如果这个项目对您有帮助，请给我们一个 Star！**
