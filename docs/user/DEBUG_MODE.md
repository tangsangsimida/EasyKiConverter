# 调试模式配置

## 概述

调试模式允许您在导出过程中导出详细的调试信息，包括：
- 原始组件数据（组件信息、CAD 数据、3D 模型数据）
- 符号和封装数据
- 每个阶段的调试日志（抓取、处理、写入）
- 性能计时信息
- 错误详情和堆栈跟踪

## 配置方法

### 方法 1：环境变量（推荐）

调试模式由 `EASYKICONVERTER_DEBUG_MODE` 环境变量控制。

#### Windows (PowerShell)

**临时（仅当前会话）：**
```powershell
$env:EASYKICONVERTER_DEBUG_MODE="true"
```

**永久（系统范围）：**
```powershell
# 为当前用户设置
[System.Environment]::SetEnvironmentVariable("EASYKICONVERTER_DEBUG_MODE", "true", "User")

# 为所有用户设置（需要管理员权限）
[System.Environment]::SetEnvironmentVariable("EASYKICONVERTER_DEBUG_MODE", "true", "Machine")

# 仅当前进程设置
$env:EASYKICONVERTER_DEBUG_MODE="true"
```

**Windows (CMD)：**
```cmd
REM 临时
set EASYKICONVERTER_DEBUG_MODE=true

REM 永久（需要重启）
setx EASYKICONVERTER_DEBUG_MODE "true"
```

#### Linux/macOS

**临时（仅当前会话）：**
```bash
export EASYKICONVERTER_DEBUG_MODE=true
```

**永久（添加到 ~/.bashrc 或 ~/.zshrc）：**
```bash
echo 'export EASYKICONVERTER_DEBUG_MODE=true' >> ~/.bashrc
source ~/.bashrc
```

#### 接受的值

- `true`、`1`、`yes` - 启用调试模式
- `false`、`0`、`no` 或未设置 - 禁用调试模式

### 方法 2：CMake 构建选项

从源代码构建时，可以启用调试模式符号/封装导出：

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64" -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=ON
```

这将编译具有额外调试导出功能的代码。

### 方法 3：配置文件（备用）

如果未设置环境变量，可以通过配置文件控制调试模式：
- **Windows**：`%APPDATA%\EasyKiConverter\EasyKiConverter\config.json`
- **Linux**：`~/.config/EasyKiConverter/EasyKiConverter/config.json`
- **macOS**：`~/Library/Application Support/EasyKiConverter/EasyKiConverter/config.json`

```json
{
  "debugMode": true
}
```

## 优先级顺序

调试模式按以下优先级顺序启用：

1. **环境变量**（最高优先级）
   - `EASYKICONVERTER_DEBUG_MODE` 环境变量

2. **配置文件**
   - config.json 中的 `debugMode` 设置

3. **默认值**
   - `false`（禁用）

## 调试输出位置

启用调试模式后，调试信息将导出到：

```
<输出目录>/debug/<组件ID>/
├── cinfo_raw.json           # 原始组件信息
├── cad_raw.json             # 原始 CAD 数据
├── adv_raw.json             # 高级数据
├── model3d_raw.obj          # 原始 3D 模型 OBJ 数据
├── model3d_raw.step         # 原始 3D 模型 STEP 数据
└── debug_info.json          # 调试摘要和日志
```

## 调试信息包含

- **组件信息**：LCSC 组件 ID、名称、封装、数据手册
- **CAD 数据**：引脚详情、形状定义、图层映射
- **3D 模型数据**：OBJ/STEP 模型文件和元数据
- **计时信息**：每个阶段的持续时间（抓取、处理、写入）
- **错误日志**：详细的错误消息和堆栈跟踪
- **验证状态**：每个阶段的成功/失败状态

## 安全注意事项

调试模式可能会暴露敏感信息：
- 组件详细信息和规格
- 内部文件结构
- 性能指标

**重要**：不要在生产环境中启用调试模式或在共享调试输出文件时启用。

## 故障排除

### 调试模式不工作？

1. **检查环境变量是否设置：**
   ```bash
   echo $EASYKICONVERTER_DEBUG_MODE  # Linux/macOS
   echo %EASYKICONVERTER_DEBUG_MODE%  # Windows CMD
   ```

2. 设置环境变量后**重启应用程序**

3. **检查应用程序日志**中是否有调试模式初始化消息

### 调试文件未创建？

1. 确保输出目录具有写入权限
2. 检查组件导出是否成功
3. 验证调试模式是否实际已启用（检查日志）

## 最佳实践

- **开发**：在开发和测试期间启用调试模式
- **生产**：在生产环境中始终禁用调试模式
- **调试**：在排查特定问题时启用调试模式
- **性能**：在性能关键操作中禁用调试模式
- **安全**：切勿将调试输出文件提交到版本控制

## 示例

### 启用调试模式进行测试（Windows PowerShell）

```powershell
# 设置环境变量
$env:EASYKICONVERTER_DEBUG_MODE="true"

# 运行应用程序
.\build\bin\EasyKiConverter.exe
```

### 启用调试模式进行开发（Linux）

```bash
# 添加到 ~/.bashrc
echo 'export EASYKICONVERTER_DEBUG_MODE=true' >> ~/.bashrc

# 重新加载 shell
source ~/.bashrc

# 运行应用程序
./build/bin/EasyKiConverter
```

### 禁用调试模式

```bash
# 取消设置环境变量（Linux/macOS）
unset EASYKICONVERTER_DEBUG_MODE

# 或设置为 false（Windows CMD）
set EASYKICONVERTER_DEBUG_MODE=false
```

## 相关文档

- [BUILD.md](docs/developer/BUILD.md) - 带有调试选项的构建说明
- [FAQ.md](docs/user/FAQ.md) - 关于调试模式的常见问题
- [FEATURES.md](docs/user/FEATURES.md) - 调试模式功能详情