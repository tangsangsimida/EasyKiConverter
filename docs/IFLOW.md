# EasyKiConverter C++ 版本项目概述

## 项目简介

EasyKiConverter C++ 版本是一个基于 Qt 6 Quick 的桌面应用程序，旨在将嘉立创（LCSC）和 EasyEDA 元件转换为 KiCad 格式。本项目是对现有 Python 版本（位于 `EasyKiConverter_QT/` 目录）的**升级优化版本**，使用 C/C++ 重写并编译为独立的桌面可执行文件，提供更好的性能和用户体验。

**当前版本**：3.0.0
**开发状态**：第四阶段（功能完善）进行中
**完成进度**：约 65%（3/6 阶段已完成）
**最后更新**：2026年1月9日

## 核心功能

- **符号转换**：将 EasyEDA 符号转换为 KiCad 符号库（.kicad_sym）
- **封装生成**：从 EasyEDA 封装创建 KiCad 封装（.kicad_mod）
- **3D 模型支持**：自动下载并转换 3D 模型（支持 WRL、STEP 等格式）
- **批量处理**：支持多个元件同时转换
- **现代化界面**：使用 Qt Quick 提供流畅的用户体验
- **深色模式**：支持深色/浅色主题切换
- **BOM 导入**：支持导入 BOM 文件批量转换元件
- **网络优化**：带重试机制的网络请求，支持 GZIP 解压缩
- **进度追踪**：实时显示转换进度和状态

## 项目目标

本项目的主要目标是将 Python 版本的 EasyKiConverter 升级优化为 C/C++ 版本，具体包括：

1. **性能优化**：
   - 提升转换速度，减少处理时间
   - 优化内存使用，支持批量处理更多元件
   - 改善启动速度和响应速度

2. **部署优化**：
   - 编译为独立的可执行文件，无需 Python 环境
   - 减少依赖项，简化安装过程
   - 支持单文件发布，方便用户使用

3. **功能对等**：
   - 完全实现 Python 版本的所有功能
   - 保持相同的用户体验和界面设计
   - 支持相同的文件格式和转换选项

4. **架构优化**：
   - 采用更高效的 C++ 架构设计
   - 使用 Qt Quick 提供更流畅的 UI 体验
   - 改进错误处理和异常管理

## 技术栈

- **编程语言**：C++17
- **UI 框架**：Qt 6.10.1（Qt Quick + Qt Quick Controls 2）
- **构建系统**：CMake 3.16+
- **编译器**：MinGW 13.10（Windows 平台）
- **架构模式**：MVC（Model-View-Controller）
- **网络库**：Qt Network（带重试机制和 GZIP 支持）
- **样式系统**：Material Design 风格的 QML 样式系统

## 项目结构

```
EasyKiconverter_Cpp_Version/
├── CMakeLists.txt              # CMake 构建配置
├── main.cpp                    # 主程序入口
├── Main.qml                    # QML 主入口
├── IFLOW.md                    # 项目概述文档（本文件）
├── MIGRATION_PLAN.md           # 详细的移植计划
├── MIGRATION_CHECKLIST.md      # 可执行的任务清单
├── MIGRATION_QUICKREF.md       # 快速参考卡片
├── .gitignore                  # Git 忽略规则
├── resources/                  # 资源文件
│   ├── icons/                  # 应用图标
│   │   ├── app_icon.icns       # macOS 图标
│   │   ├── app_icon.ico        # Windows 图标
│   │   ├── app_icon.png        # PNG 图标
│   │   ├── app_icon.svg        # SVG 图标
│   │   ├── add.svg             # 添加图标
│   │   ├── play.svg            # 播放图标
│   │   ├── folder.svg          # 文件夹图标
│   │   ├── trash.svg           # 删除图标
│   │   └── upload.svg          # 上传图标
│   ├── imgs/                   # 图片资源
│   │   └── background.jpg      # 背景图片
│   └── styles/                 # 样式文件
├── src/
│   ├── core/                   # 核心转换引擎
│   │   ├── easyeda/            # EasyEDA API 和导入器
│   │   │   ├── EasyedaApi.h/cpp         # EasyEDA API 客户端
│   │   │   ├── EasyedaImporter.h/cpp    # 数据导入器
│   │   │   └── JLCDatasheet.h/cpp       # JLC 数据表解析
│   │   ├── kicad/              # KiCad 导出器
│   │   │   ├── ExporterSymbol.h/cpp     # 符号导出器
│   │   │   ├── ExporterFootprint.h/cpp  # 封装导出器
│   │   │   └── Exporter3DModel.h/cpp    # 3D 模型导出器
│   │   └── utils/              # 工具类
│   │       ├── GeometryUtils.h/cpp      # 几何计算工具
│   │       └── NetworkUtils.h/cpp       # 网络请求工具（带重试和 GZIP）
│   ├── models/                 # 数据模型
│   │   ├── ComponentData.h/cpp          # 元件数据模型
│   │   ├── SymbolData.h/cpp             # 符号数据模型
│   │   ├── FootprintData.h/cpp          # 封装数据模型
│   │   └── Model3DData.h/cpp            # 3D 模型数据模型
│   ├── ui/                     # UI 层
│   │   ├── controllers/        # 控制器
│   │   │   └── MainController.h/cpp     # 主控制器
│   │   ├── qml/                # QML 界面
│   │   │   ├── MainWindow.qml           # 主窗口
│   │   │   ├── components/              # 可复用组件
│   │   │   │   ├── Card.qml            # 卡片容器组件
│   │   │   │   ├── ModernButton.qml    # 现代化按钮组件
│   │   │   │   ├── Icon.qml            # 图标组件
│   │   │   │   ├── ComponentListItem.qml    # 元件列表项组件
│   │   │   │   └── ResultListItem.qml       # 结果列表项组件
│   │   │   └── styles/         # 样式系统
│   │   │       ├── AppStyle.qml         # 全局样式（支持深色模式）
│   │   │       └── qmldir               # QML 模块定义
│   │   └── utils/              # UI 工具
│   │       └── ConfigManager.h/cpp      # 配置管理器
│   └── workers/                # 工作线程（待实现）
└── EasyKiConverter_QT/         # Python 版本参考实现
```

