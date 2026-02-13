#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
工具名称：编码转换工具 (EasyKiConverter Encoding Fixer)
功能描述：
    此工具用于递归扫描项目目录，将指定类型的文件（如 .cpp, .h, .cmake 等）从各种编码（如 GBK, GB18030 等）
    转换为标准的 UTF-8 编码（不带 BOM）。

使用示例：
    1. 转换当前目录及子目录下所有支持的文件：
       python tools/python/convert_to_utf8.py
    2. 转换特定目录：
       python tools/python/convert_to_utf8.py ./src
    3. 仅转换特定类型的文件：
       python tools/python/convert_to_utf8.py --ext .cpp .h
    4. 执行预览（不实际修改文件）：
       python tools/python/convert_to_utf8.py --dry-run
    5. 指定需要排除的目录：
       python tools/python/convert_to_utf8.py --exclude .git build .qt

主要特点：
    - 自动识别多种常见编码（GBK, GB18030, UTF-8, Latin-1 等）。
    - 统一输出为无 BOM 的 UTF-8 编码，避免跨平台乱码。
    - 包含目录过滤功能（默认跳过 .git, build 等）。
"""

import os
import sys
import argparse

# 尝试导入 charset_normalizer 或 chardet 以获得更好的编码检测能力
try:
    import charset_normalizer as chardet
except ImportError:
    try:
        import chardet
    except ImportError:
        chardet = None

def detect_encoding(file_path):
    """
    检测文件编码。
    返回检测到的编码字符串，如果失败则返回 None。
    """
    try:
        with open(file_path, 'rb') as f:
            raw_data = f.read()
    except Exception as e:
        print(f"[错误] 无法读取文件进行检测: {file_path} - {e}")
        return None

    if chardet:
        # charset_normalizer 返回结果对象，chardet 返回字典
        result = chardet.detect(raw_data)
        if isinstance(result, dict):
            return result['encoding']
        else:
            if hasattr(result, 'encoding'):
                return result.encoding
            if hasattr(result, 'best'):
                return result.best().encoding

    # 如果没有第三方库或检测失败，尝试手动匹配常见编码
    # 优先尝试 GBK 族，因为 UTF-8 文件通常能被识别
    encodings = ['utf-8', 'gb18030', 'gbk', 'utf-16', 'latin-1']
    for enc in encodings:
        try:
            raw_data.decode(enc)
            return enc
        except UnicodeDecodeError:
            continue

    return None

def convert_file_to_utf8(file_path, dry_run=False):
    """
    将单个文件转换为不带 BOM 的 UTF-8 编码。
    """
    encoding = detect_encoding(file_path)

    if not encoding:
        # 手动二次尝试，增强对中文编码的兼容性
        try:
            with open(file_path, 'rb') as f:
                raw_data = f.read()
            for enc in ['gbk', 'gb18030', 'utf-8', 'latin-1']:
                try:
                    raw_data.decode(enc)
                    encoding = enc
                    break
                except UnicodeDecodeError:
                    continue
        except Exception:
            pass

    if not encoding:
        print(f"[错误] 无法检测到文件编码: {file_path}")
        return False

    # 检测是否已经是 UTF-8 且无 BOM（此处简化逻辑，总是重新写入以确保一致性）
    # 但如果是预览模式，我们只显示待转换的非 UTF-8 文件或根据需求显示

    print(f"[转换] {file_path} ({encoding} -> utf-8)")

    if dry_run:
        return True

    try:
        # 以检测到的编码读取
        with open(file_path, 'r', encoding=encoding) as f:
            content = f.read()

        # 以不带 BOM 的 UTF-8 写入
        # 注意：不要使用 'utf-8-sig'，因为我们要的是无 BOM 的 UTF-8
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    except Exception as e:
        print(f"[错误] 转换失败 {file_path}: {e}")
        return False

def main():
    # 获取脚本所在项目根目录脚本在 tools/python/ 目录下
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(script_dir, "..", ".."))

    parser = argparse.ArgumentParser(description="将代码文件转换为 UTF-8 (无 BOM) 编码工具")
    parser.add_argument("path", nargs="?", default=project_root, help=f"开始扫描的根路径 (默认为项目根目录)")
    parser.add_argument("--ext", nargs="+", default=[".cpp", ".h", ".c", ".hpp", ".qml", ".py", ".txt", ".cmake"],
                        help="包含的文件后缀名")
    parser.add_argument("--dry-run", action="store_true", help="预览待转换文件，不执行实际修改")
    parser.add_argument("--exclude", nargs="+", default=[".git", "build", ".qt", "bin", "lib", "node_modules", ".vscode"],
                        help="要排除的目录名称")

    args = parser.parse_args()

    # 将输入路径转换为绝对路径
    root_path = os.path.abspath(args.path)
    extensions = tuple(args.ext)
    exclude_dirs = set(args.exclude)

    if not os.path.exists(root_path):
        print(f"路径不存在: {root_path}")
        sys.exit(1)

    convert_count = 0
    error_count = 0

    print(f"--- 正在扫描: {os.path.relpath(root_path, project_root)} ---")

    if os.path.isfile(root_path):
        # 处理单文件情况
        if root_path.lower().endswith(extensions) or os.path.basename(root_path) == "CMakeLists.txt":
            # 显示相对于当前工作目录或项目根目录的路径
            display_path = os.path.relpath(root_path, os.getcwd())
            if convert_file_to_utf8(root_path, args.dry_run):
                convert_count += 1
            else:
                error_count += 1
    else:
        # 递归扫描目录
        for root, dirs, files in os.walk(root_path):
            # 过滤排除目录
            dirs[:] = [d for d in dirs if d not in exclude_dirs]

            for file in files:
                # 匹配后缀或 CMakeLists.txt
                if file.lower().endswith(extensions) or file == "CMakeLists.txt":
                    file_path = os.path.join(root, file)
                    # 显示相对于当前工作目录的相对路径
                    display_path = os.path.relpath(file_path, os.getcwd())
                    if convert_file_to_utf8(file_path, args.dry_run):
                        convert_count += 1
                    else:
                        error_count += 1

    print("\n--- 处理汇总 ---")
    print(f"成功处理/校验文件数: {convert_count}")
    print(f"发生错误文件数: {error_count}")
    if args.dry_run:
        print("注意：当前处于预览模式 (Dry Run)，未对文件进行实际修改。")

if __name__ == "__main__":
    main()
