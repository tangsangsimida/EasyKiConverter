# EasyKiConverter 🔄

**[English](README_en.md)** | [中文](README.md)

一个强大的 C++ 桌面应用程序，基于 Qt 6 Quick 和 MVVM 架构，用于将嘉立创（LCSC）和 EasyEDA 元件转换为 KiCad 格式。支持符号、封装和 3D 模型的完整转换，提供现代化的用户界面和高效的转换性能。

## ✨ 功能特性

### 🎯 核心功能
- **符号转换**：将 EasyEDA 符号转换为 KiCad 符号库（.kicad_sym）
- **封装生成**：从 EasyEDA 封装创建 KiCad 封装（.kicad_mod）
- **3D模型支持**：自动下载并转换 3D 模型（支持 WRL、STEP 等格式）
- **批量处理**：支持多个元件同时转换
- **网络重试机制**：网络请求失败时自动重试，提高转换成功率
- **GZIP 解压缩**：自动解压缩 GZIP 编码的响应数据，减少数据传输量

### 🚀 性能优化
- **并行转换**：支持多线程并行处理，充分利用多核 CPU
- **两阶段导出**：数据收集并行，数据导出串行，优化批量转换性能
- **状态机模式**：异步数据收集，提高响应速度
- **内存优化**：智能指针管理，减少内存泄漏

### 🎨 用户界面
- **现代化界面**：基于 Qt 6 Quick 的流畅用户界面
- **深色模式**：支持深色/浅色主题切换
- **卡片式布局**：清晰的界面组织，易于使用
- **流畅动画**：按钮悬停、卡片进入、状态切换等动画效果
- **实时进度**：实时显示转换进度和状态

### 🔧 高级功能
- **图层映射系统**：完整的嘉立创 EDA 到 KiCad 图层映射（50+ 图层）
- **多边形焊盘支持**：支持自定义形状焊盘的正确导出
- **椭圆弧计算**：精确的圆弧计算，支持复杂几何形状
- **文本层处理**：支持类型 "N" 的特殊处理和镜像文本处理
- **覆盖文件功能**：支持覆盖已存在的 KiCad V9 格式文件
- **智能提取**：支持从剪贴板文本中智能提取元件编号
- **BOM 导入**：支持导入 BOM 文件批量转换元件

## 🏗️ 项目架构

本项目采用 **MVVM (Model-View-ViewModel)** 架构模式，提供清晰的职责分离和高效的代码组织。

### 四层架构

```
┌─────────────────────────────────────────┐
│              View Layer                  │
│         (QML Components)                 │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│          ViewModel Layer                │
│  ┌──────────────────────────────────┐   │
│  │ ComponentListViewModel          │   │
│  │ ExportSettingsViewModel         │   │
│  │ ExportProgressViewModel         │   │
│  │ ThemeSettingsViewModel          │   │
│  └──────────────────────────────────┘   │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│           Service Layer                  │
│  ┌──────────────────────────────────┐   │
│  │ ComponentService                 │   │
│  │ ExportService                    │   │
│  │ ConfigService                    │   │
│  └──────────────────────────────────┘   │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│            Model Layer                   │
│  ┌──────────────────────────────────┐   │
│  │ ComponentData                    │   │
│  │ SymbolData                       │   │
│  │ FootprintData                    │   │
│  │ Model3DData                      │   │
│  └──────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

### 设计模式

- **MVVM 模式**：清晰的职责分离，View 负责界面，ViewModel 负责状态，Model 负责数据
- **状态机模式**：异步数据收集，提高响应速度
- **两阶段导出策略**：数据收集并行，数据导出串行，优化批量转换性能

## 📚 详细文档

更多详细信息，请参阅 `docs` 目录下的文档：

### 项目文档
- [项目概述](IFLOW.md) - 项目概述和开发状态
- [架构文档](docs/ARCHITECTURE.md) - MVVM 架构设计文档
- [图层映射](docs/LAYER_MAPPING.md) - 嘉立创 EDA 到 KiCad 图层映射说明

### 重构文档
- [重构计划](docs/REFACTORING_PLAN.md) - MVVM 重构计划
- [重构总结](docs/REFACTORING_SUMMARY.md) - 重构总结和成果
- [MainController 迁移计划](docs/MAINCONTROLLER_MIGRATION_PLAN.md) - MainController 迁移步骤
- [MainController 清理计划](docs/MAINCONTROLLER_CLEANUP_PLAN.md) - MainController 清理步骤
- [QML 迁移指南](docs/QML_MIGRATION_GUIDE.md) - QML 文件迁移指南
- [文档更新指南](docs/DOCUMENTATION_UPDATE_GUIDE.md) - 文档更新指南

### 开发文档
- [调试数据导出](docs/DEBUG_EXPORT_GUIDE.md) - 调试数据导出功能使用指南
- [封装解析修复](docs/FIX_FOOTPRINT_PARSING.md) - 封装解析修复说明

### 测试文档
- [单元测试指南](tests/TESTING_GUIDE.md) - 单元测试指南
- [集成测试指南](tests/INTEGRATION_TEST_GUIDE.md) - 集成测试指南
- [性能测试指南](tests/PERFORMANCE_TEST_GUIDE.md) - 性能测试指南

## 💻 技术栈

- **编程语言**：C++17
- **UI 框架**：Qt 6.10.1 (Qt Quick + Qt Quick Controls 2)
- **构建系统**：CMake 3.16+
- **编译器**：MinGW 13.10 (Windows 平台)
- **架构模式**：MVVM (Model-View-ViewModel)
- **网络库**：Qt Network (带重试机制和 GZIP 支持)
- **多线程**：QThreadPool + QRunnable + QMutex
- **压缩库**：zlib (用于 GZIP 解压缩)

## 🚀 快速开始

### 环境要求

- **操作系统**：Windows 10/11 (推荐)、macOS、Linux
- **Qt 版本**：Qt 6.8 或更高版本 (推荐 Qt 6.10.1)
- **CMake 版本**：CMake 3.16 或更高版本
- **编译器**：
  - Windows: MinGW 13.10 (推荐) 或 MSVC 2019+
  - macOS: Clang (Xcode 12+)
  - Linux: GCC 9+ 或 Clang 10+

### 构建步骤

#### Windows + MinGW

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64"

# 编译项目
cmake --build . --config Debug

# 运行应用程序
./bin/EasyKiConverter.exe
```

