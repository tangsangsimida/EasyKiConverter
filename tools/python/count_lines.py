#!/usr/bin/env python3
import os
import sys

"""
EasyKiConverter 代码行数统计工具 (Project Scale Counter)

用途:
    提供项目规模的高层级统计，以 KLoC (千行代码) 为单位输出结果。
    该工具递归遍历 src, tests 和 translations 目录，并按目录汇总统计结果。

环境要求:
    - Python 3.6+
    - 无需外部依赖库

用法:
    在项目根目录下执行:
    python tools/python/count_lines.py

作者: DennisReYoonjiho
日期: 2026-02-15
"""

def count_lines(directory, extensions):
    total_lines = 0
    file_count = 0

    for root, dirs, files in os.walk(directory):
        # 排除常见的非源码目录
        if any(ignored in root for ignored in ['build', '.git', '.cache', 'vcpkg_installed', '.qt', '.gemini']):
            continue

        for file in files:
            if any(file.endswith(ext) for ext in extensions):
                file_path = os.path.join(root, file)
                try:
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                        lines = f.readlines()
                        total_lines += len(lines)
                        file_count += 1
                except Exception as e:
                    print(f"读取错误 {file_path}: {e}")

    return total_lines, file_count

def main():
    # 获取项目根目录 (脚本在 tools/python/)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(script_dir, "../../"))

    # 配置
    search_dirs = ["src", "tests", "resources/translations"]
    extensions = [".cpp", ".h", ".qml", ".py", ".cmake", "CMakeLists.txt", ".ts"]

    total_lines = 0
    total_files = 0

    # 打印表头 (注意：中文在终端占2个字符位，这里微调对齐)
    print(f"\n{'目录 (Directory)':<25} | {'文件数':<8} | {'行数':<10}")
    print("-" * 50)

    for d in search_dirs:
        dir_path = os.path.join(project_root, d)
        if os.path.exists(dir_path):
            lines, files = count_lines(dir_path, extensions)
            print(f"{d:<25} | {files:<8} | {lines:<10}")
            total_lines += lines
            total_files += files

    # 计算 KLoC
    total_k = total_lines / 1000.0

    print("-" * 50)
    print(f"{'总计 (TOTAL)':<25} | {total_files:<8} | {total_lines:<10}")
    print(f"\n>>> 项目规模: {total_k:.2f} k 行代码 (KLoC)")
    print(">>> 注: 此统计包含指定目录下的递归统计, 已排除构建产物.\n")

if __name__ == "__main__":
    main()
