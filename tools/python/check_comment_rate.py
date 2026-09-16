#!/usr/bin/env python3
"""
EasyKiConverter 代码注释覆盖率检查工具。

本工具检查 C++ 可维护性边界，而不是把重复的行尾注释当作质量指标：

1. 函数定义前必须存在 Doxygen 或行注释；
2. 会引入多路分支的 switch 块前必须存在解释其映射目的的注释；
3. 文件中至少有一个文件级说明，避免新增模块缺少整体设计背景。

检查结果称为“逻辑代码注释率”。它统计已经识别的函数和控制流逻辑单元，
阈值默认是 90%，用于约束新增和改动代码中的可维护性说明。

启用 --forbid-external-references 后，还会拒绝源码注释中的外部项目引用，
用于保持注释只描述本项目的功能、约束和实现语义。
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


# 函数定义模式覆盖普通函数、成员函数和带 const/override 的声明实现。
FUNCTION_RE = re.compile(r"^\s*[\w:<>*&]+\s+[\w:~]+(?:::\w+)?\s*\([^;]*\)\s*(?:const)?\s*\{")

# 只统计 switch 这种需要说明映射策略的控制流块，不把循环和普通条件实现
# 当作逐行注释对象，避免检查结果鼓励无意义的重复注释。
CONTROL_RE = re.compile(r"^\s*switch\s*\(")

# 仅匹配明确的外部项目引用表达，避免误报普通的“reference”技术术语。
FORBIDDEN_REFERENCE_RE = re.compile(
    r"(?:lckiconverter|参考\s*(?:其他)?项目|参考\s*(?:其他)?实现|reference\s+implementation)",
    re.IGNORECASE,
)


def has_comment_before(lines: list[str], index: int) -> bool:
    """检查目标代码单元前的连续注释区域，允许空行和注释结束标记。"""
    cursor = index - 1
    while cursor >= 0 and not lines[cursor].strip():
        cursor -= 1
    if cursor < 0:
        return False
    return lines[cursor].lstrip().startswith(("//", "/*", "*", "*/"))


def inspect_file(path: Path) -> tuple[int, int, list[str]]:
    """统计一个文件的逻辑单元和有注释单元，并返回缺失位置。"""
    lines = path.read_text(encoding="utf-8").splitlines()
    total = 0
    documented = 0
    missing: list[str] = []
    for index, line in enumerate(lines):
        if FUNCTION_RE.match(line) or CONTROL_RE.match(line):
            total += 1
            if has_comment_before(lines, index):
                documented += 1
            else:
                missing.append(f"{path}:{index + 1}")
    return total, documented, missing


def find_forbidden_references(path: Path) -> list[str]:
    """查找文件注释中的外部项目引用，并返回带行号的位置。"""
    lines = path.read_text(encoding="utf-8").splitlines()
    locations: list[str] = []
    in_block_comment = False
    for index, line in enumerate(lines):
        comment_text = ""
        cursor = 0
        while cursor < len(line):
            if in_block_comment:
                end = line.find("*/", cursor)
                if end < 0:
                    comment_text += line[cursor:]
                    break
                comment_text += line[cursor:end]
                cursor = end + 2
                in_block_comment = False
                continue
            block_start = line.find("/*", cursor)
            line_start = line.find("//", cursor)
            if line_start >= 0 and (block_start < 0 or line_start < block_start):
                comment_text += line[line_start + 2 :]
                break
            if block_start < 0:
                break
            cursor = block_start + 2
            in_block_comment = True
        if FORBIDDEN_REFERENCE_RE.search(comment_text):
            locations.append(f"{path}:{index + 1}")
    return locations


def main() -> int:
    """解析参数并以 90% 作为默认门槛执行检查。"""
    parser = argparse.ArgumentParser(description="检查 C++ 逻辑代码注释覆盖率")
    parser.add_argument("files", nargs="+", type=Path, help="需要检查的 C++ 文件")
    parser.add_argument("--threshold", type=float, default=90.0, help="最低注释覆盖率，默认 90")
    parser.add_argument(
        "--forbid-external-references",
        action="store_true",
        help="拒绝源码注释中的外部项目引用",
    )
    args = parser.parse_args()

    total = 0
    documented = 0
    missing: list[str] = []
    forbidden_references: list[str] = []
    for path in args.files:
        file_total, file_documented, file_missing = inspect_file(path)
        total += file_total
        documented += file_documented
        missing.extend(file_missing)
        if args.forbid_external_references:
            forbidden_references.extend(find_forbidden_references(path))

    rate = 100.0 if total == 0 else documented * 100.0 / total
    print(f"逻辑代码注释率: {rate:.1f}% ({documented}/{total})，门槛: {args.threshold:.1f}%")
    if missing:
        print("缺少注释的逻辑单元:")
        for location in missing:
            print(f"- {location}")
    if forbidden_references:
        print("检测到外部项目引用注释:")
        for location in forbidden_references:
            print(f"- {location}")
    if rate + 1e-9 < args.threshold:
        return 1
    if forbidden_references:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
