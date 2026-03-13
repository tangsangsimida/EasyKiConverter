#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Flatpak 清单文件快速验证脚本
用于快速检查清单文件配置
"""

import os
import sys
import yaml
import subprocess
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


def run_command(cmd, capture_output=True):
    """运行命令并返回结果"""
    try:
        if capture_output:
            result = subprocess.run(
                cmd,
                shell=True,
                capture_output=True,
                text=True,
                check=True
            )
            return result.stdout.strip()
        else:
            subprocess.run(cmd, shell=True, check=True)
            return ""
    except subprocess.CalledProcessError as e:
        return None


def main():
    """主函数"""
    # 获取项目根目录
    script_dir = Path(__file__).parent.parent.parent
    manifest_path = script_dir / "deploy" / "flatpak" / "io.github.tangsangsimida.easykiconverter.yml"

    print_header("Flatpak 清单文件快速验证")

    print(f"项目根目录: {script_dir}")
    print(f"Flatpak 清单: {manifest_path}")
    print()

    # 1. 检查文件存在
    if not manifest_path.exists():
        error("清单文件不存在")
        return 1
    success("清单文件存在")
    print()

    # 2. 验证 YAML 语法
    print_step(1, "验证 YAML 语法")
    try:
        with open(manifest_path, 'r', encoding='utf-8') as f:
            yaml.safe_load(f)
        success("YAML 语法正确")
    except yaml.YAMLError as e:
        error(f"YAML 语法错误: {e}")
        return 1
    print()

    # 3. 提取并显示关键信息
    print_step(2, "检查关键配置")
    print()

    with open(manifest_path, 'r', encoding='utf-8') as f:
        manifest = yaml.safe_load(f)

    app_id = manifest.get('app-id', 'N/A')
    runtime = manifest.get('runtime', 'N/A')
    runtime_version = manifest.get('runtime-version', 'N/A')
    command = manifest.get('command', 'N/A')
    build_system = manifest.get('modules', [{}])[0].get('buildsystem', 'N/A')

    print(f"  App ID: {app_id}")
    print(f"  Runtime: {runtime}")
    print(f"  Runtime Version: {runtime_version}")
    print(f"  Command: {command}")
    print(f"  Build System: {build_system}")
    print()

    # 4. 检查源码配置
    print_step(3, "检查源码配置")
    print()

    modules = manifest.get('modules', [])
    main_module = modules[0] if modules else {}
    sources = main_module.get('sources', [])

    source_url = None
    tag = None

    for source in sources:
        if source.get('type') == 'git':
            source_url = source.get('url', '')
            tag = source.get('tag', '')
            break

    if source_url:
        print(f"  源码 URL: {source_url}")
        print(f"  版本标签: {tag}")
    else:
        error("未找到源码配置")
        return 1
    print()

    # 5. 检查运行时是否已安装
    print_step(4, "检查运行时是否已安装")
    print()

    runtime_full = f"{runtime}//{runtime_version}"
    flatpak_list = run_command("flatpak list")

    # 检查运行时是否已安装（检查 runtime 和 version 是否在同一行）
    runtime_installed = False
    if flatpak_list:
        lines = flatpak_list.split('\n')
        for line in lines:
            if runtime in line and runtime_version in line:
                runtime_installed = True
                break

    if runtime_installed:
        success(f"运行时已安装: {runtime_full}")
    else:
        warning(f"运行时未安装: {runtime_full}")
        print(f"   提示: flatpak install --system flathub {runtime_full}")
    print()

    # 6. 检查依赖
    print_step(5, "检查依赖")
    print()

    qxlsx_url = None
    qxlsx_sha256 = None

    for source in sources:
        if source.get('type') == 'archive':
            qxlsx_url = source.get('url', '')
            qxlsx_sha256 = source.get('sha256', '')
            break

    if qxlsx_url:
        sha256_short = qxlsx_sha256[:16] + "..." if qxlsx_sha256 else "N/A"
        print(f"  QXlsx URL: {qxlsx_url}")
        print(f"  QXlsx SHA256: {sha256_short}")
        success("依赖配置正确")
    else:
        error("未找到 QXlsx 依赖配置")
        return 1
    print()

    # 7. 检查权限配置
    print_step(6, "检查权限配置")
    print()

    finish_args = manifest.get('finish-args', [])
    permissions = " ".join(finish_args)
    print(f"  权限: {permissions}")
    print()

    # 8. 总结
    print_header("验证结果")
    print()
    success("所有检查通过!")
    print()
    print("清单文件配置正确，可以执行完整构建")
    print()
    print("执行完整构建:")
    print("  python tools/python/test_flatpak_build.py")
    print()
    print("或直接构建:")
    print(f"  flatpak-builder --force-clean --repo=flatpak-repo build-flatpak {manifest_path}")
    print()

    return 0


if __name__ == "__main__":
    sys.exit(main())