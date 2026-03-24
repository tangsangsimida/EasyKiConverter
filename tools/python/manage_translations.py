#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
EasyKiConverter 翻译管理工具 (Translation Management Tool)

功能说明:
    1. 自动提取 (lupdate): 扫描项目中的 C++ 和 QML 文件，更新 .ts 翻译源文件。
    2. 编译翻译 (lrelease): 将 .ts 文件编译为二进制 .qm 文件，供程序运行时加载。
    3. 自动完成翻译: 自动为常见的中文文本提供英文翻译，减少手动工作量。
    4. 检查未完成翻译: 自动检测 .ts 文件中未完成的翻译项，防止发布时遗漏。
    5. 工具链管理: 优先从系统环境变量中查找 Qt 工具，支持自定义路径变量。

使用示例:
    # 更新所有翻译源文件 (.ts)
    python tools/python/manage_translations.py --update

    # 自动完成可识别的未完成翻译
    python tools/python/manage_translations.py --auto-complete

    # 编译所有翻译文件 (.qm)
    python tools/python/manage_translations.py --release

    # 执行全流程 (更新 + 自动完成翻译 + 编译)
    python tools/python/manage_translations.py --all --auto-complete

    # 检查未完成的翻译项
    python tools/python/manage_translations.py --check-unfinished

    # 严格模式编译：发现未完成翻译时终止
    python tools/python/manage_translations.py --release --strict

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
import xml.etree.ElementTree as ET
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

# 翻译字典：中文到英文的映射
TRANSLATION_DICT = {
    # 退出相关
    "关闭程序": "Close Program",
    "退出程序": "Exit Program",
    "取消": "Cancel",
    "确定": "OK",
    "最小化到托盘": "Minimize to Tray",
    "记住我的选择": "Remember my choice",
    "您可以选择最小化到系统托盘以保持后台运行，或者完全退出程序。": 
        "You can choose to minimize to the system tray to keep running in the background, or completely exit the program.",
    
    # 导出确认相关
    "确认退出": "Confirm Exit",
    "转换正在进行中。退出将取消当前转换，已导出的文件会保留。确定要退出吗？": 
        "Conversion is in progress. Exiting will cancel the current conversion, but exported files will be preserved. Are you sure you want to exit?",
    "强制退出": "Force Exit",
    "继续转换": "Continue",
    
    # 组件列表相关
    "已复制 ID": "ID Copied",
    "全部 (%1)": "All (%1)",
    "导出中 (%1)": "Exporting (%1)",
    "成功 (%1)": "Success (%1)",
    "失败 (%1)": "Failed (%1)",
    
    # 导出结果相关
    "3D模型: %1": "3D Model: %1",
    
    # 系统托盘相关
    "显示窗口": "Show Window",
    "退出": "Exit",
    
    # 导出完成相关
    "导出完成": "Export Complete",
    "导出失败：%1 个元器件全部失败": "Export failed: All %1 components failed",
    "成功 %1 个，失败 %2 个": "Success: %1, Failed: %2",
    "输出：符号 %1 · 封装 %2 · 3D %3": "Output: Symbols %1 · Footprints %2 · 3D %3",
    "\n输出：符号 %1 · 封装 %2 · 3D %3": "\nOutput: Symbols %1 · Footprints %2 · 3D %3",
    "成功导出 1 个元器件": "Successfully exported 1 component",
    "成功导出 %1 个元器件": "Successfully exported %1 components",
    "%1 秒": "%1 seconds",
    "%1 分 %2 秒": "%1 min %2 sec",
    "耗时：%1": "Time: %1",
    
    # 应用相关
    "EasyKiConverter - LCSC/EasyEDA 元件转 KiCad 库工具": 
        "EasyKiConverter - LCSC/EasyEDA to KiCad Library Converter",
    "EasyKiConverter - LCSC 转换工具": "EasyKiConverter - LCSC Conversion Tool",
}

