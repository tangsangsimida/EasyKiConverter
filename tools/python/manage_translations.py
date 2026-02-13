#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
EasyKiConverter 翻译管理工具 (Translation Management Tool)

功能说明:
    1. 自动提取 (lupdate): 扫描项目中的 C++ 和 QML 文件，更新 .ts 翻译源文件。
    2. 编译翻译 (lrelease): 将 .ts 文件编译为二进制 .qm 文件，供程序运行时加载。
    3. 工具链管理: 优先从系统环境变量中查找 Qt 工具，支持自定义路径变量。

使用示例:
    # 更新所有翻译源文件 (.ts)
    python tools/python/manage_translations.py --update

    # 编译所有翻译文件 (.qm)
    python tools/python/manage_translations.py --release

    # 执行全流程 (更新 + 编译)
    python tools/python/manage_translations.py --all

    # 检查工具链状态
    python tools/python/manage_translations.py --check

环境要求:
    - Python 3.6+
    - Qt 6 开发环境 (需包含 lupdate 和 lrelease 命令行工具)
"""

import os
import sys
import subprocess
import shutil
import argparse
from pathlib import Path

# ==============================================================================
# 工具链路径配置 (Toolchain Path Configuration)
# ==============================================================================
# 如果您的 Qt 工具不在系统 PATH 中，请在此处设置您的 Qt 路径 (例如: C:/Qt/6.6.1/mingw_64/bin)
QT_BIN_PATH = ""

# 工具名称定义
LUPDATE_EXE = "lupdate.exe" if sys.platform == "win32" else "lupdate"
LRELEASE_EXE = "lrelease.exe" if sys.platform == "win32" else "lrelease"

# ==============================================================================

def find_tool(tool_name, custom_path=None):
    """
    寻找工具路径：
    1. 尝试从系统环境变量 PATH 中查找。
    2. 如果失败，尝试从 custom_path 中查找。
    3. 如果都失败，返回 None。
    """
    # 1. 检查环境变量
    path = shutil.which(tool_name)
    if path:
        return path

    # 2. 检查自定义路径
    if custom_path:
        full_path = os.path.join(custom_path, tool_name)
        if os.path.exists(full_path):
            return full_path

    return None

def get_project_root():
    """获取项目根目录"""
    return Path(__file__).parent.parent.parent.absolute()

def run_command(cmd, cwd):
    """执行命令并打印输出"""
    print(f"执行命令: {' '.join(cmd)}")
    try:
        result = subprocess.run(cmd, cwd=cwd, check=True, capture_output=True, text=True)
        if result.stdout:
            print(result.stdout)
        return True
    except subprocess.CalledProcessError as e:
        print(f"错误: 命令执行失败 (退出代码: {e.returncode})")
        print(f"标准输出: {e.stdout}")
        print(f"标准错误: {e.stderr}")
        return False
    except Exception as e:
        print(f"发生异常: {str(e)}")
        return False

def check_tools(lupdate_path, lrelease_path):
    """检查工具链是否可用"""
    all_ok = True
    print("-" * 60)
    print("工具链检查:")

    if lupdate_path:
        print(f"[OK] lupdate: {lupdate_path}")
    else:
        print(f"[错误] 未找到 {LUPDATE_EXE}")
        all_ok = False

    if lrelease_path:
        print(f"[OK] lrelease: {lrelease_path}")
    else:
        print(f"[错误] 未找到 {LRELEASE_EXE}")
        all_ok = False

    if not all_ok:
        print("\n提示: 请确保 Qt 工具链已安装并将 bin 目录添加到系统环境变量 PATH 中。")
        print("或者在脚本 tools/python/manage_translations.py 的 QT_BIN_PATH 变量中手动指定路径。")

    print("-" * 60)
    return all_ok

def main():
    parser = argparse.ArgumentParser(description="EasyKiConverter 翻译管理工具")
    parser.add_argument("--update", action="store_true", help="提取翻译字符串到 .ts 文件")
    parser.add_argument("--release", action="store_true", help="编译 .ts 文件为 .qm 文件")
    parser.add_argument("--all", action="store_true", help="执行提取和编译全流程")
    parser.add_argument("--check", action="store_true", help="检查工具链状态")

    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(1)

    args = parser.parse_args()

    # 1. 初始化路径
    project_root = get_project_root()
    lupdate_path = find_tool(LUPDATE_EXE, QT_BIN_PATH)
    lrelease_path = find_tool(LRELEASE_EXE, QT_BIN_PATH)

    # 2. 检查工具 (如果指定了 --check 或其他操作)
    if args.check:
        check_tools(lupdate_path, lrelease_path)
        sys.exit(0)

    if not check_tools(lupdate_path, lrelease_path):
        sys.exit(1)

    # 定义翻译资源路径
    ts_files = [
        "resources/translations/translations_easykiconverter_zh_CN.ts",
        "resources/translations/translations_easykiconverter_en.ts"
    ]

    # 获取 QML 文件列表 (参考 CMakeLists.txt)
    # 实际应用中可以使用通配符扫描
    scan_dirs = ["src"]

    success = True

    # 3. 执行更新 (lupdate)
    if args.update or args.all:
        print("\n>>> 开始提取翻译字符串 (lupdate)...")
        for ts in ts_files:
            cmd = [lupdate_path, "-recursive"] + scan_dirs + ["-ts", ts]
            if not run_command(cmd, project_root):
                success = False
                break
        if success:
            print("lupdate 完成。")

    # 4. 执行编译 (lrelease)
    if success and (args.release or args.all):
        print("\n>>> 开始编译翻译文件 (lrelease)...")
        for ts in ts_files:
            cmd = [lrelease_path, ts]
            if not run_command(cmd, project_root):
                success = False
                break
        if success:
            print("lrelease 完成。")

    if success:
        print("\n所有操作成功完成！")
    else:
        print("\n操作过程中出现错误，请检查输出。")
        sys.exit(1)

if __name__ == "__main__":
    main()
