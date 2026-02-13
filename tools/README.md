# 工具 (Tools)

此目录包含 EasyKiConverter 项目的各种开发和构建工具。

## 目录结构

```
tools/
├── windows/          # Windows 平台特定工具
│   └── format_code.bat
├── linux/            # Linux 平台特定工具 (暂未实现)
├── macos/            # macOS 平台特定工具 (暂未实现)
└── python/           # 跨平台 Python 工具
    └── manage_version.py
```

## Windows 工具

### format_code.bat

使用 clang-format 自动格式化 C++ 和 QML 源代码。

**用法:**
```bash
cd tools/windows
format_code.bat
```

**要求:**
- 必须安装 clang-format 并将其添加到 PATH 环境变量中
- 项目根目录下应存在 `.clang-format` 配置文件

**功能:**
- 格式化项目中所有的 `.cpp`, `.h` 和 `.qml` 文件
- 使用项目根目录下的 `.clang-format` 配置文件
- 确保整个代码库的代码风格一致

## Python 工具

### manage_version.py

自动同步 `vcpkg.json`, `CMakeLists.txt` 和 `src/main.cpp` 中的项目版本信息。

**用法:**
```bash
# 检查当前版本
python tools/python/manage_version.py --check

# 将版本更新为 3.0.7
python tools/python/manage_version.py 3.0.7
```

**要求:**
- Python 3.x
- 建议从项目根目录运行。

**更新的文件:**
- `vcpkg.json`: 更新 `"version-string"` 字段。
- `CMakeLists.txt`: 更新默认的 `VERSION_FROM_CI` 和 `qt_add_qml_module` 版本。
- `src/main.cpp`: 更新 `app.setApplicationVersion`。

## 添加新工具

添加新工具时：
1. 创建合适的子目录 (`windows/`, `linux/`, `macos/` 或 `python/`)
2. 添加工具脚本或可执行文件
3. 更新此 README 文档（包括中文和英文版本）
4. 考虑添加使用示例和具体要求