#### macOS

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake .. -DCMAKE_PREFIX_PATH="/usr/local/Qt-6.10.1"

# 编译项目
cmake --build . --config Debug

# 运行应用程序
./bin/EasyKiConverter
```

#### Linux

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake .. -DCMAKE_PREFIX_PATH="/opt/Qt/6.10.1/gcc_64"

# 编译项目
cmake --build . --config Debug

# 运行应用程序
./bin/EasyKiConverter
```

### 使用 Qt Creator

1. 使用 Qt Creator 打开 `CMakeLists.txt`
2. 配置构建套件 (Kit)：
   - 选择 Qt 版本 (Qt 6.10.1 或更高)
   - 选择编译器 (MinGW、Clang 或 GCC)
3. 点击"构建"按钮 (或按 Ctrl+B)
4. 点击"运行"按钮 (或按 F5) 启动应用程序

## 📊 项目状态

- **当前版本**：3.0.0
- **开发状态**：重构完成，进入优化阶段
- **完成进度**：约 95% (核心功能已实现，架构重构完成)
- **架构模式**：MVVM (Model-View-ViewModel)
- **最后更新**：2026年1月17日

### 已完成的功能

- ✅ 基础架构搭建 (CMake、Qt Quick 框架)
- ✅ 核心转换引擎 (EasyEDA API、KiCad 导出器)
- ✅ 现代化 UI 界面 (卡片式布局、深色模式、动画效果)
- ✅ MVVM 架构重构
- ✅ Service 层实现
- ✅ ViewModel 层实现
- ✅ 状态机模式实现
- ✅ 两阶段导出策略
- ✅ 并行转换支持
- ✅ 网络请求优化 (重试机制、GZIP 解压缩)
- ✅ 图层映射系统 (50+ 图层)
- ✅ 多边形焊盘支持
- ✅ 椭圆弧计算
- ✅ 文本层处理逻辑
- ✅ 覆盖文件功能
- ✅ 智能提取功能
- ✅ BOM 文件导入
- ✅ 完整的测试框架

### 进行中的功能

- ⏳ 集成测试 (完整转换流程)
- ⏳ 性能测试和优化
- ⏳ 兼容性测试

## 🤝 贡献指南

本项目欢迎贡献！在提交代码之前，请确保：

### 代码质量

- 代码符合 Qt 编码规范
- 添加必要的注释和文档
- 遵循项目的 MVVM 架构设计
- 通过代码风格检查

### 测试要求

- 测试代码功能
- 确保没有引入新的 Bug
- 添加单元测试 (如适用)
- 确保测试覆盖率

### 文档更新

- 更新相关文档
- 添加变更说明
- 更新 API 文档

### 设计参考

- 参考项目的设计模式
- 保持代码风格一致
- 遵循现有的架构设计
- 确保与 Python 版本的转换结果一致

### 提交规范

- 使用清晰的提交信息
- 遵循 Git 提交规范
- 使用语义化提交信息
- 每个提交只包含一个逻辑变更
- [单元测试指南](tests/TESTING_GUIDE.md) - 单元测试指南
- [集成测试指南](tests/INTEGRATION_TEST_GUIDE.md) - 集成测试指南
- [性能测试指南](tests/PERFORMANCE_TEST_GUIDE.md) - 性能测试指南

## 📄 许可证

本项目采用 **GNU General Public License v3.0 (GPL-3.0)** 许可证。

查看 [LICENSE](LICENSE) 文件了解完整许可证条款。

## 🙏 致谢

### 🌟 特别感谢

本项目参考了 **[uPesy/easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py)** 项目的设计和算法。感谢原作者提供的优秀基础框架和核心转换算法，为本项目的开发奠定了坚实的基础。

**注意**：本项目是一个独立的 C++ 实现，不包含 Python 代码。Python 版本仅作为设计和算法的参考。

### 🤝 其他致谢

感谢 [GitHub](https://github.com/) 平台以及所有为本项目提供贡献的贡献者。

我们要向所有贡献者表示诚挚的感谢。

<a href="https://github.com/tangsangsimida/EasyKiConverter_QT/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=tangsangsimida/EasyKiConverter_QT" />
</a>

感谢 [EasyEDA](https://easyeda.com/) 和 [嘉立创](https://www.szlcsc.com/) 提供的开放 API。

感谢 [KiCad](https://www.kicad.org/) 开源电路设计软件。

感谢 [Qt](https://www.qt.io/) 提供的强大的跨平台开发框架。

---

**⭐ 如果这个项目对您有帮助，请给我们一个 Star！**
