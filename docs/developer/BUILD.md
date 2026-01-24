# 本地编译指南

本文档介绍如何在本地编译 EasyKiConverter 项目。

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

## 安装依赖

### Windows

#### 安装 Qt

1. 访问 [Qt 官网](https://www.qt.io/download)
2. 下载 Qt Online Installer
3. 运行安装程序
4. 选择 Qt 6.10.1 或更高版本
5. 选择 MinGW 13.10 编译器
6. 安装所需的 Qt 模块：
   - Qt Quick
   - Qt Network
   - Qt Core
   - Qt Gui
   - Qt Widgets
   - Qt Quick Controls 2

#### 安装 CMake

1. 访问 [CMake 官网](https://cmake.org/download/)
2. 下载 Windows 安装包
3. 运行安装程序
4. 将 CMake 添加到系统 PATH

#### 安装编译器

**MinGW**
1. Qt 安装程序会自动安装 MinGW
2. 或从 [MinGW-w64](https://www.mingw-w64.org/) 下载

**MSVC**
1. 安装 Visual Studio 2019 或更高版本
2. 在安装时选择 "使用 C++ 的桌面开发" 工作负载

### macOS

#### 使用 Homebrew 安装

```bash
# 安装 Homebrew（如果尚未安装）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 安装 Qt
brew install qt@6

# 安装 CMake
brew install cmake

# 安装 Xcode 命令行工具
xcode-select --install
```

#### 从 Qt 官网安装

1. 访问 [Qt 官网](https://www.qt.io/download)
2. 下载 macOS 安装包
3. 运行安装程序
4. 安装 Qt 6.10.1 或更高版本

### Linux

**Ubuntu/Debian**

```bash
# 更新包管理器
sudo apt-get update

# 安装 Qt 6
sudo apt-get install qt6-base-dev qt6-declarative-dev qt6-tools-dev qt6-network-dev

# 安装 CMake
sudo apt-get install cmake

# 安装编译器
sudo apt-get install build-essential g++

# 安装 zlib
sudo apt-get install zlib1g-dev
```

**Fedora**

```bash
# 安装 Qt 6
sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qttools-devel qt6-qtnetworkauth-devel

# 安装 CMake
sudo dnf install cmake

# 安装编译器
sudo dnf install gcc-c++

# 安装 zlib
sudo dnf install zlib-devel
```

**Arch Linux**

```bash
# 安装 Qt 6
sudo pacman -S qt6-base qt6-declarative qt6-tools qt6-networkauth

# 安装 CMake
sudo pacman -S cmake

# 安装编译器
sudo pacman -S gcc

# 安装 zlib
sudo pacman -S zlib
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

### Windows + MSVC

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/msvc2019_64"

# 编译项目（Debug 版本）
cmake --build . --config Debug

# 编译项目（Release 版本）
cmake --build . --config Release

# 运行应用程序
./bin/Debug/EasyKiConverter.exe
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

这将启用调试数据导出功能，用于调试转换过程。

### 多线程编译

**Windows (MSVC)**

MSVC 编译器会自动使用 /MP 选项进行多线程编译，无需额外参数。

**Windows (MinGW)**

```bash
# 使用 16 个并行任务编译
cmake --build . --config Debug -- -j 16

# 或使用系统最大核心数
cmake --build . --config Debug -- -j
```

**Linux/macOS**

```bash
# 使用 16 个并行任务编译
cmake --build . --config Debug -- -j 16

# 或使用系统最大核心数
cmake --build . --config Debug -- -j$(nproc)
```

**注意：**
- 并行编译可以显著加快编译速度，特别是在多核 CPU 上
- 建议根据 CPU 核心数调整并行任务数（通常设置为 CPU 核心数的 1-2 倍）
- 如果遇到内存不足问题，可以减少并行任务数

### 指定构建类型

```bash
# Debug 版本
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release 版本
cmake .. -DCMAKE_BUILD_TYPE=Release

# RelWithDebInfo 版本
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
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

## 常见问题

### 找不到 Qt

**错误信息：**
```
Could not find Qt6
```

**解决方案：**
确保 CMAKE_PREFIX_PATH 指向正确的 Qt 安装路径。

```bash
cmake .. -DCMAKE_PREFIX_PATH="/path/to/Qt/6.10.1/gcc_64"
```

### 找不到 zlib

**错误信息：**
```
Could not find ZLIB
```

**解决方案：**

**Windows：**
Qt 通常自带 zlib，确保 Qt 安装完整。

**macOS：**
```bash
brew install zlib
```

**Linux：**
```bash
sudo apt-get install zlib1g-dev  # Ubuntu/Debian
sudo dnf install zlib-devel       # Fedora
sudo pacman -S zlib               # Arch Linux
```

### 找不到 Qt 动态库

**错误信息：**
```
Failed to load platform plugin "windows"
```

**解决方案：**

**Windows：**
将 Qt 的 bin 目录添加到 PATH 环境变量。

```bash
set PATH=C:\Qt\6.10.1\mingw_64\bin;%PATH%
```

**Linux/macOS：**
```bash
export PATH="/path/to/Qt/6.10.1/gcc_64/bin:$PATH"
export LD_LIBRARY_PATH="/path/to/Qt/6.10.1/gcc_64/lib:$LD_LIBRARY_PATH"
```

### 编译错误

**错误信息：**
```
error: 'xxx' was not declared in this scope
```

**解决方案：**

1. 检查 Qt 版本是否正确
2. 检查编译器版本是否支持 C++17
3. 清理构建目录重新编译

```bash
cd build
cmake --build . --target clean
cmake ..
cmake --build .
```

### 链接错误

**错误信息：**
```
undefined reference to `xxx'
```

**解决方案：**

1. 检查 CMakeLists.txt 中的链接库配置
2. 确保所有必需的 Qt 模块都已链接
3. 检查编译器是否支持所有使用的 C++ 特性

## 清理构建

### 清理构建目录

```bash
cd build
cmake --build . --target clean
```

### 完全清理

```bash
# 删除构建目录
rm -rf build

# Windows
rmdir /s /q build
```

## 安装

### 安装到指定目录

```bash
cmake --install . --prefix /path/to/install
```

### 系统范围安装（Linux）

```bash
sudo cmake --install . --prefix /usr/local
```

## 打包

### Windows

**生成便携版 (Portable Zip):**
使用 windeployqt 工具打包依赖项：

```bash
cd build/bin/Release
windeployqt --release --no-translations EasyKiConverter.exe
# 然后压缩该目录
```

**生成安装包 (Installer .exe):**
需要安装 [Inno Setup 6](https://jrsoftware.org/isinfo.php)。

1. 确保已运行 `windeployqt` 填充了依赖。
2. 将 `build/bin/Release` 内容复制到临时目录 `build/Staging`。
3. 运行编译器：
   ```bash
   iscc "/DMyAppVersion=3.0.0" "/DSourceDir=path\to\staging" "resources\windows_setup.iss"
   ```

### macOS

使用 macdeployqt 工具打包依赖项：

```bash
cd build/bin
macdeployqt EasyKiConverter.app
```

### Linux

创建 AppImage 或使用系统包管理器安装依赖项。

## 性能优化

### 启用编译器优化

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

### 使用链接时优化（LTO）

```bash
cmake .. -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

### 使用静态链接

```bash
cmake .. -DBUILD_SHARED_LIBS=OFF
```

## 调试

### 启用调试符号

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

### 使用 GDB 调试

```bash
cd build/bin
gdb ./EasyKiConverter
```

### 使用 Qt Creator 调试

1. 在 Qt Creator 中打开项目
2. 设置断点
3. 点击 "调试"按钮（或按 F5）

## 获取帮助

如果遇到编译问题：

1. 查阅本文档的常见问题部分
2. 搜索 [GitHub Issues](https://github.com/tangsangsimida/EasyKiConverter_QT/issues)
3. 提交新的 Issue，提供以下信息：
   - 操作系统版本
   - Qt 版本
   - CMake 版本
   - 编译器版本
   - 完整的错误信息
   - CMake 配置命令

## 相关资源

- [Qt 官网](https://www.qt.io/)
- [CMake 官网](https://cmake.org/)
- [MinGW-w64](https://www.mingw-w64.org/)
- [项目主页](https://github.com/tangsangsimida/EasyKiConverter_QT)

## CI/CD 架构

本项目使用 GitHub Actions 实现全自动化 CI/CD。

*   **依赖管理**: 使用 `vcpkg.json` (Manifest Mode) 统一管理 CI 环境依赖。
*   **本地 Actions**:
    *   `setup-env`: 统一配置 CMake, Qt, ccache/sccache。
    *   `get-version`: 统一从 Git Tag 提取版本号。
*   **缓存策略**: 启用了 Qt 安装缓存、vcpkg 二进制缓存以及 CMake 编译器缓存 (ccache/sccache)。
*   **安全性**: 所有工作流均配置了最小权限和并发控制。

详细依赖关系请参考 `.github/workflows/WORKFLOW_DEPENDENCIES.md`。