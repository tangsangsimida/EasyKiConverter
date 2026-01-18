# EasyKiConverter

[English](README_en.md) | 中文

一个基于 Qt 6 Quick 和 MVVM 架构的 C++ 桌面应用程序，用于将嘉立创（LCSC）和 EasyEDA 元件转换为 KiCad 格式。

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

详细功能特性请参考：[功能特性文档](docs/FEATURES.md)

## 快速开始

```bash
# Clone 仓库
git clone https://github.com/tangsangsimida/EasyKiConverter_QT.git
cd EasyKiConverter_QT

# 创建构建目录
mkdir build
cd build

# 配置项目 (Windows + MinGW)
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64"

# 编译项目
cmake --build . --config Debug

# 运行应用程序
./bin/EasyKiConverter.exe
```

详细构建说明请参考：[快速开始文档](docs/GETTING_STARTED.md)

## 文档

### 项目文档

- [项目概述](IFLOW.md) - 项目概述和开发状态
- [功能特性](docs/FEATURES.md) - 详细的功能特性说明
- [快速开始](docs/GETTING_STARTED.md) - 快速开始指南
- [架构文档](docs/ARCHITECTURE.md) - MVVM 架构设计文档
- [图层映射](docs/LAYER_MAPPING.md) - 嘉立创 EDA 到 KiCad 图层映射说明

### 开发文档

- [贡献指南](docs/CONTRIBUTING.md) - 如何贡献代码
- [调试数据导出](docs/DEBUG_EXPORT_GUIDE.md) - 调试数据导出功能使用指南
- [封装解析修复](docs/FIX_FOOTPRINT_PARSING.md) - 封装解析修复说明

### 重构文档

- [重构计划](docs/REFACTORING_PLAN.md) - MVVM 重构计划
- [重构总结](docs/REFACTORING_SUMMARY.md) - 重构总结和成果
- [MainController 迁移计划](docs/MAINCONTROLLER_MIGRATION_PLAN.md) - MainController 迁移步骤
- [MainController 清理计划](docs/MAINCONTROLLER_CLEANUP_PLAN.md) - MainController 清理步骤
- [QML 迁移指南](docs/QML_MIGRATION_GUIDE.md) - QML 文件迁移指南

### 测试文档

- [单元测试指南](tests/TESTING_GUIDE.md) - 单元测试指南
- [集成测试指南](tests/INTEGRATION_TEST_GUIDE.md) - 集成测试指南
- [性能测试指南](tests/PERFORMANCE_TEST_GUIDE.md) - 性能测试指南

## 贡献

欢迎贡献！请阅读 [贡献指南](docs/CONTRIBUTING.md) 了解如何参与项目开发。

## 许可证

本项目采用 GNU General Public License v3.0 (GPL-3.0) 许可证。

查看 [LICENSE](LICENSE) 文件了解完整许可证条款。

## 致谢

本项目参考了 [uPesy/easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py) 项目的设计和算法。感谢原作者提供的优秀基础框架和核心转换算法。

注意：本项目是一个独立的 C++ 实现，不包含 Python 代码。Python 版本仅作为设计和算法的参考。

## 联系方式

如有问题或建议，请通过 GitHub Issues 联系项目维护者。

## 项目主页

[GitHub Repository](https://github.com/tangsangsimida/EasyKiConverter_QT)