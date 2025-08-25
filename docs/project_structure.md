# 📁 项目结构

[English Version](project_structure_en.md)

```
EasyKiConverter/
├── src/                               # C++源代码目录
│   ├── main.cpp                       # 命令行工具主入口
│   ├── gui_main.cpp                   # GUI应用程序主入口
│   ├── easyeda/                       # EasyEDA API 和数据处理
│   │   ├── EasyedaApi.cpp            # EasyEDA API 客户端
│   │   ├── EasyedaApi.h              # EasyEDA API 头文件
│   │   ├── EasyedaImporter.cpp       # 数据导入器
│   │   └── EasyedaImporter.h         # 数据导入器头文件
│   ├── kicad/                        # KiCad 导出引擎
│   │   ├── KicadSymbolExporter.cpp   # 符号导出器
│   │   ├── KicadSymbolExporter.h     # 符号导出器头文件
│   │   ├── export_kicad_footprint.cpp # 封装导出器（待实现）
│   │   └── export_kicad_3d_model.cpp # 3D模型导出器（待实现）
│   └── gui/                          # Qt GUI界面
│       ├── MainWindow.cpp            # 主窗口实现
│       ├── MainWindow.h              # 主窗口头文件
│       ├── ComponentManager.cpp      # 组件管理器实现
│       └── ComponentManager.h        # 组件管理器头文件
├── docs/                              # 详细文档目录
│   ├── README.md                     # 文档索引
│   ├── project_structure.md          # 项目结构详细说明
│   ├── development_guide.md          # 开发指南
│   ├── contributing.md               # 贡献指南
│   ├── performance.md                # 性能优化说明
│   ├── system_requirements.md        # 系统要求
│   ├── project_structure_en.md       # 项目结构（英文版）
│   ├── development_guide_en.md       # 开发指南（英文版）
│   ├── contributing_en.md            # 贡献指南（英文版）
│   ├── performance_en.md             # 性能优化说明（英文版）
│   └── system_requirements_en.md     # 系统要求（英文版）
├── test_data/                         # 测试数据
│   └── C2040.json                    # 示例元件数据
├── CMakeLists.txt                     # CMake构建配置
├── LICENSE                           # GPL-3.0 许可证
├── README.md                         # 中文文档
└── README_en.md                      # 英文文档
```

## 📋 核心模块说明

### 🎯 命令行工具
| 文件 | 功能描述 |
|------|----------|
| **main.cpp** | 命令行界面主入口，处理参数解析、验证，并协调整个转换过程 |
| **gui_main.cpp** | GUI应用程序入口，初始化Qt应用程序 |

### 🖼️ GUI 界面
| 文件 | 功能描述 |
|------|----------|
| **MainWindow.cpp/.h** | Qt主窗口实现，提供图形用户界面和用户交互功能 |
| **ComponentManager.cpp/.h** | 组件管理器，负责管理组件列表和相关操作 |

### 📚 文档目录
| 文件 | 功能描述 |
|------|----------|
| **README.md** | 文档索引，提供所有文档的链接和简要说明 |
| **project_structure.md** | 详细的项目结构和模块说明 |
| **development_guide.md** | 开发环境设置和开发流程指南 |
| **contributing.md** | 项目贡献流程和规范 |
| **performance.md** | 性能优化说明 |
| **system_requirements.md** | 系统要求和支持的元件类型 |

### 🔧 核心引擎
| 模块 | 功能描述 |
|------|----------|
| **easyeda/** | EasyEDA API客户端和数据处理模块 |
| **kicad/** | KiCad格式导出引擎，支持符号、封装和3D模型 |

### 📦 数据处理流程
1. **API获取**：从EasyEDA/LCSC获取元件数据
2. **数据解析**：解析符号、封装和3D模型信息
3. **格式转换**：转换为KiCad兼容格式
4. **文件生成**：输出.kicad_sym、.kicad_mod等文件

### 🛠️ 构建系统
C++版本使用CMake作为构建系统，支持跨平台编译：
- **CMakeLists.txt**: 项目根目录的CMake配置文件
- **src/CMakeLists.txt**: 源代码目录的CMake配置文件
- **src/easyeda/CMakeLists.txt**: EasyEDA模块的CMake配置文件
- **src/kicad/CMakeLists.txt**: KiCad模块的CMake配置文件
- **src/gui/CMakeLists.txt**: GUI模块的CMake配置文件

### 🎯 可执行文件
构建完成后将生成以下可执行文件：
- **easykiconverter**: 命令行版本的转换工具
- **easykiconverter-gui**: 图形界面版本的转换工具