## 📚 项目文档

本项目提供完整的文档体系，帮助开发者快速了解和参与项目：

| 文档 | 说明 | 目标读者 |
|------|------|---------|
| **IFLOW.md** | 项目概述文档 | 所有人 |
| **MIGRATION_PLAN.md** | 详细的移植计划（16 周，6 个阶段） | 开发者 |
| **MIGRATION_CHECKLIST.md** | 可执行的任务清单 | 开发者 |
| **MIGRATION_QUICKREF.md** | 快速参考卡片 | 所有人 |

**当前开发阶段**：第四阶段（功能完善）进行中

### 文档使用指南

- **首次了解项目**：阅读 `IFLOW.md` 了解项目概况
- **开始移植工作**：阅读 `MIGRATION_PLAN.md` 了解详细计划
- **执行开发任务**：按照 `MIGRATION_CHECKLIST.md` 逐个完成任务
- **快速查找信息**：参考 `MIGRATION_QUICKREF.md` 快速查找关键信息

## 构建和运行

### 环境要求

- Qt 6.8 或更高版本（推荐 Qt 6.10.1）
- CMake 3.16 或更高版本
- MinGW 编译器（Windows 平台，推荐 MinGW 13.10）
- Qt Quick 模块
- Qt Network 模块
- Qt Widgets 模块
- Qt Quick Controls 2 模块
- zlib 库（用于 GZIP 解压缩）

### 构建步骤

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目（使用 Qt Creator 或命令行）
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=<Qt安装路径>

# 编译项目
cmake --build . --config Debug

