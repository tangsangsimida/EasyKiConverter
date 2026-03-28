# Flatpak 构建和验证指南

本目录包含 EasyKiConverter 的 Flatpak 打包配置和相关验证工具。

## 文件说明

- `io.github.tangsangsimida.easykiconverter.yml` - 正式版 Flatpak 清单文件（使用远程 git 仓库）
- `io.github.tangsangsimida.easykiconverter.local.yml` - 本地测试版清单文件（使用本地源码）

## 快速验证

### 1. 使用统一的 Flatpak 工具

```bash
# Linux/macOS
python3 tools/python/flatpak_tool.py --check

# Windows
python tools\python\flatpak_tool.py --check
```

这个工具会检查：
- Flatpak 构建工具是否已安装（flatpak, flatpak-builder）
- YAML 语法
- 关键配置（App ID、Runtime、Command 等）
- 源码 URL 和版本标签
- 依赖配置（QXlsx）
- 权限配置
- 运行时是否已安装

### 2. 完整构建和测试

```bash
# 检查环境
python tools/python/flatpak_tool.py --check

# 构建 Flatpak
python tools/python/flatpak_tool.py --build

# 完整流程（检查+构建+安装+运行）
python tools/python/flatpak_tool.py --all --force

# 查看应用信息
python tools/python/flatpak_tool.py --info

# 清理构建目录
python tools/python/flatpak_tool.py --clean
```

**注意**：统一工具支持跨平台（Linux、macOS、Windows），所有产物统一放在 `flatpak_build/` 目录下。

## 手动构建

### 使用远程仓库（正式版）

```bash
# 构建应用（产物放在 flatpak_build/ 目录）
flatpak-builder --force-clean --repo=flatpak_build/repo flatpak_build/build deploy/flatpak/io.github.tangsangsimida.easykiconverter.yml

# 添加本地仓库
flatpak --user remote-add --no-gpg-verify local-repo flatpak_build/repo

# 安装应用
flatpak --user install local-repo io.github.tangsangsimida.easykiconverter

# 运行应用
flatpak run io.github.tangsangsimida.easykiconverter
```

### 使用本地源码（测试版）

```bash
# 构建应用（使用当前工作目录的源码）
flatpak-builder --force-clean --repo=flatpak_build/repo-local flatpak_build/build-local deploy/flatpak/io.github.tangsangsimida.easykiconverter.local.yml

# 添加本地仓库
flatpak --user remote-add --no-gpg-verify local-repo-local flatpak_build/repo-local

# 安装应用
flatpak --user install local-repo-local io.github.tangsangsimida.easykiconverter

# 运行应用
flatpak run io.github.tangsangsimida.easykiconverter
```

## 构建要求

### 系统要求

- Linux 系统
- Flatpak 和 flatpak-builder
- org.kde.Platform 6.10
- org.kde.Sdk 6.10

### 安装依赖

```bash
# 添加 Flathub 仓库
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

# 安装运行时和 SDK
flatpak install flathub org.kde.Platform//6.10
flatpak install flathub org.kde.Sdk//6.10
```

## 清理构建产物

```bash
# 使用工具清理（推荐）
python tools/python/flatpak_tool.py --clean

# 或者手动删除
rm -rf flatpak_build/

# 卸载应用
flatpak remove io.github.tangsangsimida.easykiconverter

# 删除本地仓库
flatpak remote-delete local-repo
```

## 构建选项

### 常用构建选项

```bash
# 强制清理旧的构建目录
--force-clean

# 指定仓库目录
--repo=flatpak_build/repo

# 停留在构建目录（用于调试）
--keep-build-dirs

# 禁用并行构建
--disable-rofiles-fuse

# 显示详细输出
--verbose
```

### 调试构建

```bash
# 停留在构建目录
flatpak-builder --keep-build-dirs --repo=flatpak_build/repo flatpak_build/build deploy/flatpak/io.github.tangsangsimida.easykiconverter.yml

# 进入构建目录
cd flatpak_build/build

# 查看构建日志
cat .flatpak-builder/logs/*.log
```

## 故障排除

### 运行时未安装

```bash
flatpak install flathub org.kde.Platform//6.10
flatpak install flathub org.kde.Sdk//6.10
```

### 构建失败

1. 检查构建日志：`cat /tmp/flatpak-build.log`
2. 确保网络连接正常（需要下载源码和依赖）
3. 检查磁盘空间是否充足（至少需要 5GB）

### QXlsx 下载失败

清单文件中已包含 QXlsx 的 SHA256 校验和，如果下载失败，可以：
1. 手动下载 QXlsx 源码
2. 修改清单文件使用本地源码
3. 检查网络连接

## 权限说明

Flatpak 应用使用沙盒运行，需要以下权限：

- `--share=ipc` - 共享 IPC
- `--socket=fallback-x11` - X11 支持
- `--socket=wayland` - Wayland 支持
- `--device=dri` - GPU 访问（用于图形渲染）
- `--share=network` - 网络访问（用于下载元件数据）

## 发布

### 创建 Flatpak 包

```bash
# 构建应用
flatpak-builder --repo=flatpak_build/repo flatpak_build/build deploy/flatpak/io.github.tangsangsimida.easykiconverter.yml

# 导出单文件包
flatpak build-bundle flatpak_build/repo easykiconverter.flatpak io.github.tangsangsimida.easykiconverter

# 导出运行时包（可选）
flatpak build-bundle --runtime flatpak_build/repo org.kde.Platform.flatpak org.kde.Platform//6.10
```

## 相关链接

- [Flatpak 文档](https://docs.flatpak.org/)
- [Flathub](https://flathub.org/)
- [KDE Runtime](https://flathub.org/apps/details/org.kde.Platform)