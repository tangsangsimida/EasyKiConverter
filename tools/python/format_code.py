"""
EasyKiConverter 代码格式化工具
============================================================================
说明:
    统一的代码格式化工具，支持 C++ 和 QML 文件

主要功能:
    1. 格式化 C++ 文件
    2. 格式化 QML 文件
    3. --check 模式：仅检查不修改，用于 CI
    4. 统计修改文件列表
    5. 生成格式化报告

环境要求:
    - Python: 3.6+
    - clang-format: 13+ (推荐 18+)
    - qmlformat: Qt 6.6+ (推荐 6.10.1)
============================================================================
"""

import os
import sys
import subprocess
import argparse
import logging
import time
import shutil
from datetime import datetime
from pathlib import Path
from typing import Optional, List, Tuple

# ============================================================================
# ANSI 颜色支持
# ============================================================================

class Colors:
    """ANSI 颜色代码"""
    RESET = '\033[0m'
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    BOLD = '\033[1m'

    @classmethod
    def supports_color(cls) -> bool:
        """检测终端是否支持颜色"""
        # Windows 10+ 支持 ANSI 颜色
        if sys.platform == 'win32':
            try:
                import ctypes
                kernel32 = ctypes.windll.kernel32
                # 启用 ANSI 转义序列
                kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
                return True
            except:
                return False
        return hasattr(sys.stdout, 'isatty') and sys.stdout.isatty()

def colorize(text: str, color: str) -> str:
    """添加颜色"""
    if Colors.supports_color():
        return f"{color}{text}{Colors.RESET}"
    return text

# ============================================================================
# 统计信息
# ============================================================================

class FormatStatistics:
    """格式化统计信息"""
    def __init__(self):
        self.start_time: float = time.time()
        self.total_files: int = 0
        self.modified_files: List[str] = []
        self.unchanged_files: List[str] = []
        self.failed_files: List[Tuple[str, str]] = []  # (file, error_message)

    @property
    def success_count(self) -> int:
        return len(self.modified_files) + len(self.unchanged_files)

    @property
    def elapsed_time(self) -> float:
        return time.time() - self.start_time

    def report(self, logger: logging.Logger, verbose: bool = False) -> None:
        """生成统计报告"""
        logger.info("")
        logger.info("=" * 60)
        logger.info("格式化统计报告")
        logger.info("=" * 60)
        logger.info(f"  总文件数:   {self.total_files}")
        logger.info(f"  已修改:     {colorize(str(len(self.modified_files)), Colors.YELLOW)} 个文件")
        logger.info(f"  无变化:     {colorize(str(len(self.unchanged_files)), Colors.GREEN)} 个文件")

        if self.failed_files:
            logger.info(f"  失败:       {colorize(str(len(self.failed_files)), Colors.RED)} 个文件")

        logger.info(f"  耗时:       {self.elapsed_time:.2f}s")
        logger.info("=" * 60)

        # 显示修改文件列表
        if self.modified_files:
            logger.info("")
            logger.info(colorize("[修改文件列表]", Colors.YELLOW))
            for i, f in enumerate(self.modified_files, 1):
                logger.info(f"  {i}. {f}")

        # 显示失败文件列表
        if self.failed_files:
            logger.info("")
            logger.info(colorize("[失败文件列表]", Colors.RED))
            for f, err in self.failed_files:
                logger.error(f"  {f}: {err}")

        # 详细模式显示无变化文件
        if verbose and self.unchanged_files:
            logger.info("")
            logger.info(colorize("[无变化文件]", Colors.GREEN))
            for f in self.unchanged_files:
                logger.debug(f"  {f}")

# ============================================================================
# 代码格式化器
# ============================================================================