# 运行应用程序
./bin/EasyKiConverter.exe
```

### 使用 Qt Creator

1. 使用 Qt Creator 打开 `CMakeLists.txt`
2. 配置构建套件（Kit）
3. 点击"构建"按钮
4. 点击"运行"按钮启动应用程序

## 开发指南

### 当前开发状态

项目已完成**前三个阶段**的开发，目前正在推进**第四阶段**：

**当前进度**：约 65% 完成（3/6 阶段已完成）

#### ✅ 第一阶段：基础架构（已完成）
- ✅ CMake 构建系统配置完成
- ✅ 基础 Qt Quick 应用程序框架
- ✅ 项目目录结构搭建
- ✅ 基础模型类（ComponentData, SymbolData, FootprintData, Model3DData）
- ✅ 主程序入口（main.cpp）
- ✅ QML 模块配置
- ✅ 资源文件管理（icons, styles）
- ✅ zlib 库集成

#### ✅ 第二阶段：核心转换引擎（已完成）
- ✅ EasyEDA API 客户端（EasyedaApi）
- ✅ EasyEDA 导入器（EasyedaImporter）
- ✅ JLC 数据表解析（JLCDatasheet）
- ✅ KiCad 符号导出器（ExporterSymbol）
- ✅ KiCad 封装导出器（ExporterFootprint）
- ✅ KiCad 3D 模型导出器（Exporter3DModel）
- ✅ 主控制器（MainController）
- ✅ 工具类（GeometryUtils, NetworkUtils）
- ✅ 配置管理器（ConfigManager）
- ✅ 网络请求优化（重试机制、GZIP 解压缩）

#### ✅ 第三阶段：UI 升级优化（已完成）
- ✅ 现代化 QML 界面设计（MainWindow.qml）
- ✅ 卡片式布局系统（Card.qml）
- ✅ 流畅动画效果（按钮悬停、卡片进入、状态切换）
- ✅ 响应式布局优化
- ✅ SVG 图标支持（Icon.qml）
- ✅ 深色模式切换功能（AppStyle.qml + MainController）
- ✅ 全局样式系统（AppStyle.qml）
- ✅ 可复用组件：
  - Card.qml - 卡片容器组件
  - ModernButton.qml - 现代化按钮组件
  - Icon.qml - 图标组件
  - ComponentListItem.qml - 元件列表项组件
  - ResultListItem.qml - 结果列表项组件
- ✅ 性能优化（延迟加载、属性绑定优化）
- ✅ 主界面完整布局（元件输入、BOM 导入、导出选项、进度显示、结果展示）
- ✅ 背景图片支持

#### ⏳ 第四阶段：功能完善（进行中）
- ✅ UI 界面布局和组件
- ✅ 深色模式切换功能
- ✅ 导出选项配置界面
- ✅ 进度显示界面
- ✅ 结果展示界面
- ✅ 网络请求优化（GZIP 解压缩、重试机制）
- ⏳ 元件输入和验证逻辑（部分完成）
- ⏳ BOM 文件导入功能（部分完成）
- ⏳ 错误处理和用户提示（部分完成）
- ⏳ 工作线程和并发处理（待实现）

#### ⏳ 第五阶段：测试和优化（待开发）
- ⏳ 单元测试（核心转换逻辑）
- ⏳ 集成测试（完整转换流程）
- ⏳ 性能测试和优化（对比 Python 版本）
- ⏳ 内存使用优化
- ⏳ 兼容性测试（KiCad 5.x/6.x）
- ⏳ 网络请求测试和优化

#### ⏳ 第六阶段：打包和发布（待开发）
- ⏳ 可执行文件打包（windeployqt）
- ⏳ 安装程序制作（NSIS 或 WiX）
- ⏳ 发布和分发（GitHub Releases）
- ⏳ 文档完善（用户手册、开发文档）
- ⏳ 许可证和版权信息

### 核心技术特性

#### 网络请求优化

项目实现了带重试机制的网络请求工具类 `NetworkUtils`，具有以下特性：

- **自动重试**：支持配置最大重试次数（默认 3 次）
- **智能延迟**：使用指数退避算法计算重试延迟
- **GZIP 解压缩**：自动解压缩 GZIP 编码的响应数据
- **超时控制**：可配置请求超时时间（默认 30 秒）
- **进度追踪**：实时报告下载进度
- **二进制数据支持**：支持下载和处理二进制数据（如 3D 模型文件）

#### 深色模式支持

项目实现了完整的深色模式支持：

- **全局样式系统**：`AppStyle.qml` 提供统一的颜色主题
- **动态切换**：通过 `MainController` 统一管理深色模式状态
- **组件适配**：所有 UI 组件自动适配主题变化
- **平滑过渡**：使用动画效果实现主题切换

#### 现代化 UI 设计

- **Material Design 风格**：采用 Material Design 设计原则
- **卡片式布局**：使用卡片容器组织内容
- **流畅动画**：按钮悬停、卡片进入、状态切换等动画效果
- **响应式设计**：适配不同屏幕尺寸
- **图标系统**：使用 SVG 图标，支持主题颜色

### 最近解决的问题

#### 问题 1：QML 文件加载失败
- **错误**：`qrc:/qt/qml/EasyKiconverter_Cpp_Version/src/ui/qml/MainWindow.qml: No such file or directory`
- **解决**：添加所有 QML 文件到 CMakeLists.txt QML_FILES 列表，并更新 Main.qml 使用正确的资源路径

#### 问题 2：QMessageBox 未找到
- **错误**：`QMessageBox: No such file or directory`
- **解决**：在 CMakeLists.txt 中添加 Qt6::Widgets 到 find_package 和 target_link_libraries

#### 问题 3：QML 属性重复赋值
- **错误**：`Property value set multiple times` 在 ComponentListItem.qml 和 ResultListItem.qml
- **解决**：删除重复的 `color` 属性赋值，只保留单个条件绑定

#### 问题 4：Icon.qml ColorOverlay 语法错误
- **错误**：`Expected token 'identifier'` 在 Icon.qml:8:19
- **解决**：简化 Icon 组件，移除 ColorOverlay 效果，直接使用 SVG 原始颜色

#### 问题 5：MainController isDarkMode 方法缺失
- **错误**：`no declaration matches 'bool EasyKiConverter::MainController::isDarkMode() const'`
- **解决**：在 MainController.h 中添加 `isDarkMode()` getter 方法

#### 问题 6：CMakeLists.txt 重复的 SOURCES 声明
- **问题**：CMakeLists.txt 中多次重复声明 SOURCES，导致构建混乱
- **解决**：清理 CMakeLists.txt，移除重复的 SOURCES 声明，保持清晰的构建配置

#### 问题 7：ModernButton 组件图标显示问题
- **问题**：ModernButton 组件中的图标颜色不随主题变化
- **解决**：在 ModernButton.qml 中添加 textColor 属性绑定，确保图标颜色与主题一致

#### 问题 8：AppStyle 深色模式状态管理
- **问题**：AppStyle.qml 中的深色模式状态与 MainController 不同步
- **解决**：通过 MainController 的 setDarkMode 方法统一管理深色模式状态，确保 UI 统一

#### 问题 9：主窗口滚动条显示问题
- **问题**：MainWindow.qml 中的滚动条显示不正确
- **解决**：优化 ScrollView 配置，设置正确的滚动条策略

#### 问题 10：背景图片加载和渐变背景替换
- **问题**：原有的渐变背景效果不够美观
- **解决**：添加背景图片支持，使用 `resources/imgs/background.jpg` 作为背景，并添加半透明遮罩层确保内容可读性

#### 问题 11：网络请求 GZIP 解压缩支持
- **问题**：EasyEDA API 返回的数据使用 GZIP 编码，需要解压缩
- **解决**：在 NetworkUtils 中添加 `decompressGzip()` 方法，使用 zlib 库自动解压缩 GZIP 编码的响应数据

### 下一步计划

1. **第四阶段：功能完善（进行中）**
   - ✅ 实现 UI 界面布局和组件
   - ✅ 实现深色模式切换功能
   - ✅ 实现导出选项配置界面
   - ✅ 实现进度显示和结果展示
   - ✅ 实现网络请求优化（GZIP 解压缩、重试机制）
   - ⏳ 完善元件输入和验证逻辑
   - ⏳ 完善 BOM 文件导入功能
   - ⏳ 完善错误处理和用户提示
   - ⏳ 实现工作线程和并发处理

2. **集成测试**
   - 测试完整的转换流程
   - 验证所有功能模块的集成
   - 测试 UI 交互和用户体验
   - 测试深色模式切换
   - 测试网络请求和重试机制

3. **性能优化**
   - 分析性能瓶颈
   - 优化网络请求
   - 优化批量处理性能
   - 优化内存使用
   - 优化 QML 渲染性能

4. **打包和发布**
   - 配置可执行文件打包
   - 创建安装程序
   - 测试部署流程
   - 发布和分发

### 代码规范

- **C++ 编码规范**：遵循 Qt 编码规范
- **QML 编码规范**：使用 Qt Quick 最佳实践
- **命名规范**：
  - 类名使用大驼峰命名法（PascalCase）
  - 变量名使用小驼峰命名法（camelCase）
  - 常量使用全大写下划线分隔（UPPER_SNAKE_CASE）
- **文件组织**：
  - 头文件（.h）和实现文件（.cpp）分离
  - QML 文件按功能模块组织
- **注释规范**：
  - 类和公共方法添加 Doxygen 风格注释
  - 复杂逻辑添加行内注释
- **C++ 特性**：使用 C++17 特性进行开发
- **设计模式**：采用面向对象的设计模式（MVC、单例、工厂等）

## 相关资源

### Python 参考版本

Python 版本的完整实现位于 `EasyKiConverter_QT/` 目录，包含：

- **核心转换引擎**：`EasyKiConverter_QT/src/core/`
  - EasyEDA API 客户端和数据处理
  - KiCad 格式导出引擎
  - 几何工具函数
- **UI 实现**：`EasyKiConverter_QT/src/ui/pyqt6/`
  - PyQt6 桌面界面
  - 现代化 UI 组件
  - 样式管理器
- **文档**：`EasyKiConverter_QT/docs/`
  - 项目结构说明
  - 开发指南
  - 性能优化说明

### 外部依赖

- [EasyEDA API](https://easyeda.com/) - 元件数据来源
- [KiCad](https://www.kicad.org/) - 目标格式
- [Qt 6](https://www.qt.io/) - UI 框架（当前使用 Qt 6.10.1）
- [CMake](https://cmake.org/) - 构建系统
- [MinGW](https://www.mingw-w64.org/) - Windows 编译器
- [zlib](https://zlib.net/) - 数据压缩库（用于 GZIP 解压缩）

## 注意事项

1. **项目阶段**：当前项目已完成前三个阶段（基础架构、核心转换引擎、UI 升级优化），正在进行第四阶段（功能完善）的开发
2. **参考实现**：Python 版本（`EasyKiConverter_QT/`）提供了完整的实现参考，建议在迁移过程中：
   - 仔细研究 Python 版本的架构设计
   - 理解核心转换算法和数据处理逻辑
   - 保持功能对等性，确保所有功能都能实现
3. **Qt 版本**：确保使用 Qt 6.8 或更高版本（推荐 Qt 6.10.1）
4. **构建环境**：Windows 平台使用 MinGW 编译器（推荐 MinGW 13.10）
5. **迁移策略**：
   - ✅ 已完成核心转换功能
   - ✅ 已完成基础 UI 组件
   - ✅ 已完成网络请求优化
   - ⏳ 正在完善功能逻辑
   - ⏳ 需要添加工作线程支持
   - ⏳ 需要添加适当的错误处理和日志记录
6. **性能优化**：在实现过程中注意性能瓶颈，充分利用 C++ 的性能优势
7. **深色模式**：已实现深色模式切换功能，需确保所有组件都支持主题切换
8. **CMake 配置**：注意 CMakeLists.txt 中的重复声明问题，保持构建配置清晰
9. **网络请求**：项目已实现带重试机制和 GZIP 解压缩的网络请求工具类，确保网络请求的稳定性和效率

## C++ 版本的优势

相比 Python 版本，C++ 版本具有以下优势：

1. **性能提升**：
   - 编译型语言，执行速度更快
   - 更高效的内存管理
   - 更好的多线程支持

2. **部署便利**：
   - 独立可执行文件，无需安装 Python 环境
   - 减少依赖项，简化部署流程
   - 更小的安装包体积

3. **稳定性**：
   - 编译时类型检查，减少运行时错误
   - 更好的异常处理机制
   - 更强的类型安全

4. **用户体验**：
   - 更快的启动速度
   - 更流畅的界面响应
   - 更低的系统资源占用

5. **网络优化**：
   - 内置重试机制，提高网络请求成功率
   - 自动 GZIP 解压缩，减少数据传输量
   - 更好的错误处理和超时控制

## 贡献指南

本项目欢迎贡献！在提交代码之前，请确保：

1. **代码质量**：
   - 代码符合 Qt 编码规范
   - 添加必要的注释和文档
   - 遵循项目的架构设计

2. **测试要求**：
   - 测试代码功能
   - 确保没有引入新的 Bug
   - 添加单元测试（如适用）

3. **文档更新**：
   - 更新相关文档
   - 添加变更说明

4. **参考实现**：
   - 参考 Python 版本的实现逻辑
   - 保持功能对等性

5. **提交规范**：
   - 使用清晰的提交信息
   - 遵循 Git 提交规范

## 许可证

本项目采用 **GNU General Public License v3.0 (GPL-3.0)** 许可证。

## 版本信息

- **当前版本**：3.0.0
- **最后更新**：2026年1月9日
- **开发状态**：第四阶段（功能完善）进行中
- **完成进度**：约 65%（3/6 阶段已完成）
- **当前分支**：dev

## 项目状态总结

### 已完成的功能
- ✅ 基础架构搭建（CMake、Qt Quick 框架）
- ✅ 核心转换引擎（EasyEDA API、KiCad 导出器）
- ✅ 现代化 UI 界面（卡片式布局、深色模式、动画效果）
- ✅ 数据模型和工具类
- ✅ 主控制器和配置管理
- ✅ 网络请求优化（重试机制、GZIP 解压缩）
- ✅ 深色模式支持
- ✅ 背景图片支持

### 进行中的功能
- ⏳ 元件输入和验证逻辑（部分完成）
- ⏳ BOM 文件导入功能（部分完成）
- ⏳ 错误处理和用户提示（部分完成）
- ⏳ 工作线程和并发处理（待实现）

### 待开发的功能
- ⏳ 单元测试和集成测试
- ⏳ 性能优化
- ⏳ 打包和发布
- ⏳ 用户文档完善

## 联系方式

如有问题或建议，请通过 GitHub Issues 联系项目维护者。

---

**项目维护者**：EasyKiConverter 开发团队
**项目主页**：[GitHub Repository](https://github.com/your-username/EasyKiconverter_Cpp_Version)
**许可证**：GPL-3.0