#!/usr/bin/env python3
"""
EasyKiConverter Flatpak 构建管理工具
=====================================

统一管理 Flatpak 清单验证、构建、安装、运行和清理。

使用方法:
    python tools/python/flatpak_tool.py --check        # 快速检查清单文件
    python tools/python/flatpak_tool.py --build       # 构建 Flatpak
    python tools/python/flatpak_tool.py --install     # 安装构建的应用
    python tools/python/flatpak_tool.py --run         # 运行应用测试
    python tools/python/flatpak_tool.py --clean       # 清理构建目录
    python tools/python/flatpak_tool.py --all          # 执行完整流程（检查+构建+安装+运行）

示例:
    $ python tools/python/flatpak_tool.py --check
    $ python tools/python/flatpak_tool.py --build --force
    $ python tools/python/flatpak_tool.py --all --force
"""

import os
import sys
import yaml
import subprocess
import shutil
import argparse
import logging
from pathlib import Path
from typing import Optional, Tuple, List
from datetime import datetime


class Colors:
    RESET = "\033[0m"
    RED = "\033[91m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    CYAN = "\033[96m"
    BOLD = "\033[1m"

    @classmethod
    def supports_color(cls) -> bool:
        if sys.platform == "win32":
            try:
                import ctypes

                kernel32 = ctypes.windll.kernel32
                kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
                return True
            except:
                return False
        return hasattr(sys.stdout, "isatty") and sys.stdout.isatty()


def colorize(text: str, color: str) -> str:
    if Colors.supports_color():
        return f"{color}{text}{Colors.RESET}"
    return text


class FlatpakTool:
    def __init__(self, verbose: bool = False, log_file: Optional[Path] = None):
        self.project_root = Path(__file__).parent.parent.parent
        self.manifest_path = (
            self.project_root
            / "deploy"
            / "flatpak"
            / "io.github.tangsangsimida.easykiconverter.yml"
        )
        self.build_dir = self.project_root / "flatpak_build" / "build"
        self.repo_dir = self.project_root / "flatpak_build" / "repo"
        self.app_id = "io.github.tangsangsimida.easykiconverter"
        self.log_file = (
            log_file
            or self.project_root
            / "flatpak_build"
            / "logs"
            / f"flatpak_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log"
        )
        self.verbose = verbose
        self.manifest: dict = {}
        self.required_commands = ["flatpak", "flatpak-builder"]

        self._ensure_build_dirs()
        self._setup_logging()

    def _ensure_build_dirs(self) -> None:
        flatpak_build_dir = self.project_root / "flatpak_build"
        flatpak_build_dir.mkdir(parents=True, exist_ok=True)
        self.build_dir.mkdir(parents=True, exist_ok=True)
        self.log_file.parent.mkdir(parents=True, exist_ok=True)

    def _setup_logging(self) -> None:
        self.logger = logging.getLogger("FlatpakTool")
        level = logging.DEBUG if self.verbose else logging.INFO
        self.logger.setLevel(level)

        self.logger.handlers.clear()

        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setLevel(level)
        formatter = logging.Formatter("%(message)s")
        console_handler.setFormatter(formatter)
        self.logger.addHandler(console_handler)

        if self.log_file.parent != Path("/dev"):
            self.log_file.parent.mkdir(parents=True, exist_ok=True)
            file_handler = logging.FileHandler(self.log_file, encoding="utf-8")
            file_handler.setLevel(logging.DEBUG)
            file_formatter = logging.Formatter(
                "%(asctime)s [%(levelname)s] %(message)s", datefmt="%Y-%m-%d %H:%M:%S"
            )
            file_handler.setFormatter(file_formatter)
            self.logger.addHandler(file_handler)

    def _banner(self, text: str) -> None:
        self.logger.info("")
        self.logger.info("=" * 60)
        self.logger.info(colorize(f"  {text}", Colors.CYAN + Colors.BOLD))
        self.logger.info("=" * 60)

    def _step(self, num: int, desc: str) -> None:
        self.logger.info("")
        self.logger.info(f"{colorize('[步骤 %d]' % num, Colors.BOLD)} {desc}")

    def _ok(self, text: str) -> None:
        self.logger.info(colorize(f"[OK] {text}", Colors.GREEN))

    def _warn(self, text: str) -> None:
        self.logger.warning(colorize(f"[WARNING] {text}", Colors.YELLOW))

    def _error(self, text: str) -> None:
        self.logger.error(colorize(f"[ERROR] {text}", Colors.RED))

    def _info(self, text: str) -> None:
        self.logger.info(colorize(f"[INFO] {text}", Colors.BLUE))

    def _run(
        self, cmd: str, capture: bool = True, show: bool = False
    ) -> Tuple[bool, str, str]:
        try:
            result = subprocess.run(
                cmd, shell=True, capture_output=capture, text=True, check=False
            )
            if show and result.stdout:
                self.logger.info(result.stdout)
            if result.stderr:
                self.logger.debug(result.stderr)
            return result.returncode == 0, result.stdout.strip(), result.stderr.strip()
        except Exception as e:
            self._error(f"命令执行失败: {e}")
            return False, "", str(e)

    def _confirm(self, prompt: str, default: bool = False) -> bool:
        default_str = "Y/n" if default else "y/N"
        try:
            response = input(f"{prompt} [{default_str}]: ").strip().lower()
            if not response:
                return default
            return response in ["y", "yes"]
        except (EOFError, KeyboardInterrupt):
            return default

    def check_environment(self) -> bool:
        self._banner("检查 Flatpak 环境")
        self._step(0, "检查构建工具")

        missing = []
        for cmd in self.required_commands:
            success, _, _ = self._run(f"which {cmd}")
            if success:
                version_result = subprocess.run(
                    [cmd, "--version"], capture_output=True, text=True, timeout=10
                )
                version = (
                    version_result.stdout.strip().split("\n")[0]
                    if version_result.returncode == 0
                    else "未知版本"
                )
                self._ok(f"{cmd}: {version}")
            else:
                self._error(f"{cmd}: 未安装")
                missing.append(cmd)

        if missing:
            self._error(f"缺少必要的工具: {', '.join(missing)}")
            self.logger.info("")
            self.logger.info("请运行以下命令安装:")
            for cmd in missing:
                if cmd == "flatpak":
                    self.logger.info("  # Ubuntu/Debian:")
                    self.logger.info("  sudo apt install flatpak")
                    self.logger.info("  # Fedora:")
                    self.logger.info("  sudo dnf install flatpak")
                    self.logger.info("  # Arch:")
                    self.logger.info("  sudo pacman -S flatpak")
                    self.logger.info("")
                    self.logger.info("  # 添加 Flathub 仓库:")
                    self.logger.info(
                        "  flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo"
                    )
                elif cmd == "flatpak-builder":
                    self.logger.info("  # Ubuntu/Debian:")
                    self.logger.info("  sudo apt install flatpak-builder")
                    self.logger.info("  # Fedora:")
                    self.logger.info("  sudo dnf install flatpak-builder")
                    self.logger.info("  # Arch:")
                    self.logger.info("  sudo pacman -S flatpak-builder")
            return False

        self._ok("所有构建工具已安装")
        return True

    def check_manifest(self) -> bool:
        self._banner("Flatpak 清单文件快速验证")
        self.logger.info(f"项目根目录: {self.project_root}")
        self.logger.info(f"清单文件: {self.manifest_path}")
        self.logger.info(f"日志文件: {self.log_file}")

        if not self.manifest_path.exists():
            self._error(f"清单文件不存在: {self.manifest_path}")
            return False
        self._ok("清单文件存在")

        self._step(1, "验证 YAML 语法")
        try:
            with open(self.manifest_path, "r", encoding="utf-8") as f:
                self.manifest = yaml.safe_load(f)
            self._ok("YAML 语法正确")
        except yaml.YAMLError as e:
            self._error(f"YAML 语法错误: {e}")
            return False

        self._step(2, "检查关键配置")
        app_id = self.manifest.get("app-id", "N/A")
        runtime = self.manifest.get("runtime", "N/A")
        runtime_version = self.manifest.get("runtime-version", "N/A")
        command = self.manifest.get("command", "N/A")

        self.logger.info(f"  App ID:         {app_id}")
        self.logger.info(f"  Runtime:        {runtime}")
        self.logger.info(f"  Runtime Version:{runtime_version}")
        self.logger.info(f"  Command:        {command}")

        modules = self.manifest.get("modules", [])
        main_module = modules[0] if modules else {}
        build_system = main_module.get("buildsystem", "N/A")
        self.logger.info(f"  Build System:   {build_system}")

        self._step(3, "检查源码配置")
        sources = main_module.get("sources", [])
        source_url = None
        tag = None
        for source in sources:
            if source.get("type") == "git":
                source_url = source.get("url", "")
                tag = source.get("tag", "")
                break

        if source_url:
            self.logger.info(f"  源码 URL: {source_url}")
            self.logger.info(f"  版本标签: {tag}")
            self._ok("源码配置正确")
        else:
            self._error("未找到源码配置")
            return False

        self._step(4, "检查依赖")
        qxlsx_url = None
        qxlsx_sha256 = None
        for source in sources:
            if source.get("type") == "archive":
                qxlsx_url = source.get("url", "")
                qxlsx_sha256 = source.get("sha256", "")
                break

        if qxlsx_url:
            sha256_short = qxlsx_sha256[:16] + "..." if qxlsx_sha256 else "N/A"
            self.logger.info(f"  QXlsx URL:    {qxlsx_url}")
            self.logger.info(f"  QXlsx SHA256: {sha256_short}")
            self._ok("依赖配置正确")
        else:
            self._error("未找到 QXlsx 依赖配置")
            return False

        self._step(5, "检查权限配置")
        finish_args = self.manifest.get("finish-args", [])
        permissions = " ".join(finish_args)
        self.logger.info(f"  权限: {permissions}")
        self._ok("权限配置正确")

        self._step(6, "检查运行时")
        runtime_full = f"{runtime}//{runtime_version}"
        _, flatpak_list, _ = self._run("flatpak list")

        runtime_installed = False
        if flatpak_list:
            for line in flatpak_list.split("\n"):
                if runtime in line and runtime_version in line:
                    runtime_installed = True
                    break

        if runtime_installed:
            self._ok(f"运行时已安装: {runtime_full}")
        else:
            self._warn(f"运行时未安装: {runtime_full}")
            self.logger.info(f"  提示: flatpak install --system flathub {runtime_full}")

        self._banner("验证结果")
        self._ok("所有检查通过!")
        self.logger.info("")
        self.logger.info("清单文件配置正确，可以执行完整构建")
        return True

    def install_runtime(self) -> bool:
        if not self.manifest:
            try:
                with open(self.manifest_path, "r", encoding="utf-8") as f:
                    self.manifest = yaml.safe_load(f)
            except Exception as e:
                self._error(f"读取清单文件失败: {e}")
                return False

        runtime = self.manifest.get("runtime", "org.kde.Platform")
        runtime_version = self.manifest.get("runtime-version", "6.10")
        runtime_full = f"{runtime}//{runtime_version}"

        self._step(1, "检查 Flatpak 运行时和 SDK")
        self.logger.info(f"检查运行时: {runtime} (版本 {runtime_version})")

        _, flatpak_list, _ = self._run("flatpak list")

        runtime_installed = False
        if flatpak_list:
            for line in flatpak_list.split("\n"):
                if runtime in line and runtime_version in line:
                    runtime_installed = True
                    break

        if runtime_installed:
            self._ok("运行时已安装")
            return True

        self._warn("运行时未安装，正在安装...")
        self.logger.info(f"运行命令: flatpak install --system flathub {runtime_full}")

        self._run(
            "flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo"
        )

        success, stdout, stderr = self._run(
            f"flatpak install --system flathub {runtime_full} -y"
        )
        if not success:
            self._error("运行时安装失败")
            self.logger.info(
                f"提示: 请手动运行: flatpak install --system flathub {runtime_full}"
            )
            self.logger.info(f"错误: {stderr}")
            return False

        self._ok("运行时安装成功")
        return True

    def build(self, force: bool = False) -> bool:
        if not self.check_manifest():
            return False

        if not force and not self._confirm("是否执行完整构建?", default=False):
            self._info("跳过构建步骤")
            return True

        self._step(2, "清理旧的构建目录")
        if self.build_dir.exists():
            shutil.rmtree(self.build_dir)
        if self.repo_dir.exists():
            shutil.rmtree(self.repo_dir)
        self.build_dir.mkdir(parents=True, exist_ok=True)
        self._ok("清理完成")

        self._step(3, "执行构建")
        self.logger.info("这可能需要 10-30 分钟，请耐心等待...")
        self.logger.info(f"日志文件: {self.log_file}")

        build_cmd = f"flatpak-builder --force-clean --repo={self.repo_dir} {self.build_dir} {self.manifest_path}"

        success, stdout, stderr = self._run(build_cmd, capture=True, show=True)

        if success:
            self._ok("Flatpak 构建成功!")
            self.logger.info(f"构建目录: {self.build_dir}")
            self.logger.info(f"仓库目录: {self.repo_dir}")
        else:
            self._error("Flatpak 构建失败")
            self.logger.info(f"请检查构建日志: {self.log_file}")
            return False

        return True

    def install(self, force: bool = False) -> bool:
        if not self.build_dir.exists():
            self._error(f"构建目录不存在，请先运行构建: {self.build_dir}")
            return False

        if not force and not self._confirm("是否安装构建的应用?", default=False):
            self._info("跳过安装步骤")
            return True

        self._step(4, "安装构建的应用")

        _, flatpak_remote_list, _ = self._run("flatpak remote-list")
        if "local-repo" not in (flatpak_remote_list or ""):
            self._run(
                f"flatpak --user remote-add --no-gpg-verify local-repo {self.repo_dir}"
            )
            self._ok("添加本地仓库: local-repo")
        else:
            self._info("本地仓库已存在: local-repo")

        success, stdout, stderr = self._run(
            f"flatpak --user install local-repo {self.app_id} -y"
        )
        if success:
            self._ok("应用安装成功")
        else:
            self._warn("应用安装可能失败（如果已安装）")
            self.logger.info(f"提示: {stderr}")

        return True

    def run(self, force: bool = False) -> bool:
        if not force and not self._confirm("是否运行应用进行测试?", default=False):
            self._info("跳过运行测试")
            return True

        self._step(5, "运行应用测试")
        self.logger.info("启动应用 (按 Ctrl+C 停止)...")

        try:
            subprocess.Popen(["flatpak", "run", self.app_id])
            self._info("应用已启动")
            self.logger.info("请检查应用是否正常运行")
            input("按回车键停止应用...")
            self._ok("应用测试完成")
        except KeyboardInterrupt:
            self._info("用户中断")
        except Exception as e:
            self._error(f"启动应用失败: {e}")
            return False

        return True

    def clean(self) -> bool:
        self._banner("清理 Flatpak 构建目录")
        removed = []

        if self.build_dir.exists():
            shutil.rmtree(self.build_dir)
            removed.append(str(self.build_dir))
            self._ok(f"已删除: {self.build_dir}")

        if self.repo_dir.exists():
            shutil.rmtree(self.repo_dir)
            removed.append(str(self.repo_dir))
            self._ok(f"已删除: {self.repo_dir}")

        if not removed:
            self._info("没有需要清理的目录")
        else:
            self._info("清理完成")

        return True

    def show_info(self) -> bool:
        self._banner("Flatpak 应用信息")
        _, output, _ = self._run(f"flatpak info {self.app_id}")
        if output:
            self.logger.info(output)
        else:
            self._warn(f"应用未安装: {self.app_id}")
        return True


def main():
    parser = argparse.ArgumentParser(
        description="EasyKiConverter Flatpak 构建管理工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python tools/python/flatpak_tool.py --check        # 快速检查清单
  python tools/python/flatpak_tool.py --build        # 构建 Flatpak
  python tools/python/flatpak_tool.py --install      # 安装应用
  python tools/python/flatpak_tool.py --run          # 运行测试
  python tools/python/flatpak_tool.py --clean        # 清理构建
  python tools/python/flatpak_tool.py --all          # 完整流程
  python tools/python/flatpak_tool.py --info         # 显示应用信息
        """,
    )

    parser.add_argument("--check", action="store_true", help="快速检查清单文件")
    parser.add_argument("--build", action="store_true", help="构建 Flatpak")
    parser.add_argument("--install", action="store_true", help="安装构建的应用")
    parser.add_argument("--run", action="store_true", help="运行应用测试")
    parser.add_argument("--clean", action="store_true", help="清理构建目录")
    parser.add_argument("--info", action="store_true", help="显示应用信息")
    parser.add_argument(
        "--all", action="store_true", help="执行完整流程（检查+构建+安装+运行）"
    )

    parser.add_argument("-f", "--force", action="store_true", help="跳过确认提示")
    parser.add_argument("-v", "--verbose", action="store_true", help="详细输出模式")
    parser.add_argument("--log", type=str, help="指定日志文件路径")

    args = parser.parse_args()

    if not any(
        [
            args.check,
            args.build,
            args.install,
            args.run,
            args.clean,
            args.info,
            args.all,
        ]
    ):
        parser.print_help()
        sys.exit(1)

    log_path = Path(args.log) if args.log else None
    tool = FlatpakTool(verbose=args.verbose, log_file=log_path)

    if args.clean:
        success = tool.clean()
        sys.exit(0 if success else 1)

    if args.info:
        success = tool.show_info()
        sys.exit(0 if success else 1)

    if args.all:
        if not tool.check_environment():
            sys.exit(1)
        if not tool.check_manifest():
            sys.exit(1)
        if not tool.install_runtime():
            sys.exit(1)
        if not tool.build(force=args.force):
            sys.exit(1)
        if not tool.install(force=args.force):
            sys.exit(1)
        if not tool.run(force=args.force):
            sys.exit(1)
        sys.exit(0)

    success = True
    if args.check:
        success = tool.check_manifest() and success

    if args.build:
        if not tool.check_environment():
            sys.exit(1)
        if not tool.install_runtime():
            sys.exit(1)
        success = tool.build(force=args.force) and success

    if args.install:
        if not tool.check_environment():
            sys.exit(1)
        success = tool.install(force=args.force) and success

    if args.run:
        if not tool.check_environment():
            sys.exit(1)
        success = tool.run(force=args.force) and success

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    sys.exit(main())
