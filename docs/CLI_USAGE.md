# CLI 模式使用说明

EasyKiConverter 支持命令行模式，可以在不打开 UI 界面的情况下进行转换操作。

## 子命令

```bash
# 转换 BOM 表
easykiconverter convert bom -i <bom_file> -o <output_dir> [options]

# 转换单个元器件
easykiconverter convert component -c <lcsc_id> -o <output_dir> [options]

# 批量转换
easykiconverter convert batch -i <component_list_file> -o <output_dir> [options]
```

## 通用参数

| 参数 | 简写 | 描述 |
|------|------|------|
| `--input` | `-i` | 输入文件路径 |
| `--output` | `-o` | 输出目录路径 |
| `--component` | `-c` | LCSC 元器件编号 |
| `--symbol` | | 导出符号库 (默认: true) |
| `--footprint` | | 导出封装库 (默认: true) |
| `--3d-model` | | 导出 3D 模型 |
| `--preview` | | 导出预览图 |
| `--progress` | | 显示进度条 |
| `--quiet` | `-q` | 安静模式 |

## 使用示例

```bash
# 转换 BOM 表
easykiconverter convert bom -i my_project.xlsx -o ./kicad_libs

# 转换单个元器件
easykiconverter convert component -c C12345 -o ./output

# 批量转换
easykiconverter convert batch -i components.txt -o ./output --3d-model

# 带进度条
easykiconverter convert bom -i bom.xlsx -o ./output --progress
```

## 架构说明

CLI 模块采用低耦合设计，包含以下组件：

- `CommandLineParser` - 命令行参数解析器
- `CliContext` - CLI 上下文，保存共享状态
- `CliPrinter` - CLI 输出工具
- `FileReader` - 文件读取工具
- `BaseConverter` - 转换器基类
- `BomConverter` - BOM 表转换器
- `ComponentConverter` - 单个元器件转换器
- `BatchConverter` - 批量转换器
- `CliConverter` - 主协调器
