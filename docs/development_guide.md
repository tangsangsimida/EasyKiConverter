# 📚 开发指南

[English Version](development_guide_en.md)

## 🌟 开发环境设置

### 🛠️ 工具链
- **编译器**: GCC (Linux/macOS), MSVC (Windows)
- **构建工具**: CMake >= 3.10
- **依赖管理**: vcpkg 或系统包管理器
- **Qt版本**: Qt5 (建议使用系统安装的Qt库)

### 📦 必要的依赖项
```bash
# Linux/macOS
sudo apt-get install build-essential cmake qt5-default

# Windows
# 使用vcpkg安装依赖
./vcpkg install qt5
```

## 🚀 开发流程

### 1. 克隆仓库
```bash
git clone <repository-url>
cd EasyKiConverter
```

### 2. 创建构建目录
```bash
mkdir build
cd build
```

### 3. 配置CMake
```bash
cmake ..
```

### 4. 构建项目
```bash
make
```

### 5. 运行测试程序（可选）
```bash
# 在build目录下运行测试程序
ctest
```

## 🖥️ IDE 设置

- **Visual Studio Code**:
  - 安装 C/C++ 扩展
  - 配置 CMake Tools 扩展
  - 配置 launch.json 以支持调试

- **CLion**:
  - 导入项目时选择CMakeLists.txt
  - 自动配置构建环境

## 🧪 调试技巧

- 使用 GDB 或 LLDB 进行调试
- 配置 VSCode 的 launch.json 文件以支持断点调试
- 使用 CMake 的 Debug 模式构建项目

## 📂 目录结构
参照 [项目结构](project_structure.md) 文档


```

## 🔧 代码结构

- **easyeda/** - EasyEDA API 和数据处理
- **kicad/** - KiCad 格式导出引擎
- **gui/** - Qt GUI 界面
- **main.cpp** - 命令行入口
- **gui_main.cpp** - GUI入口

## 🔧 命令行选项

```bash
python main.py [options]

必需参数:
  --lcsc_id TEXT         要转换的LCSC元件编号 (例如: C13377)

导出选项 (至少需要一个):
  --symbol               导出符号库 (.kicad_sym)
  --footprint            导出封装库 (.kicad_mod)
  --model3d              导出3D模型

可选参数:
  --output_dir PATH      输出目录路径 [默认: ./output]
  --lib_name TEXT        库文件名称 [默认: EasyKiConverter]
  --kicad_version INT    KiCad版本 (5 或 6) [默认: 6]
  --overwrite            覆盖现有文件
  --debug                启用详细日志
  --help                 显示帮助信息
```

### 📝 使用示例

```bash
# 导出所有内容到默认目录
python main.py --lcsc_id C13377 --symbol --footprint --model3d

# 仅导出符号到指定目录
python main.py --lcsc_id C13377 --symbol --output_dir ./my_symbols

# 导出到自定义库名称
python main.py --lcsc_id C13377 --symbol --footprint --lib_name MyComponents

# 启用调试模式
python main.py --lcsc_id C13377 --symbol --debug
```