# CLI 模式手动测试清单

本文档用于验证 EasyKiConverter 命令行模式的各项功能，覆盖 BOM 转换、单个元器件转换、批量转换、补全功能、进度显示等场景。

## 适用范围

- Linux / macOS / Windows 命令行环境
- Bash / Zsh / Fish Shell
- 支持 QXlsx 的 Qt 6 环境

## 测试前准备

1. 使用最新代码构建应用：

```bash
python3 tools/python/build_project.py -t Debug -j 4
```

2. 确认测试 BOM 表文件存在：

```bash
ls ./BOM_MP6539GV_Main_2026-02-10.xlsx
```

3. 确认缓存目录存在（用于测试缓存复用）：

```bash
ls ~/.local/share/EasyKiConverter/cache/
```

4. 测试期间如需清理输出，可删除临时目录：

```bash
rm -rf /tmp/cli_test_*
```

## 测试记录模板

- [ ] 测试环境已记录
- [ ] 操作系统已记录：`Linux` / `macOS` / `Windows`
- [ ] Shell 类型已记录：`Bash` / `Zsh` / `Fish`
- [ ] 测试结果已记录：`通过` / `失败`
- [ ] 备注已记录

---

## 执行总览

### 基础功能

- [ ] 用例 1：帮助信息显示
- [ ] 用例 2：版本信息显示
- [ ] 用例 3：BOM 表转换
- [ ] 用例 4：单个元器件转换
- [ ] 用例 5：批量转换

### 导出选项

- [ ] 用例 6：默认导出选项验证
- [ ] 用例 7：禁用 3D 模型导出
- [ ] 用例 8：启用预览图导出
- [ ] 用例 9：调试模式生成报告
- [ ] 用例 10：普通模式不生成报告

### 进度与输出

- [ ] 用例 11：进度条显示
- [ ] 用例 12：安静模式

### 补全功能

- [ ] 用例 13：Bash 补全脚本生成
- [ ] 用例 14：Zsh 补全脚本生成
- [ ] 用例 15：Fish 补全脚本生成
- [ ] 用例 16：动态补全 LCSC ID

### 缓存功能

- [ ] 用例 17：缓存复用验证

### 错误处理

- [ ] 用例 18：无效输入文件处理
- [ ] 用例 19：无效 LCSC ID 处理
- [ ] 用例 20：缺少必需参数处理

---

## 用例 1：帮助信息显示

**目的**：
验证帮助信息正确显示所有可用参数和子命令。

**步骤**：
1. 运行帮助命令：

```bash
./build/bin/easykiconverter --help
```

2. 检查输出内容。

**预期结果**：
- 显示所有全局参数（`--help`, `--version`, `--debug`, `--log-level` 等）
- 显示参数说明文字
- 无错误信息

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 2：版本信息显示

**目的**：
验证版本信息正确显示。

**步骤**：
1. 运行版本命令：

```bash
./build/bin/easykiconverter --version
```

2. 检查输出内容。

**预期结果**：
- 显示版本号（如 `EasyKiConverter 3.1.3`）
- 格式正确，无多余信息

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 3：BOM 表转换

**目的**：
验证 BOM 表转换功能完整可用。

**步骤**：
1. 创建输出目录并运行转换：

```bash
rm -rf /tmp/cli_test_bom
mkdir -p /tmp/cli_test_bom
./build/bin/easykiconverter convert bom -i ./BOM_MP6539GV_Main_2026-02-10.xlsx -o /tmp/cli_test_bom
```

2. 检查输出目录内容：

```bash
ls -la /tmp/cli_test_bom/
```

**预期结果**：
- 显示 "开始转换 BOM 表..."
- 显示找到的元器件数量
- 显示 "转换完成: 成功 X, 失败 Y"
- 生成 `EasyKiConverter.kicad_sym` 符号库文件
- 生成 `EasyKiConverter.pretty/` 封装库目录
- 生成 `EasyKiConverter.3dmodels/` 3D 模型目录

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 4：单个元器件转换

**目的**：
验证单个元器件转换功能。

**步骤**：
1. 运行单个元器件转换：

```bash
rm -rf /tmp/cli_test_single
mkdir -p /tmp/cli_test_single
./build/bin/easykiconverter convert component -c C23186 -o /tmp/cli_test_single
```

2. 检查输出目录内容。

**预期结果**：
- 显示 "开始转换单个元器件..."
- 显示 "元器件编号: C23186"
- 显示 "转换完成: 成功 1, 失败 0"
- 生成符号库、封装库、3D 模型文件

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 5：批量转换

**目的**：
验证批量转换功能。

**步骤**：
1. 创建元器件列表文件：

```bash
echo -e "C23186\nC23166\nC23102" > /tmp/components.txt
```

2. 运行批量转换：

```bash
rm -rf /tmp/cli_test_batch
mkdir -p /tmp/cli_test_batch
./build/bin/easykiconverter convert batch -i /tmp/components.txt -o /tmp/cli_test_batch
```

