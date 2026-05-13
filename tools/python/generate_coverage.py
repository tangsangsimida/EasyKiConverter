#!/usr/bin/env python3
"""
EasyKiConverter 代码覆盖率生成工具
====================================

基于 gcovr (优先) 或 lcov/genhtml 生成代码覆盖率报告。

使用方法:
    python tools/python/generate_coverage.py              # 完整流程: 配置 → 构建 → 测试 → 报告
    python tools/python/generate_coverage.py --report-only  # 仅生成报告 (需已有 .gcda 文件)
    python tools/python/generate_coverage.py --xml          # 额外生成 XML 报告 (CI 消费)
    python tools/python/generate_coverage.py --html-only    # 仅 HTML 报告 (默认)
    python tools/python/generate_coverage.py --xml-only     # 仅 XML 报告

    # 通过构建脚本间接调用
    python tools/python/build_project.py --coverage

输出目录:
    tests/reports/coverage/

依赖:
    gcovr (推荐, pip install gcovr)
    lcov + genhtml (备选, apt install lcov)

平台:
    仅支持 GCC (ENABLE_COVERAGE 使用 --coverage 标志)
"""

import os
import sys
import shutil
import argparse
import subprocess
from functools import lru_cache
from pathlib import Path
from typing import Optional


PROJECT_ROOT = Path(__file__).parent.parent.parent.absolute()
DEFAULT_BUILD_DIR = PROJECT_ROOT / "build_coverage"
REPORT_OUTPUT_DIR = PROJECT_ROOT / "tests" / "reports" / "coverage"

EXCLUDE_PATTERNS = [
    "*/build/*",
    "*/build_coverage/*",
    "*/tests/*",
    "*/googletest/*",
    "*/3rdparty/*",
    "*/third_party/*",
    "*/_deps/*",
    "*/moc_*",
    "*/qrc_*",
    "*ui_*",
    "*_autogen/*",
    "*/CMakeFiles/*",
    "/usr/*",
    "/opt/*",
    "*/Qt/*",
    "*QXlsx/*",
]

HTML_REPORT = REPORT_OUTPUT_DIR / "index.html"
XML_REPORT = REPORT_OUTPUT_DIR / "coverage.xml"


@lru_cache(maxsize=None)
def find_tool(tool_name: str) -> Optional[str]:
    """查找命令行工具 (结果缓存，避免重复 PATH 扫描)"""
    return shutil.which(tool_name)


def get_report_tool() -> Optional[str]:
    """按优先级获取报告工具: gcovr > lcov+genhtml"""
    if find_tool("gcovr"):
        return "gcovr"
    if find_tool("lcov") and find_tool("genhtml"):
        return "lcov"
    return None


def configure_cmake(build_dir: Path, build_type: str = "Debug") -> bool:
    """配置 CMake 开启覆盖率"""
    print("\n" + "=" * 60)
    print("配置 CMake (覆盖率模式)")
    print("=" * 60)

    build_dir.mkdir(parents=True, exist_ok=True)

    cmake_cmd = [
        "cmake",
        str(PROJECT_ROOT),
        f"-DCMAKE_BUILD_TYPE={build_type}",
        "-DENABLE_COVERAGE=ON",
        "-DEASYKICONVERTER_BUILD_TESTS=ON",
    ]

    # 传递 Qt 路径
    for var in ["Qt6_DIR", "QTDIR", "QT_PREFIX_PATH", "Qt6_ROOT", "QT_ROOT", "CMAKE_PREFIX_PATH"]:
        val = os.environ.get(var)
        if val:
            cmake_cmd.append(f"-DCMAKE_PREFIX_PATH={val}")
            print(f"  使用 Qt 路径: {val}")
            break

    print(f"  源目录: {PROJECT_ROOT}")
    print(f"  构建目录: {build_dir}")
    print(f"  命令: {' '.join(cmake_cmd)}")

    try:
        result = subprocess.run(cmake_cmd, capture_output=True, text=True, cwd=build_dir)
        if result.returncode == 0:
            print("  CMake 配置成功")
            if result.stdout:
                for line in result.stdout.strip().splitlines():
                    print(f"    {line}")
            return True
        else:
            print(f"  CMake 配置失败:\n{result.stderr}")
            return False
    except Exception as e:
        print(f"  CMake 配置异常: {e}")
        return False


