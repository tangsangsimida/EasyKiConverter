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
    └── check_env.py          # 环境检查工具 [NEW]
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

> [!TIP]
> 以上 Python 工具均支持优先使用环境变量中的工具链，若未配置则回退到脚本内定义的路径变量。详情请参阅各脚本顶部的说明文档。

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