3. 检查输出目录内容。

**预期结果**：
- 显示 "开始批量转换..."
- 显示 "找到 3 个元器件"
- 显示 "转换完成: 成功 X, 失败 Y"
- 生成所有元器件的符号库、封装库、3D 模型文件

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 6：默认导出选项验证

**目的**：
验证默认导出符号库、封装库、3D 模型（WRL 格式）。

**步骤**：
1. 运行转换并启用调试模式：

```bash
rm -rf /tmp/cli_test_default
mkdir -p /tmp/cli_test_default
./build/bin/easykiconverter convert component -c C23186 -o /tmp/cli_test_default --debug
```

2. 查看详细报告中的导出选项：

```bash
cat /tmp/cli_test_default/easykiconverter_export_detailed_report.md | grep -E "Export (symbol|footprint|3D model)"
```

3. 检查 3D 模型文件格式：

```bash
ls /tmp/cli_test_default/EasyKiConverter.3dmodels/
```

**预期结果**：
- 报告显示：
  - `Export symbol: yes`
  - `Export footprint: yes`
  - `Export 3D model: yes`
- 3D 模型目录只有 `.wrl` 文件，没有 `.step` 文件

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 7：禁用 3D 模型导出

**目的**：
验证可以通过参数禁用 3D 模型导出。

**步骤**：
1. 运行转换并禁用 3D 模型：

```bash
rm -rf /tmp/cli_test_no3d
mkdir -p /tmp/cli_test_no3d
./build/bin/easykiconverter convert component -c C23186 -o /tmp/cli_test_no3d --3d-model false --debug
```

2. 检查输出目录：

```bash
ls /tmp/cli_test_no3d/EasyKiConverter.3dmodels/ 2>/dev/null || echo "目录不存在或为空"
cat /tmp/cli_test_no3d/easykiconverter_export_detailed_report.md | grep "Export 3D model"
```

**预期结果**：
- 报告显示 `Export 3D model: no`
- 3D 模型目录不存在或为空

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 8：启用预览图导出

**目的**：
验证可以通过参数启用预览图导出。

**步骤**：
1. 运行转换并启用预览图：

```bash
rm -rf /tmp/cli_test_preview
mkdir -p /tmp/cli_test_preview
./build/bin/easykiconverter convert component -c C23186 -o /tmp/cli_test_preview --preview --debug
```

2. 检查输出目录：

```bash
ls /tmp/cli_test_preview/EasyKiConverter.preview/
cat /tmp/cli_test_preview/easykiconverter_export_detailed_report.md | grep "Export preview"
```

**预期结果**：
- 报告显示 `Export preview images: yes`
- 生成预览图目录并包含预览图文件

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 9：调试模式生成报告

**目的**：
验证调试模式会生成详细报告文件。

**步骤**：
1. 运行转换并启用调试模式：

```bash
rm -rf /tmp/cli_test_debug
mkdir -p /tmp/cli_test_debug
./build/bin/easykiconverter convert component -c C23186 -o /tmp/cli_test_debug --debug
```

2. 检查报告文件：

```bash
ls -la /tmp/cli_test_debug/easykiconverter_export_detailed_report.md
cat /tmp/cli_test_debug/easykiconverter_export_detailed_report.md | head -30
```

**预期结果**：
- 生成 `easykiconverter_export_detailed_report.md` 文件
- 报告包含导出选项、进度、网络统计等信息

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 10：普通模式不生成报告

**目的**：
验证普通模式不生成详细报告文件。

**步骤**：
1. 运行转换（不启用调试模式）：

```bash
rm -rf /tmp/cli_test_normal
mkdir -p /tmp/cli_test_normal
./build/bin/easykiconverter convert component -c C23186 -o /tmp/cli_test_normal
```

2. 检查报告文件：

```bash
ls /tmp/cli_test_normal/easykiconverter_export_detailed_report.md 2>/dev/null || echo "报告文件不存在（符合预期）"
```

**预期结果**：
- 不生成 `easykiconverter_export_detailed_report.md` 文件

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 11：进度条显示

**目的**：
验证进度条正常显示且不超过 100%。

**步骤**：
1. 运行带进度条的转换：

```bash
rm -rf /tmp/cli_test_progress
mkdir -p /tmp/cli_test_progress
./build/bin/easykiconverter convert bom -i ./BOM_MP6539GV_Main_2026-02-10.xlsx -o /tmp/cli_test_progress --progress 2>&1 | tee /tmp/progress_output.txt
```

2. 检查进度百分比：

```bash
grep -oP '\d+%' /tmp/progress_output.txt | sort -u
```

**预期结果**：
- 进度条从 0% 逐渐增加到 100%
- 所有百分比值都在 0-100 范围内
- 不出现超过 100% 的情况

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 12：安静模式

**目的**：
验证安静模式减少输出。

**步骤**：
1. 运行安静模式转换：

