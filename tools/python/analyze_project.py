#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
EasyKiConverter 代码统计分析工具
=========================================================================

工具简介:
    本工具合并了原来的 count_lines.py 和 analyze_lines.py，提供项目代码
    统计和风险分析功能，帮助评估代码规模和复杂度。

主要功能:
    1. 项目规模统计: 按目录统计代码行数，计算 KLoC
    2. 高风险文件分析: 识别代码行数过多可能需要重构的文件
    3. 终端超链接: 支持点击文件路径直接打开（需终端支持）
    4. 多格式输出: 支持表格和 JSON 格式输出

=========================================================================
使用方法:
=========================================================================

基础用法:
    python tools/python/analyze_project.py           # 统计项目规模
    python tools/python/analyze_project.py --analyze # 分析高风险文件

输出选项:
    --json              JSON 格式输出
    --analyze           详细分析模式（显示高风险文件列表）
    --link              启用终端超链接（可点击打开文件）
    --top N             仅显示前 N 个最大文件

分析目录:
    python tools/python/analyze_project.py src       # 分析指定目录
    python tools/python/analyze_project.py --all    # 分析所有代码目录

=========================================================================
使用示例:
=========================================================================

示例 1: 查看项目规模统计
    $ python tools/python/analyze_project.py

示例 2: 详细分析高风险文件
    $ python tools/python/analyze_project.py --analyze

示例 3: 显示代码量前 20 的文件
    $ python tools/python/analyze_project.py --analyze --top 20

示例 4: JSON 格式输出（便于 CI/CD 集成）
    $ python tools/python/analyze_project.py --json

示例 5: 启用超链接点击打开文件
    $ python tools/python/analyze_project.py --analyze --link

示例 6: 分析指定目录
    $ python tools/python/analyze_project.py src --analyze

示例 7: 分析所有代码目录
    $ python tools/python/analyze_project.py --all --analyze

=========================================================================
风险等级说明:
=========================================================================

    🔴 高风险: > 500 行，建议拆分重构
    🟡 中风险: > 300 行，可考虑优化
    🟢 低风险: > 200 行，基本可接受

