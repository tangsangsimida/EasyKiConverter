#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
EasyKiConverter 环境检查工具 (Environment Dependency Checker)
"""
import argparse
import os
import sys
import shutil
import subprocess
from pathlib import Path

# ==============================================================================
# 工具链自定义路径配置 (可根据本地环境修改)
# ==============================================================================
QT_BIN_PATH = ""       # 例如: C:/Qt/6.6.1/mingw_64/bin
CMAKE_BIN_PATH = ""    # 例如: C:/Program Files/CMake/bin
COMPILER_BIN_PATH = "" # 编译器路径 (如 MinGW/MSVC bin)
# ==============================================================================

def find_tool(tool_name, custom_path=None):
    """查找工具并返回路径"""
    path = shutil.which(tool_name)
    if path:
        return path
    if custom_path:
        full_path = os.path.join(custom_path, tool_name)
        if os.path.exists(full_path):
            return full_path
    return None

def check_command_version(tool_path, version_args=["--version"]):
    """尝试运行工具并获取版本信息"""
    try:
        result = subprocess.run([tool_path] + version_args, capture_output=True, text=True, check=True)
        # 获取第一行输出作为版本简述
        version_info = result.stdout.strip().split('\n')[0]
        return version_info
    except:
        return "无法获取版本信息"

def main():
    parser = argparse.ArgumentParser(
        description="EasyKiConverter 开发环境检查工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
功能说明:
  1. 检查构建依赖: 验证 CMake, Ninja/Make, Git 是否可用
  2. 检查 Qt 环境: 验证 lupdate, lrelease, qmake 是否可用
  3. 工具链路径管理: 优先从系统 PATH 查找，支持自定义路径变量回退

示例:
  python tools/python/check_env.py           # 执行完整环境检查
  python tools/python/check_env.py --qt-only # 仅检查 Qt 开发环境
        """
    )
    parser.add_argument(
        "--qt-only",
        action="store_true",
        help="仅检查 Qt 开发环境"
    )

    args = parser.parse_args()

    print("=" * 60)
    print("EasyKiConverter 开发环境检查")
    print("=" * 60)

    all_dependencies_met = True

    # 1. 基础构建工具检查
    if not args.qt_only:
        print("\n[1/2] 基础构建工具检查:")
        build_tools = {
            "CMake": ("cmake.exe" if sys.platform == "win32" else "cmake", CMAKE_BIN_PATH),
            "Ninja": ("ninja.exe" if sys.platform == "win32" else "ninja", None),
            "Git": ("git.exe" if sys.platform == "win32" else "git", None)
        }

        for name, (exe, custom) in build_tools.items():
            path = find_tool(exe, custom)
            if path:
                version = check_command_version(path)
                print(f"  [OK] {name:8}: {version}")
            else:
                print(f"  [缺失] {name:8} (建议安装并添加到 PATH)")
                all_dependencies_met = False

    # 2. Qt 环境检查
    stage_label = "[Qt]" if args.qt_only else "[2/2]"
    print(f"\n{stage_label} Qt 开发环境检查:")
    qt_tools = {
        "lupdate": ("lupdate.exe" if sys.platform == "win32" else "lupdate", QT_BIN_PATH),
        "lrelease": ("lrelease.exe" if sys.platform == "win32" else "lrelease", QT_BIN_PATH),
        "qmake": ("qmake.exe" if sys.platform == "win32" else "qmake", QT_BIN_PATH)
    }

    for name, (exe, custom) in qt_tools.items():
        path = find_tool(exe, custom)
        if path:
            version = check_command_version(path, ["-v"]) if name == "qmake" else check_command_version(path, ["-version"])
            print(f"  [OK] {name:8}: {version}")
        else:
            print(f"  [缺失] {name:8} (需要 Qt6 开发组件)")
            all_dependencies_met = False

    print("\n" + "=" * 60)
    if all_dependencies_met:
        print("检查通过！您的开发环境已准备就绪。")
    else:
        print("警告: 您的环境中缺少必要工具，请参考上述缺失项进行安装。")
        print("安装后，请确保工具所在目录已添加到系统环境变量 PATH 中。")
    print("=" * 60)

if __name__ == "__main__":
    main()
