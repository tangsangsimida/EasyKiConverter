#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
EasyKiConverter 文档构建工具
=========================================================================

工具简介:
    本工具用于构建 EasyKiConverter 项目的所有文档，包括 Doxygen API
    文档和 MkDocs 静态站点，支持并行构建和多种输出格式。

主要功能:
    1. Doxygen API 文档: 生成 C++/QML API 参考文档
    2. MkDocs 站点文档: 构建用户手册、开发者指南等静态站点
    3. 本地预览: 启动本地服务器预览文档
    4. 环境检查: 检查文档构建所需依赖
    5. 并行构建: Doxygen 和 MkDocs 可同时执行
    6. 多种输出: 支持普通文本和 JSON 格式输出

=========================================================================
使用方法:
=========================================================================

基础用法:
    python tools/python/build_docs.py --all          # 构建所有文档
    python tools/python/build_docs.py --doxygen      # 仅生成 Doxygen
    python tools/python/build_docs.py --mkdocs       # 仅构建 MkDocs
    python tools/python/build_docs.py --serve        # 本地预览
    python tools/python/build_docs.py --clean        # 清理生成的文档

输出选项:
    -v, --verbose      详细输出模式
    --json             JSON 格式输出（CI 友好）

=========================================================================
使用示例:
=========================================================================

示例 1: 构建所有文档
    $ python tools/python/build_docs.py --all

示例 2: 详细输出模式
    $ python tools/python/build_docs.py --all --verbose

示例 3: CI 模式（JSON 输出）
    $ python tools/python/build_docs.py --all --json

示例 4: 仅检查环境
    $ python tools/python/build_docs.py --check

示例 5: 本地预览
    $ python tools/python/build_docs.py --serve

示例 6: 清理文档
    $ python tools/python/build_docs.py --clean

=========================================================================
输出目录结构:
=========================================================================

所有生成的文档统一输出到 build/docs/ 目录:

    build/docs/
    ├── api/              # Doxygen API 文档
    │   ├── html/         # HTML 格式 API
    │   ├── latex/        # LaTeX 格式（可选）
    │   └── xml/          # XML 格式（可选）
    ├── site/             # MkDocs 静态站点
    └── logs/             # 构建日志
        └── build.log     # 构建日志文件

=========================================================================
环境要求:
=========================================================================

必需工具:
    - doxygen        API 文档生成
    - graphviz (dot) Doxygen 依赖（用于生成调用图）

可选工具:
    - mkdocs         静态站点构建 (pip install mkdocs-material)

安装指南:
    Windows: choco install doxygen graphviz
    macOS:   brew install doxygen graphviz
    Linux:   sudo apt install doxygen graphviz