class CodeFormatter:
    def __init__(self, check_mode: bool = False, verbose: bool = False):
        self.project_root = Path(__file__).parent.parent.parent.absolute()
        self.check_mode = check_mode
        self.verbose = verbose
        self.stats = FormatStatistics()
        self.logger = self._setup_logging()

        # 工具版本
        self.clang_format_version = self._get_tool_version("clang-format")
        self.qmlformat_version = self._get_tool_version("qmlformat")

    def _setup_logging(self) -> logging.Logger:
        """配置日志系统"""
        level = logging.DEBUG if self.verbose else logging.INFO
        logger = logging.getLogger("CodeFormatter")
        logger.setLevel(level)

        # 控制台处理器
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setLevel(level)

        # 简洁格式
        formatter = logging.Formatter('%(message)s')
        console_handler.setFormatter(formatter)
        logger.addHandler(console_handler)

        return logger

    def _get_tool_version(self, tool: str) -> Optional[str]:
        """获取工具版本"""
        try:
            result = subprocess.run(
                [tool, "--version"],
                capture_output=True,
                text=True,
                timeout=10
            )
            if result.returncode == 0:
                # 提取版本号
                output = result.stdout.strip()
                if tool == "clang-format":
                    # clang-format version 21.1.0
                    import re
                    match = re.search(r'version\s+(\d+\.\d+\.\d+)', output)
                    return match.group(1) if match else output.split('\n')[0]
                elif tool == "qmlformat":
                    # qmlformat 6.10.1
                    import re
                    match = re.search(r'(\d+\.\d+\.\d+)', output)
                    return match.group(1) if match else output.split('\n')[0]
                return output.split('\n')[0]
        except Exception:
            pass
        return None

    def _find_files(self, directories: List[str], extensions: List[str]) -> List[Path]:
        """查找文件"""
        files = []
        for directory in directories:
            dir_path = self.project_root / directory
            if not dir_path.exists():
                self.logger.warning(f"目录不存在: {directory}")
                continue

            for ext in extensions:
                files.extend(dir_path.rglob(f"*{ext}"))

        return sorted(set(files))

    def _check_file_needs_formatting(self, file_path: Path, tool: str) -> Tuple[bool, Optional[str]]:
        """检查文件是否需要格式化"""
        try:
            result = subprocess.run(
                [tool, "--dry-run", "--Werror", str(file_path)],
                capture_output=True,
                text=True,
                timeout=30
            )
            # 退出码非 0 表示需要格式化
            return result.returncode != 0, None
        except subprocess.TimeoutExpired:
            return False, "超时"
        except Exception as e:
            return False, str(e)

    def _format_file(self, file_path: Path, tool: str) -> Tuple[bool, bool, Optional[str]]:
        """
        格式化单个文件
        返回: (success, was_modified, error_message)
        """
        try:
            if self.check_mode:
                # 检查模式：仅检查是否需要格式化
                needs_formatting, error = self._check_file_needs_formatting(file_path, tool)
                if error:
                    return False, False, error
                return True, needs_formatting, None
            else:
                # 格式化模式：先读取原内容用于比较
                original_content = file_path.read_text(encoding='utf-8')

                # 执行格式化
                result = subprocess.run(
                    [tool, "-i", str(file_path)],
                    capture_output=True,
                    text=True,
                    timeout=30
                )

                if result.returncode != 0:
                    return False, False, result.stderr

                # 比较是否有变化
                new_content = file_path.read_text(encoding='utf-8')
                was_modified = original_content != new_content

                return True, was_modified, None

        except subprocess.TimeoutExpired:
            return False, False, "格式化超时"
        except UnicodeDecodeError:
            return False, False, "编码错误"
        except Exception as e:
            return False, False, str(e)

    def format_cpp(self, directories: List[str] = None) -> bool:
        """格式化 C++ 文件"""
        if directories is None:
            directories = ["src", "tests"]

        self.logger.info("")
        self.logger.info(colorize("[C++ 格式化]", Colors.CYAN))
        self.logger.info(f"  工具: clang-format {self.clang_format_version or '未找到'}")
        self.logger.info(f"  模式: {'检查' if self.check_mode else '格式化'}")
        self.logger.info(f"  目录: {', '.join(directories)}")

        if not self.clang_format_version:
            self.logger.error("  错误: 未找到 clang-format")
            return False

        # 检查 .clang-format 配置文件
        config_file = self.project_root / ".clang-format"
        if not config_file.exists():
            self.logger.warning("  警告: 未找到 .clang-format 配置文件")

        # 查找文件
        files = self._find_files(directories, [".h", ".cpp"])
        self.logger.info(f"  扫描到 {len(files)} 个文件")
        self.logger.info("")

        if not files:
            self.logger.info("  没有找到 C++ 文件")
            return True

        # 处理文件
        for file_path in files:
            relative_path = file_path.relative_to(self.project_root)
            self.stats.total_files += 1

            success, was_modified, error = self._format_file(file_path, "clang-format")

            if not success:
                self.stats.failed_files.append((str(relative_path), error))
                self.logger.error(f"    ✗ {relative_path} - {error}")
            elif was_modified:
                self.stats.modified_files.append(str(relative_path))
                if self.check_mode:
                    self.logger.warning(f"    ⚡ {relative_path} - 需要格式化")
                else:
                    self.logger.info(f"    ⚡ {relative_path} - 已修改")
            else:
                self.stats.unchanged_files.append(str(relative_path))
                if self.verbose:
                    self.logger.debug(f"    ✓ {relative_path} - 无变化")

        return len(self.stats.failed_files) == 0

    def format_qml(self, directories: List[str] = None) -> bool:
        """格式化 QML 文件"""
        if directories is None:
            directories = ["src/ui/qml"]

        self.logger.info("")
        self.logger.info(colorize("[QML 格式化]", Colors.CYAN))
        self.logger.info(f"  工具: qmlformat {self.qmlformat_version or '未找到'}")
        self.logger.info(f"  模式: {'检查' if self.check_mode else '格式化'}")
        self.logger.info(f"  目录: {', '.join(directories)}")

        if not self.qmlformat_version:
            self.logger.error("  错误: 未找到 qmlformat")
            return False

        # 检查 .qmlformat.json 配置文件
        config_file = self.project_root / ".qmlformat.json"
        if not config_file.exists():
            self.logger.warning("  警告: 未找到 .qmlformat.json 配置文件")

        # 查找文件
        files = self._find_files(directories, [".qml"])
        self.logger.info(f"  扫描到 {len(files)} 个文件")
        self.logger.info("")

        if not files:
            self.logger.info("  没有找到 QML 文件")
            return True

        # 处理文件
        for file_path in files:
            relative_path = file_path.relative_to(self.project_root)
            self.stats.total_files += 1

            # qmlformat 没有 --dry-run，需要手动比较
            try:
                # 查找配置文件
                config_arg = []
                if config_file.exists():
                    config_arg = ["-s", str(config_file)]

                if self.check_mode:
                    # 检查模式：创建临时文件进行比较
                    import tempfile
                    original = file_path.read_text(encoding='utf-8')

                    with tempfile.NamedTemporaryFile(mode='w', suffix='.qml', delete=False, encoding='utf-8') as tmp:
                        tmp.write(original)
                        tmp_path = tmp.name

                    try:
                        result = subprocess.run(
                            ["qmlformat", "-i"] + config_arg + [tmp_path],
                            capture_output=True,
                            text=True,
                            timeout=30
                        )

                        if result.returncode != 0:
                            self.stats.failed_files.append((str(relative_path), result.stderr.strip() or "格式化失败"))
                            self.logger.error(f"    ✗ {relative_path} - {result.stderr.strip() or '格式化失败'}")
                        else:
                            formatted = Path(tmp_path).read_text(encoding='utf-8')
                            if formatted != original:
                                self.stats.modified_files.append(str(relative_path))
                                self.logger.warning(f"    ⚡ {relative_path} - 需要格式化")
                            else:
                                self.stats.unchanged_files.append(str(relative_path))
                                if self.verbose:
                                    self.logger.debug(f"    ✓ {relative_path} - 无变化")
                    finally:
                        Path(tmp_path).unlink(missing_ok=True)
                else:
                    # 格式化模式
                    original = file_path.read_text(encoding='utf-8')

                    # 确保使用配置文件
                    config_arg = ["-s", str(config_file)] if config_file.exists() else []

                    result = subprocess.run(
                        ["qmlformat", "-i"] + config_arg + [str(file_path)],
                        capture_output=True,
                        text=True,
                        timeout=30
                    )

                    if result.returncode != 0:
                        self.stats.failed_files.append((str(relative_path), result.stderr))
                        self.logger.error(f"    ✗ {relative_path} - {result.stderr}")
                    else:
                        new_content = file_path.read_text(encoding='utf-8')
                        was_modified = original != new_content

                        if was_modified:
                            self.stats.modified_files.append(str(relative_path))
                            self.logger.info(f"    ⚡ {relative_path} - 已修改")
                        else:
                            self.stats.unchanged_files.append(str(relative_path))
                            if self.verbose:
                                self.logger.debug(f"    ✓ {relative_path} - 无变化")

            except subprocess.TimeoutExpired:
                self.stats.failed_files.append((str(relative_path), "超时"))
                self.logger.error(f"    ✗ {relative_path} - 超时")
            except Exception as e:
                self.stats.failed_files.append((str(relative_path), str(e)))
                self.logger.error(f"    ✗ {relative_path} - {e}")

        return len(self.stats.failed_files) == 0

    def fix_qml_empty_lines(self, directories: List[str] = None) -> bool:
        """修复 QML 文件中的多余空行"""
        if directories is None:
            directories = ["src/ui/qml"]

        self.logger.info("")
        self.logger.info(colorize("[修复 QML 空行]", Colors.CYAN))
        self.logger.info(f"  目录: {', '.join(directories)}")

        # 查找文件
        files = self._find_files(directories, [".qml"])
        self.logger.info(f"  扫描到 {len(files)} 个文件")
        self.logger.info("")

        if not files:
            self.logger.info("  没有找到 QML 文件")
            return True

        def remove_extra_empty_lines(content: str) -> str:
            """删除多余的空行"""
            lines = content.split('\n')
            result = []

            i = 0
            while i < len(lines):
                line = lines[i]
                stripped = line.strip()

                if stripped == '':
                    # 空行：检查是否需要保留
                    should_keep = False

                    # 检查上一行
                    if i > 0:
                        prev_line = lines[i - 1].strip()
                        if prev_line.endswith('}'):
                            should_keep = True
                        if prev_line.startswith('//') or prev_line.startswith('/*') or prev_line.endswith('*/'):
                            should_keep = True
                        if prev_line.startswith('import '):
                            should_keep = True

                    # 检查下一行
                    if i + 1 < len(lines):
                        next_line = lines[i + 1].strip()
                        if next_line == '':
                            should_keep = False
                        if next_line == '}':
                            should_keep = False

                    if should_keep:
                        result.append(line)
                else:
                    result.append(line)
                i += 1

            # 删除文件末尾的空行
            while result and result[-1].strip() == '':
                result.pop()

            # 确保文件以换行符结尾
            if result:
                result.append('')

            return '\n'.join(result)

        # 处理文件
        for file_path in files:
            relative_path = file_path.relative_to(self.project_root)
            self.stats.total_files += 1

            try:
                content = file_path.read_text(encoding='utf-8')
                original_lines = len(content.split('\n'))

                fixed_content = remove_extra_empty_lines(content)
                fixed_lines = len(fixed_content.split('\n'))

                if fixed_content != content:
                    if not self.check_mode:
                        file_path.write_text(fixed_content, encoding='utf-8')
                    self.stats.modified_files.append(str(relative_path))
                    self.logger.info(f"    ⚡ {relative_path} ({original_lines} → {fixed_lines} 行)")
                else:
                    self.stats.unchanged_files.append(str(relative_path))
                    if self.verbose:
                        self.logger.debug(f"    ✓ {relative_path} - 无变化")

            except Exception as e:
                self.stats.failed_files.append((str(relative_path), str(e)))
                self.logger.error(f"    ✗ {relative_path} - {e}")

        return len(self.stats.failed_files) == 0

    def generate_report(self, output_path: Optional[str] = None) -> None:
        """生成报告文件"""
        if output_path is None:
            log_dir = self.project_root / "logs"
            log_dir.mkdir(parents=True, exist_ok=True)
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            output_path = log_dir / f"format_{timestamp}.log"
        else:
            output_path = Path(output_path)

        try:
            with open(output_path, 'w', encoding='utf-8') as f:
                f.write("=" * 60 + "\n")
                f.write("EasyKiConverter 代码格式化报告\n")
                f.write("=" * 60 + "\n")
                f.write(f"时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"模式: {'检查' if self.check_mode else '格式化'}\n")
                f.write(f"clang-format: {self.clang_format_version or '未找到'}\n")
                f.write(f"qmlformat: {self.qmlformat_version or '未找到'}\n")
                f.write(f"\n统计:\n")
                f.write(f"  总文件数: {self.stats.total_files}\n")
                f.write(f"  已修改: {len(self.stats.modified_files)}\n")
                f.write(f"  无变化: {len(self.stats.unchanged_files)}\n")
                f.write(f"  失败: {len(self.stats.failed_files)}\n")
                f.write(f"  耗时: {self.stats.elapsed_time:.2f}s\n")

                if self.stats.modified_files:
                    f.write(f"\n修改文件列表:\n")
                    for i, file in enumerate(self.stats.modified_files, 1):
                        f.write(f"  {i}. {file}\n")

                if self.stats.failed_files:
                    f.write(f"\n失败文件列表:\n")
                    for file, err in self.stats.failed_files:
                        f.write(f"  {file}: {err}\n")

            self.logger.info(f"\n报告已保存: {output_path}")
        except Exception as e:
            self.logger.error(f"保存报告失败: {e}")

# ============================================================================
# 主函数
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="EasyKiConverter 代码格式化工具",
        epilog="""
示例:
  python tools/python/format_code.py --all           # 格式化所有文件
  python tools/python/format_code.py --cpp           # 仅格式化 C++ 文件
  python tools/python/format_code.py --qml           # 仅格式化 QML 文件
  python tools/python/format_code.py --check --all   # CI 检查模式
  python tools/python/format_code.py --all -v        # 详细模式
  python tools/python/format_code.py --all --report logs/format.log  # 生成报告
  python tools/python/format_code.py --qml --fix-empty-lines  # 修复 QML 空行
        """,
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    # 格式化目标
    target_group = parser.add_mutually_exclusive_group()
    target_group.add_argument("--cpp", action="store_true", help="格式化 C++ 文件")
    target_group.add_argument("--qml", action="store_true", help="格式化 QML 文件")
    target_group.add_argument("--all", action="store_true", help="格式化所有文件 (C++ + QML)")

    # 运行模式
    parser.add_argument("--check", action="store_true",
                        help="检查模式：仅检查不修改，用于 CI")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="显示详细输出")
    parser.add_argument("--fix-empty-lines", action="store_true",
                        help="修复 QML 文件中的多余空行（每行代码后的空行）")

    # 目录配置
    parser.add_argument("--dirs", nargs="+",
                        help="指定格式化目录 (默认: src, tests)")

    # 报告生成
    parser.add_argument("--report", type=str,
                        help="生成报告文件路径")

    args = parser.parse_args()

    # 检查参数：如果没有指定任何操作，显示帮助
    if not (args.cpp or args.qml or args.all or args.fix_empty_lines):
        parser.print_help()
        sys.exit(1)

    # 创建格式化器
    formatter = CodeFormatter(check_mode=args.check, verbose=args.verbose)

    # 显示标题
    formatter.logger.info("")
    formatter.logger.info("=" * 60)
    formatter.logger.info(colorize("  EasyKiConverter 代码格式化工具", Colors.BOLD))
    formatter.logger.info("=" * 60)

    # 执行格式化
    success = True

    if args.fix_empty_lines:
        # 仅修复空行
        if not formatter.fix_qml_empty_lines(args.dirs if args.dirs else ["src/ui/qml"]):
            success = False
    else:
        # 正常格式化
        if args.cpp or args.all:
            if not formatter.format_cpp(args.dirs):
                success = False

        if args.qml or args.all:
            if not formatter.format_qml(args.dirs if args.dirs else ["src/ui/qml"]):
                success = False

    # 生成报告
    if args.report:
        formatter.generate_report(args.report)

    # 显示统计
    formatter.stats.report(formatter.logger, args.verbose)

    # 显示结果
    formatter.logger.info("")
    if formatter.check_mode:
        if formatter.stats.modified_files:
            formatter.logger.error(colorize("✗ 代码格式检查失败！", Colors.RED))
            formatter.logger.info("")
            formatter.logger.info("请运行以下命令格式化代码:")
            formatter.logger.info(colorize("  python tools/python/format_code.py --all", Colors.CYAN))
            sys.exit(1)
        else:
            formatter.logger.info(colorize("✓ 代码格式检查通过！", Colors.GREEN))
            sys.exit(0)
    else:
        if success:
            formatter.logger.info(colorize("✓ 格式化完成！", Colors.GREEN))
            sys.exit(0)
        else:
            formatter.logger.error(colorize("✗ 格式化过程中出现错误！", Colors.RED))
            sys.exit(1)

if __name__ == "__main__":
    main()