```bash
rm -rf /tmp/cli_test_quiet
mkdir -p /tmp/cli_test_quiet
./build/bin/easykiconverter convert component -c C23186 -o /tmp/cli_test_quiet --quiet 2>&1 | tee /tmp/quiet_output.txt
wc -l /tmp/quiet_output.txt
```

2. 对比普通模式输出行数。

**预期结果**：
- 输出行数明显少于普通模式
- 仍能完成转换任务

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 13：Bash 补全脚本生成

**目的**：
验证 Bash 补全脚本正确生成。

**步骤**：
1. 生成 Bash 补全脚本：

```bash
./build/bin/easykiconverter --completion bash | head -30
```

**预期结果**：
- 输出有效的 Bash 补全脚本
- 包含 `_easykiconverter_complete` 函数
- 包含子命令和参数定义

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 14：Zsh 补全脚本生成

**目的**：
验证 Zsh 补全脚本正确生成。

**步骤**：
1. 生成 Zsh 补全脚本：

```bash
./build/bin/easykiconverter --completion zsh | head -30
```

**预期结果**：
- 输出有效的 Zsh 补全脚本
- 包含 `#compdef easykiconverter` 声明
- 包含 `_easykiconverter` 函数

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 15：Fish 补全脚本生成

**目的**：
验证 Fish 补全脚本正确生成。

**步骤**：
1. 生成 Fish 补全脚本：

```bash
./build/bin/easykiconverter --completion fish | head -30
```

**预期结果**：
- 输出有效的 Fish 补全脚本
- 包含 `complete -c easykiconverter` 命令
- 包含子命令和参数定义

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 16：动态补全 LCSC ID

**目的**：
验证动态补全 LCSC ID 功能。

**步骤**：
1. 输出缓存中的 LCSC ID：

```bash
./build/bin/easykiconverter --complete lcsc-id | head -20
./build/bin/easykiconverter --complete lcsc-id | wc -l
```

**预期结果**：
- 输出缓存中的 LCSC ID 列表
- 每行一个 LCSC ID（如 `C23186`）
- 数量大于 0

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 17：缓存复用验证

**目的**：
验证 CLI 模式使用缓存，第二次转换更快。

**步骤**：
1. 首次转换（可能需要下载）：

```bash
rm -rf /tmp/cli_test_cache1
mkdir -p /tmp/cli_test_cache1
time ./build/bin/easykiconverter convert component -c C23186 -o /tmp/cli_test_cache1
```

2. 第二次转换（应使用缓存）：

```bash
rm -rf /tmp/cli_test_cache2
mkdir -p /tmp/cli_test_cache2
time ./build/bin/easykiconverter convert component -c C23186 -o /tmp/cli_test_cache2
```

3. 对比时间。

**预期结果**：
- 第二次转换速度明显快于第一次
- 日志显示 "Cache hit" 或类似信息
- 两次转换结果一致

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 18：无效输入文件处理

**目的**：
验证无效输入文件的错误处理。

**步骤**：
1. 使用不存在的文件运行转换：

```bash
./build/bin/easykiconverter convert bom -i /nonexistent/file.xlsx -o /tmp/output
echo "退出码: $?"
```

**预期结果**：
- 显示错误信息："输入文件不存在"
- 退出码非 0

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 19：无效 LCSC ID 处理

**目的**：
验证无效 LCSC ID 的错误处理。

**步骤**：
1. 使用无效的 LCSC ID 运行转换：

```bash
rm -rf /tmp/cli_test_invalid
mkdir -p /tmp/cli_test_invalid
./build/bin/easykiconverter convert component -c INVALID_ID -o /tmp/cli_test_invalid
echo "退出码: $?"
```

**预期结果**：
- 显示转换失败信息
- 不会崩溃
- 退出码非 0

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 用例 20：缺少必需参数处理

**目的**：
验证缺少必需参数的错误处理。

**步骤**：
1. 运行转换但不指定输出目录：

```bash
./build/bin/easykiconverter convert bom -i ./BOM_MP6539GV_Main_2026-02-10.xlsx
echo "退出码: $?"
```

**预期结果**：
- 显示错误信息或帮助信息
- 退出码非 0

**登记**：
- [ ] 已执行
- [ ] 通过
- [ ] 失败
- [ ] 已记录备注

---

## 失败时请记录

如任一用例失败，请至少记录以下信息：

- [ ] 测试命令
- [ ] 操作系统：`uname -a`
- [ ] Shell 类型：`echo $SHELL`
- [ ] 完整的控制台输出
- [ ] 退出码：`echo $?`
- [ ] 输出目录内容：`ls -la /tmp/cli_test_*/`

推荐附加信息：

```bash
echo "=== 环境信息 ==="
uname -a
echo $SHELL
./build/bin/easykiconverter --version
echo "=== 缓存状态 ==="
ls -la ~/.local/share/EasyKiConverter/cache/ 2>/dev/null | head -10
```
