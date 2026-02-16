# 工具 (Tools)

此目录包含 EasyKiConverter 项目的各种开发、构建和维护工具。

> [!IMPORTANT]
> **使用须知**:
> - 运行脚本前，请确保您已满足该工具的所有先决条件。
> - 建议始终在项目的根目录执行工具，以确保路径引用正确。
> - 请在执行可能修改源代码的工具（如格式化工具）前，确保您的工作区是干净的（已提交或备份）。

## 目录结构

```
tools/
├── windows/          # Windows 平台特定工具
│   ├── format_code.bat
│   └── build_project.bat     # 一键构建脚本 [NEW]
├── linux/            # Linux 平台特定工具 (暂未实现)
├── macos/            # macOS 平台特定工具 (暂未实现)
└── python/           # 跨平台 Python 工具
    ├── manage_version.py
    ├── manage_translations.py # 翻译管理工具 [NEW]
    ├── check_env.py          # 环境检查工具 [NEW]
    └── count_lines.py        # 代码行数统计工具 [NEW]
```

## Windows 工具

### [format_code.bat](file:///C:/Users/48813/Desktop/workspace/github_projects/EasyKiConverter_QT/tools/windows/format_code.bat)

基于 `clang-format` 的 C++ 代码格式化工具。

### [build_project.bat](file:///C:/Users/48813/Desktop/workspace/github_projects/EasyKiConverter_QT/tools/windows/build_project.bat)

Windows 下的一键构建脚本，支持 Debug/Release 模式。

**快速用法:**
```bash
# 默认 Debug 构建
tools\windows\build_project.bat
# Release 构建
tools\windows\build_project.bat Release
```

> [!TIP]
> 更多关于环境配置、功能细节和注意事项，请参阅对应脚本头部的详细注释。

## Python 工具

### [manage_version.py](file:///C:/Users/48813/Desktop/workspace/github_projects/EasyKiConverter_QT/tools/python/manage_version.py)

跨平台版本同步管理工具。

### [manage_translations.py](file:///C:/Users/48813/Desktop/workspace/github_projects/EasyKiConverter_QT/tools/python/manage_translations.py)

自动化 Qt 翻译流程（lupdate/lrelease）。

**快速用法:**
```bash
# 更新并编译所有翻译文件
python tools/python/manage_translations.py --all
```

### [check_env.py](file:///C:/Users/48813/Desktop/workspace/github_projects/EasyKiConverter_QT/tools/python/check_env.py)

开发依赖环境检查工具。

**快速用法:**
```bash
python tools/python/check_env.py
```

### [count_lines.py](file:///C:/Users/48813/Desktop/workspace/github_projects/EasyKiConverter_QT/tools/python/count_lines.py)

跨平台代码行数统计工具，以 KLoC (千行代码) 为单位输出项目规模。

**快速用法:**
```bash
python tools/python/count_lines.py
```

> [!TIP]
> 以上 Python 工具均支持优先使用环境变量中的工具链，若未配置则回退到脚本内定义的路径变量。详情请参阅各脚本顶部的说明文档。

## 命令行帮助支持

所有命令行工具必须支持 `--help` 或 `-h` 参数，以提供使用说明。

### 实现要求

| 工具类型 | 推荐实现方式 | 示例 |
|----------|-------------|------|
| Python 脚本 | 使用 `argparse` 模块 | `parser = argparse.ArgumentParser(...)` |
| Windows 批处理 | 检测 `-h`, `--help`, `/h` 参数 | `if "%~1"=="--help" goto help` |
| Shell 脚本 | 检测 `-h`, `--help` 参数 | `case "$1" in --help) show_help ;; esac` |

### 帮助内容要求

帮助信息应包含：
1. **工具名称和简介** - 一句话说明工具用途
2. **用法格式** - 命令行语法 (Usage)
3. **参数说明** - 每个参数的含义和默认值
4. **使用示例** - 常见使用场景
5. **环境要求** - 依赖的工具或库

### 示例 (Python)

```python
import argparse

def main():
    parser = argparse.ArgumentParser(
        description="工具简述",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python tool.py --option value
  python tool.py --help
        """
    )
    parser.add_argument("--input", help="输入文件路径")
    parser.add_argument("--verbose", action="store_true", help="详细输出")
    args = parser.parse_args()
```

### 当前工具状态

| 工具 | `--help` 支持 |
|------|---------------|
| `manage_version.py` | ✅ |
| `convert_to_utf8.py` | ✅ |
| `build_project.py` | ✅ |
| `manage_translations.py` | ✅ |
| `build_docs.py` | ✅ |
| `format_code.py` | ✅ |
| `analyze_lines.py` | ⚠️ 待添加 |
| `check_env.py` | ⚠️ 待添加 |
| `count_lines.py` | ⚠️ 待添加 |
| `fix_qml_translations.py` | ⚠️ 待添加 |
| `build_project.bat` | ✅ |
| `format_code.bat` | ⚠️ 待添加 |
| `format_qml.bat` | ⚠️ 待添加 |

## 添加新工具

为了保持项目工具链的可维护性和一致性，添加新工具时必须遵循以下严格要求：

1.  **目录规范**: 根据工具类型和目标平台，将其放置在合适的子目录中 (`windows/`, `linux/`, `macos/` 或 `python/`)。
2.  **详细文档**:
    - 必须在下方“工具列表”中添加该工具的详细描述。
    - 必须明确列出所有环境要求（如 Python 版本、依赖库、特定环境变量或第三方二进制文件）。
    - 必须提供清晰的使用示例和命令行说明。
3.  **多平台考虑**: 如果工具逻辑是通用的，应优先使用 Python 等跨平台语言实现。如果是平台特定的，请确保在对应目录中提供清晰的文档。
4.  **错误处理**: 工具应包含基本的错误检查（例如：检查依赖是否存在），并提供友好的错误提示。
5.  **代码风格**: 新添加的脚本或代码必须符合项目的整体编码风格。
6.  **同步更新**: 更新此 README 文档的中文和英文版本（如果存在），确保信息同步。
7.  **验证**: 在提交前，请在干净的环境中验证工具的功能及其文档的准确性。
