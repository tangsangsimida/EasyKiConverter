# Flatpak 构建和验证指南

本目录包含 EasyKiConverter 的 Flatpak 打包配置和相关验证工具。

## 文件说明

- `io.github.tangsangsimida.easykiconverter.yml` - 正式版 Flatpak 清单文件（使用远程 git 仓库）
- `io.github.tangsangsimida.easykiconverter.local.yml` - 本地测试版清单文件（使用本地源码）

## 快速验证

### 1. 快速检查清单文件配置

```bash
# Linux/macOS
python3 tools/python/quick_flatpak_check.py

# Windows
python tools\python\quick_flatpak_check.py
```

这个脚本会检查：
- YAML 语法
- 关键配置（App ID、Runtime、Command 等）
- 源码 URL 和版本标签
- 运行时是否已安装
- 依赖配置（QXlsx）
- 权限配置

### 2. 完整构建测试

```bash
# Linux/macOS
python3 tools/python/test_flatpak_build.py

# Windows
python tools\python\test_flatpak_build.py
```

这个脚本会：
1. 验证清单文件语法
2. 检查运行时和 SDK
3. 执行完整构建（10-30 分钟）
4. 询问是否安装应用
5. 询问是否运行应用测试

**注意**：所有验证和构建脚本均使用 Python 实现，确保跨平台兼容性（Linux、macOS、Windows）。

## 手动构建

### 使用远程仓库（正式版）

```bash
# 构建应用
flatpak-builder --force-clean --repo=flatpak-repo build-flatpak deploy/flatpak/io.github.tangsangsimida.easykiconverter.yml

# 添加本地仓库
flatpak --user remote-add --no-gpg-verify local-repo flatpak-repo

# 安装应用
flatpak --user install local-repo io.github.tangsangsimida.easykiconverter

# 运行应用
flatpak run io.github.tangsangsimida.easykiconverter
```

### 使用本地源码（测试版）

```bash
# 构建应用（使用当前工作目录的源码）
flatpak-builder --force-clean --repo=flatpak-repo-local build-flatpak-local deploy/flatpak/io.github.tangsangsimida.easykiconverter.local.yml

# 添加本地仓库
flatpak --user remote-add --no-gpg-verify local-repo-local flatpak-repo-local

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
# 删除构建目录
rm -rf build-flatpak build-flatpak-local

# 删除仓库目录
rm -rf flatpak-repo flatpak-repo-local

# 卸载应用
flatpak remove io.github.tangsangsimida.easykiconverter

# 删除本地仓库
flatpak remote-delete local-repo
flatpak remote-delete local-repo-local
```

## 构建选项

### 常用构建选项

```bash
# 强制清理旧的构建目录
--force-clean

# 指定仓库目录
--repo=flatpak-repo

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
flatpak-builder --keep-build-dirs --repo=flatpak-repo build-flatpak deploy/flatpak/io.github.tangsangsimida.easykiconverter.yml

# 进入构建目录
cd build-flatpak

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
flatpak-builder --repo=flatpak-repo build-flatpak deploy/flatpak/io.github.tangsangsimida.easykiconverter.yml

# 导出单文件包
flatpak build-bundle flatpak-repo easykiconverter.flatpak io.github.tangsangsimida.easykiconverter

# 导出运行时包（可选）
flatpak build-bundle --runtime flatpak-repo org.kde.Platform.flatpak org.kde.Platform//6.10
```

## 相关链接

- [Flatpak 文档](https://docs.flatpak.org/)
- [Flathub](https://flathub.org/)
- [KDE Runtime](https://flathub.org/apps/details/org.kde.Platform)