# 快速开始

本文档将帮助您快速开始使用 EasyKiConverter。

## 环境要求

### 操作系统

- Windows 10/11（推荐）
- macOS
- Linux

### Qt 版本

- Qt 6.8 或更高版本
- 推荐 Qt 6.10.1

### CMake 版本

- CMake 3.16 或更高版本

### 编译器

**Windows**
- MinGW 13.10（推荐）
- MSVC 2019+

**macOS**
- Clang（Xcode 12+）

**Linux**
- GCC 9+
- Clang 10+

### Qt 模块

需要安装以下 Qt 模块：

- Qt Quick
- Qt Network
- Qt Core
- Qt Gui
- Qt Widgets
- Qt Quick Controls 2

### 第三方库

- zlib（用于 GZIP 解压缩）

## 安装 Qt

### Windows

1. 访问 [Qt 官网](https://www.qt.io/download)
2. 下载 Qt Online Installer
3. 运行安装程序
4. 选择 Qt 6.10.1 或更高版本
5. 选择 MinGW 13.10 编译器
6. 安装所需的 Qt 模块

### macOS

```bash
# 使用 Homebrew 安装
brew install qt@6

# 或从 Qt 官网下载安装包
```

### Linux

**Ubuntu/Debian**

```bash
sudo apt-get update
sudo apt-get install qt6-base-dev qt6-declarative-dev qt6-tools-dev cmake build-essential
```

**Fedora**

```bash
sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel cmake gcc-c++
```

**Arch Linux**

```bash
sudo pacman -S qt6-base qt6-declarative cmake gcc
```

## 获取源代码

### Clone 仓库

```bash
git clone https://github.com/tangsangsimida/EasyKiConverter_QT.git
cd EasyKiConverter_QT
```

### Fork 并 Clone（如果您计划贡献）

1. 在 GitHub 上 Fork 本项目
2. Clone 您的 Fork：

```bash
git clone https://github.com/your-username/EasyKiConverter_QT.git
cd EasyKiConverter_QT
```

## 编译项目

### Windows + MinGW

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64"

# 编译项目（Debug 版本）
cmake --build . --config Debug

# 编译项目（Release 版本）
cmake --build . --config Release

# 运行应用程序
./bin/EasyKiConverter.exe
```

### macOS

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake .. -DCMAKE_PREFIX_PATH="/usr/local/Qt-6.10.1"

# 编译项目（Debug 版本）
cmake --build . --config Debug

# 编译项目（Release 版本）
cmake --build . --config Release

# 运行应用程序
./bin/EasyKiConverter
```

### Linux

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake .. -DCMAKE_PREFIX_PATH="/opt/Qt/6.10.1/gcc_64"

# 编译项目（Debug 版本）
cmake --build . --config Debug

# 编译项目（Release 版本）
cmake --build . --config Release

# 运行应用程序
./bin/EasyKiConverter
```

## 使用 Qt Creator 编译

1. 安装 Qt Creator
2. 打开 Qt Creator
3. 选择 "文件" -> "打开文件或项目"
4. 选择项目根目录下的 `CMakeLists.txt`
5. 配置构建套件（Kit）：
   - 选择 Qt 版本（Qt 6.10.1 或更高）
   - 选择编译器（MinGW、Clang 或 GCC）
6. 点击 "配置项目"
7. 点击 "构建"按钮（或按 Ctrl+B）
8. 点击 "运行"按钮（或按 F5）启动应用程序

## 编译选项

### 启用调试导出功能

```bash
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64" -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=ON
```

这将启用调试数据导出功能。

### 多线程编译

**Windows (MSVC)**

```bash
cmake --build . --config Debug -- /MP
```

**Linux/macOS**

```bash
cmake --build . --config Debug -- -j$(nproc)
```

## 运行测试

### 构建测试程序

```bash
cd tests
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64"
cmake --build build
```

### 运行测试

```bash
# 运行图层映射测试
./build/test_layer_mapping.exe

# 运行 UUID 提取测试
./build/test_uuid_extraction.exe

# 运行 Service 层测试
./build/test_component_service.exe
./build/test_export_service.exe
./build/test_config_service.exe

# 运行集成测试
./build/test_integration.exe

# 运行性能测试
./build/test_performance.exe
```

详细测试说明请参考：
- [单元测试指南](../tests/TESTING_GUIDE.md)
- [集成测试指南](../tests/INTEGRATION_TEST_GUIDE.md)
- [性能测试指南](../tests/PERFORMANCE_TEST_GUIDE.md)

## 使用应用程序

### 启动应用程序

```bash
# Windows
./bin/EasyKiConverter.exe

# macOS/Linux
./bin/EasyKiConverter
```

### 基本使用流程

1. **输入元件编号**
   - 在输入框中输入 LCSC 元件编号
   - 或使用智能提取功能从剪贴板粘贴文本
   - 或导入 BOM 文件批量导入元件

2. **配置导出选项**
   - 选择导出目录
   - 选择要导出的内容（符号、封装、3D 模型）
   - 配置其他选项（如覆盖文件）

3. **开始转换**
   - 点击"开始转换"按钮
   - 等待转换完成
   - 查看转换结果

4. **查看结果**
   - 在结果列表中查看转换状态
   - 点击结果项查看详细信息
   - 打开导出目录查看生成的文件

### 界面功能

- **元件输入区**：输入或导入元件编号
- **导出设置区**：配置导出选项
- **进度显示区**：显示转换进度和状态
- **结果列表区**：显示转换结果
- **主题切换**：切换深色/浅色主题

## 常见问题

### 编译失败

**问题：找不到 Qt**

```bash
# 解决方案：确保 CMAKE_PREFIX_PATH 指向正确的 Qt 安装路径
cmake .. -DCMAKE_PREFIX_PATH="/path/to/Qt/6.10.1/gcc_64"
```

**问题：找不到 zlib**

```bash
# 解决方案：确保 zlib 已安装
# Windows：Qt 通常自带 zlib
# macOS：brew install zlib
# Linux：sudo apt-get install zlib1g-dev
```

### 运行失败

**问题：找不到 Qt 动态库**

```bash
# 解决方案：将 Qt 的 bin 目录添加到 PATH
export PATH="/path/to/Qt/6.10.1/gcc_64/bin:$PATH"
```

**问题：应用程序无法启动**

```bash
# 解决方案：确保所有依赖项都已安装
# 检查 Qt 模块是否完整
# 检查编译器版本是否兼容
```

### 转换失败

**问题：网络请求失败**

```bash
# 解决方案：
# 1. 检查网络连接
# 2. 检查防火墙设置
# 3. 确认 EasyEDA API 可访问
# 4. 查看错误日志
```

**问题：元件编号无效**

```bash
# 解决方案：
# 1. 确认元件编号格式正确
# 2. 确认元件存在于 EasyEDA 数据库
# 3. 尝试使用其他元件编号测试
```

## 获取帮助

如果遇到问题：

1. 查阅项目文档
2. 查看 [常见问题](#常见问题)
3. 搜索 [GitHub Issues](https://github.com/tangsangsimida/EasyKiConverter_QT/issues)
4. 提交新的 Issue

## 下一步

- 阅读功能特性文档：[FEATURES.md](FEATURES.md)
- 了解架构设计：[ARCHITECTURE.md](ARCHITECTURE.md)
- 参与贡献：[CONTRIBUTING.md](CONTRIBUTING.md)
- 查看编译指南：[BUILD.md](BUILD.md)

## 相关资源

- [EasyEDA 官网](https://easyeda.com/)
- [KiCad 官网](https://www.kicad.org/)
- [Qt 官网](https://www.qt.io/)
- [CMake 官网](https://cmake.org/)
- [项目主页](https://github.com/tangsangsimida/EasyKiConverter_QT)