def build_project(build_dir: Path, jobs: int = 0) -> bool:
    """构建项目"""
    print("\n" + "=" * 60)
    print("构建项目")
    print("=" * 60)

    if jobs <= 0:
        jobs = os.cpu_count() or 4

    build_cmd = ["cmake", "--build", str(build_dir), "--parallel", str(jobs)]
    print(f"  并行度: {jobs}")

    try:
        result = subprocess.run(build_cmd, cwd=build_dir)
        if result.returncode == 0:
            print("  构建成功")
            return True
        else:
            print(f"  构建失败 (退出码: {result.returncode})")
            return False
    except Exception as e:
        print(f"  构建异常: {e}")
        return False


def run_tests(build_dir: Path) -> bool:
    """运行 ctest (输出直接流式显示，保留实时进度)"""
    print("\n" + "=" * 60)
    print("运行测试 (ctest)")
    print("=" * 60)

    env = os.environ.copy()
    env["QT_QPA_PLATFORM"] = "offscreen"

    try:
        result = subprocess.run(
            ["ctest", "--output-on-failure"],
            cwd=build_dir,
            env=env,
        )
        if result.returncode == 0:
            print("\n  所有测试通过")
            return True
        else:
            print(f"\n  部分测试失败 (退出码: {result.returncode})")
            return False
    except Exception as e:
        print(f"  运行测试异常: {e}")
        return False


def generate_gcovr_report(build_dir: Path, html: bool = True, xml: bool = False) -> bool:
    """使用 gcovr 生成覆盖率报告 (HTML 和 XML 在单次调用中同时生成)"""
    print("\n" + "=" * 60)
    print("生成覆盖率报告 (gcovr)")
    print("=" * 60)

    gcovr = find_tool("gcovr")
    if not gcovr:
        print("  ✗ gcovr 不可用")
        return False

    REPORT_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    cmd = [
        gcovr,
        "--root", str(PROJECT_ROOT),
        "--object-directory", str(build_dir),
        "--print-summary",
    ]

    for pattern in EXCLUDE_PATTERNS:
        cmd.extend(["--exclude", pattern])

    if html:
        cmd.extend(["--html-details", str(HTML_REPORT)])
        print(f"  生成 HTML 报告: {HTML_REPORT}")
    if xml:
        cmd.extend(["--xml", "-o", str(XML_REPORT)])
        print(f"  生成 XML 报告: {XML_REPORT}")

    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            if html:
                print(f"  HTML 报告已生成: {HTML_REPORT}")
            if xml:
                print(f"  XML 报告已生成: {XML_REPORT}")
            if result.stdout:
                for line in result.stdout.strip().splitlines():
                    print(f"    {line}")
            return True
        else:
            print(f"  gcovr 执行失败: {result.stderr}")
            return False
    except Exception as e:
        print(f"  gcovr 异常: {e}")
        return False


def generate_lcov_report(build_dir: Path, html: bool = True, xml: bool = False) -> bool:
    """使用 lcov + genhtml 生成覆盖率报告"""
    print("\n" + "=" * 60)
    print("生成覆盖率报告 (lcov + genhtml)")
    print("=" * 60)

    if xml:
        print("  ✗ lcov 不支持直接生成 XML 报告，请安装 gcovr 以获取 XML 输出")
        return False

    lcov = find_tool("lcov")
    genhtml = find_tool("genhtml")
    if not lcov or not genhtml:
        print("  ✗ lcov/genhtml 不可用")
        return False

    REPORT_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    info_file = build_dir / "coverage.info"
    filtered_file = build_dir / "coverage_filtered.info"

    success = True

    # 1. 采集覆盖率数据
    print("  采集覆盖率数据...")
    capture_cmd = [
        lcov,
        "--capture",
        "--directory", str(build_dir),
        "--output-file", str(info_file),
        "--rc", "geninfo_auto_base=1",
    ]
    try:
        result = subprocess.run(capture_cmd, capture_output=True, text=True, cwd=build_dir)
        if result.returncode != 0:
            print(f"  lcov 采集失败: {result.stderr}")
            return False
        print("  采集完成")
    except Exception as e:
        print(f"  lcov 采集异常: {e}")
        return False

    # 2. 过滤排除目录
    print("  过滤排除目录...")
    filter_cmd = [
        lcov,
        "--remove", str(info_file),
        *EXCLUDE_PATTERNS,
        "--output-file", str(filtered_file),
    ]

    try:
        result = subprocess.run(filter_cmd, capture_output=True, text=True, cwd=build_dir)
        if result.returncode == 0:
            print("  过滤完成")
        else:
            error_msg = result.stderr if result.stderr else result.stdout
            print(f"  lcov 过滤失败: {error_msg[:500]}")
            success = False
    except Exception as e:
        print(f"  lcov 过滤异常: {e}")
        success = False

    if not success:
        return False

    # 3. 生成 HTML 报告
    if html:
        print(f"  生成 HTML 报告: {REPORT_OUTPUT_DIR}")
        genhtml_cmd = [
            genhtml,
            str(filtered_file),
            "--output-directory", str(REPORT_OUTPUT_DIR),
            "--title", "EasyKiConverter Coverage",
            "--legend",
            "--num-spaces", "2",
        ]
        try:
            result = subprocess.run(genhtml_cmd, capture_output=True, text=True, cwd=build_dir)
            if result.returncode == 0:
                print(f"  HTML 报告已生成: {REPORT_OUTPUT_DIR / 'index.html'}")
            else:
                print(f"  genhtml 失败: {result.stderr}")
                success = False
        except Exception as e:
            print(f"  genhtml 异常: {e}")
            success = False

    # 清理中间文件
    for f in [info_file, filtered_file]:
        try:
            f.unlink(missing_ok=True)
        except Exception:
            pass

    return success


