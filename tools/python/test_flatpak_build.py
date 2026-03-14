#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Flatpak 本地构建和测试脚本
用于验证 io.github.tangsangsimida.easykiconverter.yml
"""

import os
import sys
import yaml
import subprocess
import shutil
from pathlib import Path

# 检测终端是否支持颜色
def supports_color():
    """检测终端是否支持颜色"""
    # 检查是否在终端中运行
    if not hasattr(sys.stdout, 'isatty'):
        return False
    if not sys.stdout.isatty():
        return False

    # 检查环境变量
    if os.getenv('NO_COLOR'):
        return False
    if os.getenv('TERM') in ('dumb', 'emacs'):
        return False

    # 检测 Windows
    if sys.platform == 'win32':
        try:
            import ctypes
            kernel32 = ctypes.windll.kernel32
            # 启用 ANSI 色彩
            kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
            return True
        except:
            return False

    return True

# 颜色代码
COLORS = {
    'header': '\033[1;36m',    # 青色加粗
    'success': '\033[1;32m',   # 绿色加粗
    'warning': '\033[1;33m',   # 黄色加粗
    'error': '\033[1;31m',     # 红色加粗
    'info': '\033[1;34m',      # 蓝色加粗
    'reset': '\033[0m',        # 重置
}

# 启用颜色支持
USE_COLOR = supports_color()

def colorize(text, color_name):
    """给文本添加颜色"""
    if USE_COLOR and color_name in COLORS:
        return f"{COLORS[color_name]}{text}{COLORS['reset']}"
    return text


def print_header(text):
    """打印标题"""
    print("=" * 42)
    print(colorize(text, 'header'))
    print("=" * 42)
    print()


def print_step(step_num, description):
    """打印步骤信息"""
    print(f"步骤 {step_num}: {description}")
    print()


def success(text):
    """打印成功信息"""
    print(colorize(f"[OK] {text}", 'success'))


def warning(text):
    """打印警告信息"""
    print(colorize(f"[WARNING] {text}", 'warning'))


def error(text):
    """打印错误信息"""
    print(colorize(f"[ERROR] {text}", 'error'))


def info(text):
    """打印信息"""
    print(colorize(f"[INFO] {text}", 'info'))


def run_command(cmd, capture_output=True, show_output=False):
    """运行命令并返回结果"""
    try:
        if capture_output:
            result = subprocess.run(
                cmd,
                shell=True,
                capture_output=True,
                text=True,
                check=False
            )
            if show_output and result.stdout:
                print(result.stdout)
            if result.stderr:
                print(result.stderr, file=sys.stderr)
            return result.returncode == 0, result.stdout.strip(), result.stderr.strip()
        else:
            result = subprocess.run(cmd, shell=True, check=False)
            return result.returncode == 0, "", ""
    except Exception as e:
        error(f"命令执行失败: {e}")
        return False, "", str(e)


def ask_yes_no(prompt, default=False):
    """询问用户是否继续"""
    default_str = "Y/n" if default else "y/N"
    try:
        response = input(f"{prompt} [{default_str}]: ").strip().lower()
        if not response:
            return default
        return response in ['y', 'yes']
    except (EOFError, KeyboardInterrupt):
        return default


def main():
    """主函数"""
    # 获取项目根目录
    script_dir = Path(__file__).parent.parent.parent
    manifest_path = script_dir / "deploy" / "flatpak" / "io.github.tangsangsimida.easykiconverter.yml"
    build_dir = script_dir / "build-flatpak"
    repo_dir = script_dir / "flatpak-repo"
    app_id = "io.github.tangsangsimida.easykiconverter"
    log_file = Path("/tmp/flatpak-build.log")

    print_header("Flatpak 本地构建验证脚本")

    print(f"项目根目录: {script_dir}")
    print(f"Flatpak 清单: {manifest_path}")
    print()

    # 检查清单文件是否存在
    if not manifest_path.exists():
        error("错误: Flatpak 清单文件不存在")
        return 1

    success("找到 Flatpak 清单文件")
    print()

    # 1. 验证 YAML 语法
    print_step(1, "验证 YAML 语法")

    try:
        with open(manifest_path, 'r', encoding='utf-8') as f:
            yaml.safe_load(f)
        success("YAML 语法验证通过")
    except yaml.YAMLError as e:
        error(f"YAML 语法验证失败: {e}")
        return 1
    print()

    # 2. 检查必要的运行时和 SDK
    print_step(2, "检查 Flatpak 运行时和 SDK")

    with open(manifest_path, 'r', encoding='utf-8') as f:
        manifest = yaml.safe_load(f)

    runtime = manifest.get('runtime', 'org.kde.Platform')
    runtime_version = manifest.get('runtime-version', '6.10')
    runtime_full = f"{runtime}//{runtime_version}"

    print(f"检查运行时: {runtime} (版本 {runtime_version})")

    flatpak_list = run_command("flatpak list")[1]

    # 检查运行时是否已安装（检查 runtime 和 version 是否在同一行）
    runtime_installed = False
    if flatpak_list:
        lines = flatpak_list.split('\n')
        for line in lines:
            if runtime in line and runtime_version in line:
                runtime_installed = True
                break

    if runtime_installed:
        success("运行时已安装")
    else:
        warning("运行时未安装，正在安装...")
        print(f"运行命令: flatpak install --system flathub {runtime_full}")

        # 添加 Flathub 仓库
        run_command("flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo")

        # 安装运行时（使用 --system 避免多个 flathub 仓库冲突）
        install_success, stdout, stderr = run_command(f"flatpak install --system flathub {runtime_full} -y")
        if not install_success:
            error("运行时安装失败")
            print(f"提示: 请手动运行: flatpak install --system flathub {runtime_full}")
            print(f"错误: {stderr}")
            return 1
        success("运行时安装成功")
    print()

    # 3. 询问用户是否执行完整构建
    print_step(3, "执行完整构建")
    print("警告: 完整构建可能需要较长时间（10-30分钟）")
    print()

    if not ask_yes_no("是否继续构建?", default=False):
        info("跳过构建步骤")
        info("脚本已完成验证步骤")
        return 0

    # 清理旧的构建目录
    print("清理旧的构建目录...")
    if build_dir.exists():
        shutil.rmtree(build_dir)
    if repo_dir.exists():
        shutil.rmtree(repo_dir)
    build_dir.mkdir(parents=True, exist_ok=True)
    print()

    # 4. 执行构建
    print("开始构建 Flatpak 应用...")
    print("这可能需要 10-30 分钟，请耐心等待...")
    print()

    build_cmd = f"flatpak-builder --force-clean --repo={repo_dir} {build_dir} {manifest_path}"

    build_success, stdout, stderr = run_command(build_cmd, capture_output=True, show_output=True)

    # 保存构建日志
    with open(log_file, 'w', encoding='utf-8') as f:
        f.write(f"Build Command: {build_cmd}\n")
        f.write(f"Exit Code: {0 if build_success else 1}\n")
        f.write("\n=== STDOUT ===\n")
        f.write(stdout)
        f.write("\n=== STDERR ===\n")
        f.write(stderr)

    if build_success:
        print()
        success("Flatpak 构建成功!")
        print(f"构建目录: {build_dir}")
        print(f"仓库目录: {repo_dir}")
        print(f"构建日志: {log_file}")
    else:
        print()
        error("Flatpak 构建失败")
        print(f"请检查构建日志: {log_file}")
        return 1
    print()

    # 5. 安装构建的应用
    print_step(4, "安装构建的应用")

    if not ask_yes_no("是否安装构建的应用?", default=False):
        info("跳过安装步骤")
        print()
        print_header("构建完成!")
        print()
        print("如需安装，请运行:")
        print(f"  flatpak --user remote-add --no-gpg-verify local-repo {repo_dir}")
        print(f"  flatpak --user install local-repo {app_id}")
        print("如需运行，请运行:")
        print(f"  flatpak run {app_id}")
        return 0

    # 添加本地仓库
    flatpak_remote_list = run_command("flatpak remote-list")[1]
    if "local-repo" not in flatpak_remote_list:
        run_command(f"flatpak --user remote-add --no-gpg-verify local-repo {repo_dir}")
        success("添加本地仓库: local-repo")
    else:
        info("本地仓库已存在: local-repo")
    print()

    # 安装应用
    install_app_success, stdout, stderr = run_command(f"flatpak --user install local-repo {app_id} -y")
    if install_app_success:
        success("应用安装成功")
    else:
        warning("应用安装可能失败（如果已安装）")
    print()

    # 6. 运行应用测试
    print_step(5, "运行应用测试")

    if not ask_yes_no("是否运行应用进行测试?", default=False):
        info("跳过运行测试")
        print()
        print_header("构建成功完成!")
        print()
        print("后续操作:")
        print(f"  运行应用: flatpak run {app_id}")
        print(f"  查看信息: flatpak info {app_id}")
        print(f"  卸载应用: flatpak remove {app_id}")
        print(f"  删除本地仓库: flatpak remote-delete local-repo")
        print(f"  清理构建目录: rm -rf {build_dir} {repo_dir}")
        return 0

    # 运行应用
    print("启动应用...")
    print("提示: 应用将在后台运行，请按 Ctrl+C 停止")
    print()

    try:
        subprocess.Popen(["flatpak", "run", app_id])
        info(f"应用已启动")
        print()
        print("请检查应用是否正常运行")
        print()
        input("按回车键停止应用...")
        success("应用测试完成")
    except KeyboardInterrupt:
        print()
        info("用户中断")
    except Exception as e:
        error(f"启动应用失败: {e}")
    print()

    # 7. 清理选项
    print_header("构建成功完成!")
    print()
    print("后续操作:")
    print(f"  运行应用: flatpak run {app_id}")
    print(f"  查看信息: flatpak info {app_id}")
    print(f"  卸载应用: flatpak remove {app_id}")
    print(f"  删除本地仓库: flatpak remote-delete local-repo")
    print(f"  清理构建目录: rm -rf {build_dir} {repo_dir}")
    print()

    return 0


if __name__ == "__main__":
    sys.exit(main())