# EasyKiConverter C++ 版本 🔄

这是一个将原始 Python 项目重构为 C++ 的版本，保留了核心功能。

## 📋 项目概述

EasyKiConverter C++ 版本是一个强大的工具，用于将嘉立创（LCSC）和 EasyEDA 元件转换为 KiCad 格式，支持符号、封装和 3D 模型的完整转换。

### ✨ 功能特性

#### 🎯 核心功能
- **符号转换**：将 EasyEDA 符号转换为 KiCad 符号库（.kicad_sym）
- **封装生成**：从 EasyEDA 封装创建 KiCad 封装（.kicad_mod）
- **3D模型支持**：自动下载并转换 3D 模型（支持多种格式）
- **命令行界面**：简洁高效的命令行操作界面
- **图形用户界面**：直观易用的桌面应用程序界面

### 🛠️ 技术架构

该项目使用现代 C++17 编写，具有以下特点：
- 模块化设计，易于维护和扩展
- 使用 CMake 构建系统
- 依赖管理清晰
- 面向对象设计

## 🚀 快速开始

### 📋 系统要求

- C++17 兼容的编译器 (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.10 或更高版本
- libcurl 开发库
- nlohmann/json 库
- Qt5 开发库（用于 GUI 版本）

### 📦 依赖安装

#### Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libcurl4-openssl-dev nlohmann-json3-dev qtbase5-dev
```

#### CentOS/RHEL/Fedora:
```bash
# CentOS/RHEL
sudo yum install gcc gcc-c++ make cmake libcurl-devel qt5-qtbase-devel
# 或者 Fedora
sudo dnf install gcc gcc-c++ make cmake libcurl-devel qt5-qtbase-devel

# nlohmann/json 需要单独安装
git clone https://github.com/nlohmann/json.git
cd json
mkdir build && cd build
cmake ..
sudo make install
```

#### macOS (使用 Homebrew):
```bash
brew install cmake curl nlohmann-json qt5
```

### 🔧 构建项目

```bash
# 克隆仓库
git clone <repository-url>
cd EasyKiConverter

# 创建构建目录
mkdir build && cd build

# 配置项目
cmake ..

# 编译项目
make

# 运行命令行版本
./bin/easykiconverter C123456

# 运行GUI版本
./bin/easykiconverter-gui
```

## 📚 使用说明

### 命令行版本

```bash
./bin/easykiconverter <LCSC_ID>
```

例如：
```bash
./bin/easykiconverter C123456
```

### GUI版本

运行 `./bin/easykiconverter-gui` 启动图形界面应用程序。GUI版本提供以下功能：

1. **组件ID输入**：可以输入单个或多个LCSC组件ID
2. **导出路径选择**：通过浏览按钮选择导出目录
3. **导出选项**：选择导出符号、封装和/或3D模型
4. **粘贴功能**：从剪贴板粘贴组件ID
5. **进度显示**：显示转换进度
6. **日志输出**：显示详细的转换日志

## 📖 代码结构

```
EasyKiConverter/
├── CMakeLists.txt                 # 根CMake配置文件
├── src/                           # 源代码目录
│   ├── CMakeLists.txt             # 源代码CMake配置
│   ├── main.cpp                   # 主程序入口（命令行版本）
│   ├── gui_main.cpp               # GUI程序入口
│   ├── easyeda/                   # EasyEDA相关功能
│   │   ├── CMakeLists.txt         # EasyEDA模块CMake配置
│   │   ├── EasyedaApi.h/cpp       # EasyEDA API接口
│   ├── kicad/                     # KiCad相关功能
│   │   ├── CMakeLists.txt         # KiCad模块CMake配置
│   │   ├── KicadSymbolExporter.h/cpp # KiCad符号导出功能
│   ├── gui/                       # GUI相关功能
│   │   ├── CMakeLists.txt         # GUI模块CMake配置
│   │   ├── MainWindow.h/cpp       # 主窗口实现
```

## 🖥️ GUI界面特性

GUI版本提供现代化的桌面应用程序界面，具有以下特性：

- **直观的操作界面**：所有功能都通过图形界面操作
- **组件ID批量处理**：支持同时处理多个组件
- **实时进度反馈**：通过进度条显示转换进度
- **配置保存**：自动保存用户设置
- **剪贴板支持**：可以直接从剪贴板粘贴组件ID
- **日志显示**：实时显示转换过程中的详细信息

## 🤝 贡献

欢迎提交 Issue 和 Pull Request 来改进这个项目。

## 📄 许可证

本项目采用 GNU General Public License v3.0 许可证。

## 🙏 致谢

特别感谢 [uPesy/easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py) 项目提供的基础框架和核心转换算法。