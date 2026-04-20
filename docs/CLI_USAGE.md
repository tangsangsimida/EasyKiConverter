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

| 参数 | 简写 | 描述 | 默认值 |
|------|------|------|--------|
| `--input` | `-i` | 输入文件路径 | - |
| `--output` | `-o` | 输出目录路径 | - |
| `--component` | `-c` | LCSC 元器件编号 | - |
| `--symbol` | | 导出符号库 | true |
| `--footprint` | | 导出封装库 | true |
| `--3d-model` | | 导出 3D 模型 (WRL 格式) | true |
| `--preview` | | 导出预览图 | false |
| `--progress` | | 显示进度条 | false |
| `--quiet` | `-q` | 安静模式 | false |
| `--debug` | `-d` | 调试模式 (生成详细报告) | false |
| `--completion` | | 生成 Shell 补全脚本 | - |

## 默认导出选项

CLI 模式默认导出以下内容：
- 符号库 (Symbol)
- 封装库 (Footprint)
- 3D 模型 (WRL 格式)

**注意**：
- 默认不导出预览图和数据手册
- 默认使用 WRL 格式的 3D 模型
- 普通模式不生成详细报告，仅在调试模式 (`--debug`) 下生成

## 自动补全

EasyKiConverter 支持 Shell 自动补全，包括参数选项和动态补全 LCSC ID。

### Bash 补全

```bash
# 临时启用 (当前会话)
eval "$(easykiconverter --completion bash)"

# 永久启用
easykiconverter --completion bash >> ~/.bash_completion
echo 'source ~/.bash_completion' >> ~/.bashrc
```

### Zsh 补全

```bash
# 临时启用 (当前会话)
eval "$(easykiconverter --completion zsh)"

# 永久启用
mkdir -p ~/.zsh
easykiconverter --completion zsh > ~/.zsh/_easykiconverter
echo 'fpath=(~/.zsh $fpath)' >> ~/.zshrc
echo 'autoload -Uz compinit && compinit' >> ~/.zshrc
```

### Fish 补全

```bash
# 永久启用
easykiconverter --completion fish > ~/.config/fish/completions/easykiconverter.fish
```

### 补全功能

- 参数选项补全：自动补全 `--help`, `--input`, `--output` 等选项
- 子命令补全：自动补全 `convert`, `bom`, `component`, `batch` 等子命令
- 文件补全：输入 `-i` 后自动补全 `.xlsx`, `.csv` 等文件
- 目录补全：输入 `-o` 后自动补全目录路径
- **LCSC ID 动态补全**：输入 `-c` 后自动补全已缓存的元器件编号

## 使用示例

```bash
# 默认导出 (符号库 + 封装库 + 3D模型-WRL)
easykiconverter convert bom -i my_project.xlsx -o ./kicad_libs

# 仅导出符号库和封装库，不导出 3D 模型
easykiconverter convert bom -i my_project.xlsx -o ./output --3d-model false

# 导出所有内容，包括预览图
easykiconverter convert bom -i my_project.xlsx -o ./output --preview

# 调试模式 (生成详细报告)
easykiconverter convert bom -i my_project.xlsx -o ./output --debug

# 带进度条
easykiconverter convert bom -i bom.xlsx -o ./output --progress

# 转换单个元器件
easykiconverter convert component -c C12345 -o ./output

# 批量转换
easykiconverter convert batch -i components.txt -o ./output
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
- `CompletionGenerator` - Shell 补全脚本生成器