def generate_report(build_dir: Path, tool: str, html: bool = True, xml: bool = False) -> bool:
    """根据检测到的工具生成覆盖率报告"""
    if tool == "gcovr":
        return generate_gcovr_report(build_dir, html=html, xml=xml)
    elif tool == "lcov":
        return generate_lcov_report(build_dir, html=html, xml=xml)
    else:
        return False


def main() -> int:
    parser = argparse.ArgumentParser(
        description="EasyKiConverter 代码覆盖率生成工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python tools/python/generate_coverage.py              # 完整流程
  python tools/python/generate_coverage.py --report-only  # 仅生成报告
  python tools/python/generate_coverage.py --xml          # 同时生成 XML
  python tools/python/build_project.py --coverage         # 通过构建脚本调用
        """,
    )
    parser.add_argument(
        "--report-only",
        action="store_true",
        help="仅生成报告 (需已有 .gcda 文件，跳过配置/构建/测试)",
    )
    parser.add_argument(
        "--xml",
        action="store_true",
        help="额外生成 XML 报告 (默认仅 HTML)",
    )
    parser.add_argument(
        "--html-only",
        action="store_true",
        help="仅生成 HTML 报告 (默认)",
    )
    parser.add_argument(
        "--xml-only",
        action="store_true",
        help="仅生成 XML 报告",
    )
    parser.add_argument(
        "-b", "--build-dir",
        type=str,
        default=str(DEFAULT_BUILD_DIR),
        help=f"构建目录 (默认: {DEFAULT_BUILD_DIR})",
    )
    parser.add_argument(
        "-j", "--jobs",
        type=int,
        default=0,
        help="并行任务数 (默认: CPU 核心数)",
    )

    args = parser.parse_args()
    build_dir = Path(args.build_dir)

    # 确定报告格式
    if args.html_only and args.xml_only:
        parser.error("--html-only 不能与 --xml-only 同时使用")
    if args.html_only and args.xml:
        parser.error("--html-only 不能与 --xml 同时使用")

    if args.xml_only:
        html_output = False
        xml_output = True
    elif args.html_only:
        html_output = True
        xml_output = False
    else:
        html_output = True
        xml_output = args.xml

    # 检查报告工具
    print("检查报告工具...")
    tool = get_report_tool()

    if not tool:
        print("\n" + "=" * 60)
        print("错误: 未找到覆盖率报告工具")
        print("=" * 60)
        print()
        print("请安装以下任一工具:")
        print()
        print("  推荐 (gcovr):")
        print("    pip install gcovr")
        print()
        print("  备选 (lcov + genhtml):")
        print("    sudo apt install lcov           # Ubuntu/Debian")
        print("    sudo dnf install lcov           # Fedora")
        print("    brew install lcov               # macOS")
        print()
        if xml_output:
            print("  注意: XML 报告仅 gcovr 支持")
        return 1

    print(f"使用工具: {tool}")

    if tool == "lcov" and xml_output:
        print("\n错误: XML 报告需要 gcovr，当前仅检测到 lcov/genhtml")
        print("请安装 gcovr: pip install gcovr")
        return 1

    if not args.report_only:
        # 完整流程
        if not configure_cmake(build_dir):
            return 1

        if not build_project(build_dir, args.jobs):
            return 1

        if not run_tests(build_dir):
            print("\n  警告: 部分测试失败，覆盖率报告仍将生成")

    # 生成报告
    if not generate_report(build_dir, tool, html=html_output, xml=xml_output):
        print("\n  覆盖率报告生成失败")
        return 1

    print("\n" + "=" * 60)
    print("覆盖率报告生成完成")
    print(f"  输出目录: {REPORT_OUTPUT_DIR}")
    if html_output:
        print(f"  HTML 报告: {HTML_REPORT}")
    if xml_output:
        print(f"  XML 报告:  {XML_REPORT}")
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