=========================================================================
"""

import argparse
import logging
import os
import shutil
import subprocess
import sys
import json
import time
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime
from typing import Dict, List, Optional, Tuple


# ============================================================================
# 日志配置
# ============================================================================


def setup_logging(verbose: bool = False) -> logging.Logger:
    """配置日志系统"""
    level = logging.DEBUG if verbose else logging.INFO
    logger = logging.getLogger("BuildDocs")
    logger.setLevel(level)

    if logger.handlers:
        logger.handlers.clear()

    formatter = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")

    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(level)
    console_handler.setFormatter(formatter)
    logger.addHandler(console_handler)

    return logger


# ============================================================================
# 文档构建器
# ============================================================================


class DocsBuilder:
    """文档构建器"""

    def __init__(
        self, project_root: Path, verbose: bool = False, json_output: bool = False
    ):
        self.project_root = project_root
        self.build_docs_dir = project_root / "build" / "docs"
        self.logs_dir = self.build_docs_dir / "logs"
        self.verbose = verbose
        self.json_output = json_output
        self.logger = setup_logging(verbose)
        self.results: Dict[str, any] = {
            "doxygen": {"success": False, "time": 0, "errors": []},
            "mkdocs": {"success": False, "time": 0, "errors": []},
        }

    def _ensure_dirs(self) -> None:
        """确保目录存在"""
        self.build_docs_dir.mkdir(parents=True, exist_ok=True)
        self.logs_dir.mkdir(parents=True, exist_ok=True)

    def _setup_file_logging(self) -> None:
        """设置文件日志"""
        log_file = self.logs_dir / "build.log"
        file_handler = logging.FileHandler(log_file, encoding="utf-8", mode="w")
        file_handler.setLevel(logging.DEBUG)
        formatter = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")
        file_handler.setFormatter(formatter)
        self.logger.addHandler(file_handler)

    def check_command(self, cmd: str) -> Tuple[bool, Optional[str]]:
        """检查命令是否可用，返回 (可用, 版本信息)"""
        try:
            if cmd.startswith("python -m "):
                parts = cmd.split()
                result = subprocess.run(
                    parts + ["--version"], capture_output=True, text=True, timeout=10
                )
            else:
                result = subprocess.run(
                    [cmd, "--version"], capture_output=True, text=True, timeout=10
                )
            version = result.stdout.splitlines()[0] if result.stdout else "未知版本"
            return True, version
        except subprocess.TimeoutExpired:
            return False, "命令超时"
        except FileNotFoundError:
            return False, None
        except Exception as e:
            return False, str(e)

    def check_prerequisites(self) -> bool:
        """检查前提条件"""
        self.logger.info("检查环境依赖...")

        tools = {
            "doxygen": self.check_command("doxygen"),
            "dot (graphviz)": self.check_command("dot"),
            "mkdocs": self.check_command("python -m mkdocs"),
        }

        missing = []
        for tool, (available, version) in tools.items():
            if tool == "mkdocs" and not available:
                self.logger.warning(f"  ⚠ {tool}: 未安装 (pip install mkdocs-material)")
                missing.append(tool)
            elif available:
                self.logger.info(f"  ✓ {tool}: {version}")
            else:
                self.logger.error(f"  ✗ {tool}: 未找到")
                missing.append(tool)

        if missing:
            self.logger.error(f"\n缺少必需工具: {', '.join(missing)}")
            return False

        self.logger.info("\n环境检查通过")
        return True

    def run_doxygen(self) -> bool:
        """运行 Doxygen 生成 API 文档"""
        self.logger.info("=" * 60)
        self.logger.info("生成 Doxygen API 文档...")

        doxyfile = self.project_root / "Doxyfile"
        if not doxyfile.exists():
            self.logger.error(f"找不到 Doxyfile: {doxyfile}")
            return False

        start_time = time.time()

        try:
            cmd = ["doxygen", str(doxyfile)]
            if self.verbose:
                self.logger.debug(f"执行命令: {' '.join(cmd)}")

            result = subprocess.run(
                cmd,
                cwd=self.project_root,
                capture_output=True,
                text=True,
                timeout=300,
            )

            elapsed = time.time() - start_time
            self.results["doxygen"]["time"] = elapsed

            if result.returncode != 0:
                self.logger.error(f"Doxygen 执行失败 (退出码: {result.returncode})")
                if result.stderr:
                    self.logger.error(f"错误信息: {result.stderr[:500]}")
                self.results["doxygen"]["errors"].append(result.stderr[:500])
                return False

            api_html_dir = self.build_docs_dir / "api" / "html"
            if api_html_dir.exists():
                self.logger.info(f"Doxygen 文档生成成功 ({elapsed:.2f}s)")
                self.logger.info(f"输出目录: {api_html_dir}")
                self.results["doxygen"]["success"] = True
                return True
            else:
                self.logger.error("Doxygen 输出文件未生成")
                return False

        except subprocess.TimeoutExpired:
            self.logger.error("Doxygen 执行超时 (5分钟)")
            return False
        except Exception as e:
            self.logger.error(f"Doxygen 执行异常: {e}")
            return False

    def run_mkdocs_build(self) -> bool:
        """构建 MkDocs 站点"""
        self.logger.info("=" * 60)
        self.logger.info("构建 MkDocs 站点...")

        start_time = time.time()

        try:
            cmd = [sys.executable, "-m", "mkdocs", "build", "--clean"]
            if self.verbose:
                self.logger.debug(f"执行命令: {' '.join(cmd)}")

            result = subprocess.run(
                cmd,
                cwd=self.project_root,
                capture_output=True,
                text=True,
                timeout=180,
            )

            elapsed = time.time() - start_time
            self.results["mkdocs"]["time"] = elapsed

            if result.returncode != 0:
                self.logger.error(f"MkDocs 构建失败 (退出码: {result.returncode})")
                if result.stderr:
                    self.logger.error(f"错误信息: {result.stderr[:500]}")
                self.results["mkdocs"]["errors"].append(result.stderr[:500])
                return False

            site_dir = self.build_docs_dir / "site"
            self.logger.info(f"MkDocs 站点构建成功 ({elapsed:.2f}s)")
            self.logger.info(f"输出目录: {site_dir}")
            self.results["mkdocs"]["success"] = True
            return True

        except subprocess.TimeoutExpired:
            self.logger.error("MkDocs 构建超时 (3分钟)")
            return False
        except Exception as e:
            self.logger.error(f"MkDocs 构建异常: {e}")
            return False

    def run_parallel_build(self) -> Tuple[bool, bool]:
        """并行执行 Doxygen 和 MkDocs 构建"""
        self.logger.info("=" * 60)
        self.logger.info("开始并行构建文档...")
        self.logger.info(f"输出目录: {self.build_docs_dir}")

        start_time = time.time()
        doxygen_success = False
        mkdocs_success = False

        with ThreadPoolExecutor(max_workers=2) as executor:
            doxygen_future = executor.submit(self.run_doxygen)
            mkdocs_future = executor.submit(self.run_mkdocs_build)

            doxygen_success = doxygen_future.result()
            mkdocs_success = mkdocs_future.result()

        total_time = time.time() - start_time

        self.logger.info("=" * 60)
        self.logger.info("并行构建完成")
        self.logger.info(f"总耗时: {total_time:.2f}s")

        return doxygen_success, mkdocs_success

    def serve_mkdocs(self) -> None:
        """启动 MkDocs 本地服务器"""
        self.logger.info("=" * 60)
        self.logger.info("启动 MkDocs 本地服务器...")
        self.logger.info("访问地址: http://127.0.0.1:8000")
        self.logger.info("按 Ctrl+C 停止服务\n")

        try:
            subprocess.run(
                [sys.executable, "-m", "mkdocs", "serve"],
                cwd=self.project_root,
            )
        except KeyboardInterrupt:
            self.logger.info("\n服务器已停止")
        except Exception as e:
            self.logger.error(f"启动服务器失败: {e}")

    def clean_docs(self) -> None:
        """清理生成的文档"""
        self.logger.info("=" * 60)
        self.logger.info("清理文档目录...")

        dirs_to_clean = [
            self.build_docs_dir / "api",
            self.build_docs_dir / "site",
        ]

        for dir_path in dirs_to_clean:
            if dir_path.exists():
                shutil.rmtree(dir_path)
                self.logger.info(f"  删除目录: {dir_path}")

        self.logger.info("清理完成")

    def output_json(self) -> str:
        """输出 JSON 格式结果"""
        output = {
            "success": self.results["doxygen"]["success"]
            and self.results["mkdocs"]["success"],
            "timestamp": datetime.now().isoformat(),
            "output_directory": str(self.build_docs_dir),
            "results": self.results,
        }
        return json.dumps(output, indent=2, ensure_ascii=False)


# ============================================================================
# 主函数
# ============================================================================


def main():
    parser = argparse.ArgumentParser(
        description="EasyKiConverter 文档构建工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--all", action="store_true", help="构建所有文档（Doxygen + MkDocs 并行执行）"
    )
    group.add_argument("--doxygen", action="store_true", help="仅生成 Doxygen API 文档")
    group.add_argument("--mkdocs", action="store_true", help="仅构建 MkDocs 站点")
    group.add_argument("--serve", action="store_true", help="本地预览文档")
    group.add_argument("--clean", action="store_true", help="清理生成的文档")
    group.add_argument("--check", action="store_true", help="检查环境依赖")

    parser.add_argument("-v", "--verbose", action="store_true", help="详细输出模式")
    parser.add_argument("--json", action="store_true", help="JSON 格式输出（CI 模式）")

    args = parser.parse_args()

    project_root = Path(__file__).parent.parent.parent.resolve()
    builder = DocsBuilder(project_root, verbose=args.verbose, json_output=args.json)

    if args.check:
        success = builder.check_prerequisites()
        if args.json:
            print(json.dumps({"success": success, "check": "prerequisites"}, indent=2))
        return 0 if success else 1

    if args.serve:
        builder.check_prerequisites()
        builder.serve_mkdocs()
        return 0

    if args.clean:
        builder.clean_docs()
        return 0

    builder._ensure_dirs()
    builder._setup_file_logging()

    success = True

    if args.all:
        doxygen_ok, mkdocs_ok = builder.run_parallel_build()
        success = doxygen_ok and mkdocs_ok
    elif args.doxygen:
        success = builder.run_doxygen()
    elif args.mkdocs:
        success = builder.run_mkdocs_build()

    if args.json:
        print(builder.output_json())

    if success:
        builder.logger.info("\n" + "=" * 60)
        builder.logger.info("文档构建完成！")
        builder.logger.info(f"输出目录: {builder.build_docs_dir}")
        builder.logger.info("=" * 60)
    else:
        builder.logger.error("\n" + "=" * 60)
        builder.logger.error("文档构建失败")
        builder.logger.error("=" * 60)

    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