=========================================================================
"""

import os
import sys
import json
import argparse
from pathlib import Path
from typing import Dict, List, Tuple, Optional


# ============================================================================
# 超链接支持检测
# ============================================================================


def detect_hyperlink_support() -> bool:
    """检测终端是否支持 OSC 8 超链接"""
    term_program = os.environ.get("TERM_PROGRAM", "").lower()
    wt_session = os.environ.get("WT_SESSION")
    vscode_term = os.environ.get("VSCODE_GIT_IPC_HANDLE")
    jetbrains_terminal = os.environ.get("TERMINAL_EMULATOR", "").lower()

    if wt_session:
        return True
    if vscode_term:
        return True
    if term_program in ("iterm.app", "gnome-terminal", "kitty"):
        return True
    if "jetbrains" in jetbrains_terminal:
        return True
    return False


def make_hyperlink(path: str, text: str, base_dir: str) -> str:
    """生成 OSC 8 超链接"""
    abs_path = os.path.abspath(os.path.join(base_dir, path))
    if os.name == "nt":
        file_url = "file:///" + abs_path.replace(os.sep, "/")
    else:
        file_url = "file://" + abs_path

    osc_start = "\033]8;;"
    osc_end = "\033\\"
    return f"{osc_start}{file_url}{osc_end}{text}{osc_start}{osc_end}"


# ============================================================================
# 代码分析核心
# ============================================================================


class ProjectAnalyzer:
    """项目代码分析器"""

    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.stats: Dict[str, Dict] = {}
        self.all_files: List[Tuple[int, str]] = []

    def count_file_lines(self, file_path: Path) -> int:
        """统计文件行数"""
        try:
            with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                return sum(1 for _ in f)
        except Exception:
            return 0

    def scan_directory(
        self, directory: Path, extensions: Tuple[str, ...]
    ) -> List[Tuple[int, str]]:
        """扫描目录获取所有文件和行数"""
        results = []
        for root, dirs, files in os.walk(directory):
            dirs[:] = [
                d
                for d in dirs
                if d
                not in ["build", ".git", ".cache", "vcpkg_installed", ".qt", ".gemini"]
            ]

            for f in files:
                if f.endswith(extensions) or f in extensions:
                    file_path = Path(root) / f
                    lines = self.count_file_lines(file_path)
                    rel_path = str(file_path.relative_to(self.project_root))
                    results.append((lines, rel_path))
        return results

    def analyze_project(self, dirs: List[str], extensions: Tuple[str, ...]) -> Dict:
        """分析项目代码"""
        total_lines = 0
        total_files = 0
        results = {}

        for d in dirs:
            dir_path = self.project_root / d
            if not dir_path.exists():
                continue

            files_data = self.scan_directory(dir_path, extensions)
            dir_lines = sum(lines for lines, _ in files_data)
            dir_files = len(files_data)

            results[d] = {"files": dir_files, "lines": dir_lines}
            total_lines += dir_lines
            total_files += dir_files
            self.all_files.extend(files_data)

        self.stats = results
        return {
            "directories": results,
            "total": {
                "files": total_files,
                "lines": total_lines,
                "kloc": round(total_lines / 1000.0, 2),
            },
        }

    def get_risk_files(self) -> Dict[str, List[Tuple[int, str]]]:
        """按风险等级分类文件"""
        all_sorted = sorted(self.all_files, key=lambda x: -x[0])
        return {
            "high": [(lines, path) for lines, path in all_sorted if lines > 500],
            "medium": [
                (lines, path) for lines, path in all_sorted if 300 < lines <= 500
            ],
            "low": [(lines, path) for lines, path in all_sorted if 200 < lines <= 300],
        }

    def print_summary(
        self, use_hyperlink: bool = False, top: Optional[int] = None
    ) -> None:
        """打印统计摘要"""
        if not self.stats:
            print("没有统计数据")
            return

        print("")
        print("=" * 60)
        print("项目代码规模统计")
        print("=" * 60)

        print(f"{'目录 (Directory)':<30} | {'文件数':<8} | {'行数':<10}")
        print("-" * 55)

        for d in ["src", "tests", "resources/translations"]:
            if d in self.stats:
                r = self.stats[d]
                print(f"{d:<30} | {r['files']:<8} | {r['lines']:<10}")

        total = {
            "files": sum(s["files"] for s in self.stats.values()),
            "lines": sum(s["lines"] for s in self.stats.values()),
        }
        print("-" * 55)
        print(f"{'总计 (TOTAL)':<30} | {total['files']:<8} | {total['lines']:<10}")
        print(f"\n>>> 项目规模: {total['lines'] / 1000:.2f} k 行代码 (KLoC)")

    def print_risk_analysis(
        self, use_hyperlink: bool = False, top: Optional[int] = None
    ) -> None:
        """打印风险分析"""
        risk_files = self.get_risk_files()

        print("")
        print("=" * 60)
        print("高风险文件分析")
        print("=" * 60)

        risk_info = [
            ("high", "🔴 高风险 (>500行)", risk_files["high"]),
            ("medium", "🟡 中风险 (300-500行)", risk_files["medium"]),
            ("low", "🟢 低风险 (200-300行)", risk_files["low"]),
        ]

        total_risk = 0
        for _, label, files in risk_info:
            count = len(files)
            total_risk += count
            print(f"\n{label}: {count} 个文件")

        if total_risk == 0:
            print("\n✅ 没有发现高风险文件，代码规模控制良好！")
            return

        all_sorted = sorted(self.all_files, key=lambda x: -x[0])
        if top:
            all_sorted = all_sorted[:top]

        print(f"\n{'行数':>6}  {'文件路径'}")
        print("-" * 80)

        for lines, path in all_sorted:
            if lines > 200:
                if lines > 500:
                    marker = " 🔴"
                elif lines > 300:
                    marker = " 🟡"
                else:
                    marker = " 🟢"
            else:
                marker = ""

            if use_hyperlink:
                path_display = make_hyperlink(path, path, str(self.project_root))
            else:
                path_display = path

            print(f"{lines:>6}  {path_display}{marker}")

        print(f"\n{'=' * 80}")
        print(f"文件总数: {len(self.all_files)}")
        print(f"高风险 (>500行): {len(risk_files['high'])} 个")
        print(f"中风险 (300-500行): {len(risk_files['medium'])} 个")
        print(f"低风险 (200-300行): {len(risk_files['low'])} 个")
        print(f"总行数: {sum(l for l, _ in self.all_files)}")

    def output_json(self) -> str:
        """输出 JSON 格式"""
        risk_files = self.get_risk_files()

        output = {
            "statistics": {
                "directories": self.stats,
                "total": {
                    "files": sum(s["files"] for s in self.stats.values()),
                    "lines": sum(s["lines"] for s in self.stats.values()),
                    "kloc": round(
                        sum(s["lines"] for s in self.stats.values()) / 1000.0, 2
                    ),
                },
            },
            "risk_analysis": {
                "high_risk": {
                    "threshold": 500,
                    "count": len(risk_files["high"]),
                    "files": [{"lines": l, "path": p} for l, p in risk_files["high"]],
                },
                "medium_risk": {
                    "threshold": 300,
                    "count": len(risk_files["medium"]),
                    "files": [{"lines": l, "path": p} for l, p in risk_files["medium"]],
                },
                "low_risk": {
                    "threshold": 200,
                    "count": len(risk_files["low"]),
                    "files": [{"lines": l, "path": p} for l, p in risk_files["low"]],
                },
            },
        }
        return json.dumps(output, indent=2, ensure_ascii=False)


# ============================================================================
# 主函数
# ============================================================================


def main():
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent.absolute()

    default_dirs = ["src", "tests", "resources/translations"]
    extensions = (".cpp", ".h", ".qml", ".py", ".cmake", "CMakeLists.txt", ".ts")

    parser = argparse.ArgumentParser(
        description="EasyKiConverter 代码统计分析工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("directory", nargs="?", help="要分析的目录（默认: src）")
    parser.add_argument(
        "--all", action="store_true", help="分析所有代码目录 (src, tests, translations)"
    )
    parser.add_argument("--analyze", action="store_true", help="执行详细风险分析")
    parser.add_argument("--json", action="store_true", help="JSON 格式输出")
    parser.add_argument(
        "--link", action="store_true", help="启用终端超链接（可点击打开文件）"
    )
    parser.add_argument("--top", type=int, help="仅显示前 N 个最大文件")

    args = parser.parse_args()

    analyzer = ProjectAnalyzer(project_root)

    if args.all:
        dirs = default_dirs
    elif args.directory:
        dirs = [args.directory]
    else:
        dirs = default_dirs

    analyzer.analyze_project(dirs, extensions)

    if args.json:
        print(analyzer.output_json())
    elif args.analyze:
        if args.link and not detect_hyperlink_support():
            print("⚠ 警告: 当前终端可能不支持 OSC 8 超链接", file=sys.stderr)
            print(
                "  支持的终端: Windows Terminal, VS Code, JetBrains IDE, iTerm2 等\n",
                file=sys.stderr,
            )
        analyzer.print_summary(use_hyperlink=args.link)
        analyzer.print_risk_analysis(use_hyperlink=args.link, top=args.top)
    else:
        analyzer.print_summary()


if __name__ == "__main__":
    main()