def auto_complete_translations(ts_files, project_root, force=False):
    """自动完成未完成的翻译项"""
    print("\n>>> 自动完成未完成的翻译...")
    auto_completed_count = 0
    manual_needed_count = 0

    for ts_file in ts_files:
        # 只处理英文翻译文件
        if 'en.ts' not in ts_file:
            continue

        ts_path = os.path.join(project_root, ts_file)
        if not os.path.exists(ts_path):
            print(f"[警告] 文件不存在: {ts_file}")
            continue

        try:
            tree = ET.parse(ts_path)
            root = tree.getroot()

            modified = False
            for context in root.findall('context'):
                context_name = context.find('name').text
                for message in context.findall('message'):
                    translation = message.find('translation')
                    if translation is not None and translation.get('type') == 'unfinished':
                        source = message.find('source').text
                        
                        # 尝试从字典中查找翻译
                        if source in TRANSLATION_DICT:
                            # 移除 type="unfinished" 属性并设置翻译文本
                            translation.set('type', 'finished')
                            translation.text = TRANSLATION_DICT[source]
                            modified = True
                            auto_completed_count += 1
                            print(f"  [自动翻译] {source[:40]}... -> {TRANSLATION_DICT[source][:40]}...")
                        else:
                            # 无法自动翻译的项
                            if not force:
                                manual_needed_count += 1
                                print(f"  [需要手动翻译] {source[:50]}...")

            if modified:
                # 保存修改后的文件
                tree.write(ts_path, encoding='utf-8', xml_declaration=True)
                print(f"[保存] {ts_file} 已更新")

        except Exception as e:
            print(f"[错误] 处理文件失败 {ts_file}: {str(e)}")

    print(f"\n自动翻译完成: {auto_completed_count} 项")
    if manual_needed_count > 0:
        print(f"需要手动翻译: {manual_needed_count} 项")
        return False
    return True

def check_unfinished_translations(ts_files, project_root):
    """检查未完成的翻译项"""
    print("\n>>> 检查未完成的翻译...")
    has_unfinished = False
    unfinished_counts = {}

    for ts_file in ts_files:
        ts_path = os.path.join(project_root, ts_file)
        if not os.path.exists(ts_path):
            print(f"[警告] 文件不存在: {ts_file}")
            continue

        try:
            tree = ET.parse(ts_path)
            root = tree.getroot()

            unfinished = []
            for context in root.findall('context'):
                context_name = context.find('name').text
                for message in context.findall('message'):
                    translation = message.find('translation')
                    if translation is not None and translation.get('type') == 'unfinished':
                        source = message.find('source').text
                        location = message.find('location')
                        location_info = ""
                        if location is not None:
                            location_info = f" ({location.get('filename')}:{location.get('line')})"
                        unfinished.append({
                            'context': context_name,
                            'source': source,
                            'location': location_info
                        })

            if unfinished:
                has_unfinished = True
                unfinished_counts[ts_file] = len(unfinished)
                print(f"\n[发现未完成翻译] {ts_file} ({len(unfinished)} 项):")
                for item in unfinished[:10]:  # 只显示前10项
                    print(f"  - [{item['context']}] {item['source'][:50]}...{item['location']}")
                if len(unfinished) > 10:
                    print(f"  ... 还有 {len(unfinished) - 10} 项未完成")

        except Exception as e:
            print(f"[错误] 解析文件失败 {ts_file}: {str(e)}")

    if has_unfinished:
        print("\n" + "=" * 60)
        print("[警告] 发现未完成的翻译！")
        print("请使用 Qt Linguist 打开 .ts 文件完成翻译后再发布。")
        print("未完成翻译统计:")
        for ts_file, count in unfinished_counts.items():
            print(f"  - {ts_file}: {count} 项")
        print("=" * 60)
        return False
    else:
        print("[OK] 所有翻译已完成！")
        return True

def main():
    parser = argparse.ArgumentParser(description="EasyKiConverter 翻译管理工具")
    parser.add_argument("--update", action="store_true", help="提取翻译字符串到 .ts 文件")
    parser.add_argument("--release", action="store_true", help="编译 .ts 文件为 .qm 文件")
    parser.add_argument("--all", action="store_true", help="执行提取和编译全流程")
    parser.add_argument("--check", action="store_true", help="检查工具链状态")
    parser.add_argument("--check-unfinished", action="store_true", help="检查未完成的翻译项")
    parser.add_argument("--auto-complete", action="store_true", help="自动完成可识别的未完成翻译")
    parser.add_argument("--strict", action="store_true", help="严格模式：编译时如果发现未完成翻译则终止")

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

    # 3. 自动完成翻译
    if args.auto_complete:
        auto_complete_translations(ts_files, project_root)

    # 4. 检查未完成的翻译
    if args.check_unfinished:
        has_unfinished = not check_unfinished_translations(ts_files, project_root)
        sys.exit(1 if has_unfinished else 0)

    # 5. 执行更新 (lupdate)
    if args.update or args.all:
        print("\n>>> 开始提取翻译字符串 (lupdate)...")
        for ts in ts_files:
            cmd = [lupdate_path, "-recursive"] + scan_dirs + ["-ts", ts]
            if not run_command(cmd, project_root):
                success = False
                break
        if success:
            print("lupdate 完成。")

    # 6. 执行编译 (lrelease) - 严格模式下先检查未完成翻译
    if success and (args.release or args.all):
        if args.strict:
            print("\n>>> 严格模式：检查未完成的翻译...")
            if not check_unfinished_translations(ts_files, project_root):
                print("\n[错误] 发现未完成的翻译，编译终止。请先完成翻译或使用 --release (不带 --strict) 继续。")
                sys.exit(1)

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
