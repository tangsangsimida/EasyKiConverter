"""
EasyKiConverter 项目构建管理工具
============================================================================
说明:
    将 tools/windows/build_project.bat 重构为功能更强大的 Python 脚本

主要功能:
    1. 环境检查: 验证 CMake、编译器、Qt6 等依赖 (支持 VS 2019/2022)
    2. CMake 配置: 支持多种构建类型和自定义选项 (POSIX 路径解析)
    3. 项目构建: 并行编译，历史日志轮换 (保留最近 10 次成功日志，自动备份失败日志)
    4. 安装测试: 自动化安装和测试流程，支持清理确认

环境要求:
    - Python: 3.6+
    - CMake: 3.16+
    - Qt6: 6.6+ (推荐 6.10.1)
    - 编译器: MSVC 2019/2022 (Windows), GCC/Clang (Linux/macOS)
============================================================================
"""

import os
import sys
import subprocess
import argparse
import logging
import shutil
import time
import json
import platform
import re
from datetime import datetime
from pathlib import Path, PurePath
from typing import Optional, Dict, Any, List, Tuple

# ============================================================================
# 异常定义
# ============================================================================

class BuildError(Exception):
    """构建错误基类"""
    pass

class EnvironmentError(BuildError):
    """环境错误"""
    pass

class ConfigurationError(BuildError):
    """配置错误"""
    pass

class CompilationError(BuildError):
    """编译错误"""
    pass

# ============================================================================
# 统计信息
# ============================================================================

class BuildStatistics:
    """构建统计信息"""
    def __init__(self):
        self.start_time: float = time.time()
        self.configure_time: float = 0
        self.build_time: float = 0
        self.warnings: int = 0
        self.errors: int = 0

    def report(self, logger: logging.Logger):
        """生成构建统计报告"""
        total_time = time.time() - self.start_time
        logger.info("=" * 60)
        logger.info("构建统计报告")
        logger.info("=" * 60)
        logger.info(f"  总耗时:     {total_time:.2f}s")
        logger.info(f"  配置耗时:   {self.configure_time:.2f}s")
        logger.info(f"  构建耗时:   {self.build_time:.2f}s")
        logger.info(f"  警告数量:   {self.warnings}")
        logger.info(f"  错误数量:   {self.errors}")
        logger.info("=" * 60)

# ============================================================================
# 构建管理器
# ============================================================================

class BuildManager:
    def __init__(self, verbose: bool = False):
        self.project_root = Path(__file__).parent.parent.parent.absolute()
        self.build_dir = self.project_root / "build"
        self.verbose = verbose
        self.config = self._load_config()
        self.stats = BuildStatistics()
        self.logger = self._setup_logging(verbose)

        # 初始 CMake 选项
        self.cmake_options = self.config.get("cmake_options", {})

    def _setup_logging(self, verbose: bool) -> logging.Logger:
        """配置日志系统，含日志轮转边界优化"""
        level = logging.DEBUG if verbose else logging.INFO
        logger = logging.getLogger("BuildManager")
        logger.setLevel(level)

        formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')

        log_dir = self.project_root / "logs"
        if not log_dir.exists():
            log_dir.mkdir(parents=True)

        # 日志轮转：严格保留最近 10 个历史日志
        try:
            log_files = sorted(log_dir.glob("build_*.log"), key=lambda f: f.stat().st_mtime)
            max_logs = 10
            if len(log_files) > max_logs:
                for old_file in log_files[:-max_logs]:
                    old_file.unlink()
        except Exception as e:
            print(f"警告: 清理旧日志失败: {e}")

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        log_filename = f"build_{timestamp}.log"

        file_handler = logging.FileHandler(self.project_root / 'build.log', encoding='utf-8', mode='w')
        file_handler.setFormatter(formatter)
        logger.addHandler(file_handler)

        hist_handler = logging.FileHandler(log_dir / log_filename, encoding='utf-8')
        hist_handler.setFormatter(formatter)
        logger.addHandler(hist_handler)

        stream_handler = logging.StreamHandler(sys.stdout)
        stream_handler.setFormatter(formatter)
        logger.addHandler(stream_handler)

        return logger

    def _load_config(self) -> Dict[str, Any]:
        """加载构建配置"""
        config_file = self.project_root / "tools" / "config" / "build_config.json"
        default_config = {
            "qt_version": "6.10.1",
            "cmake_options": {
                "ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT": True,
                "EASYKICONVERTER_BUILD_TESTS": False
            },
            "build_types": ["Debug", "Release", "RelWithDebInfo", "MinSizeRel"]
        }

        if config_file.exists():
            try:
                with open(config_file, 'r', encoding='utf-8') as f:
                    user_config = json.load(f)
                    if "cmake_options" in user_config:
                        if isinstance(user_config["cmake_options"], dict):
                            default_config["cmake_options"].update(user_config["cmake_options"])
                        user_config.pop("cmake_options")
                    default_config.update(user_config)
            except Exception as e:
                print(f"警告: 加载配置文件失败, 使用默认配置: {e}")

        return default_config

    def _get_cmake_generator(self) -> str:
        """获取最佳 CMake 生成器 (支持 VS 2019/2022)"""
        if shutil.which("ninja"):
            return "Ninja"

        if platform.system() == "Windows":
            gen_info = self._get_msvc_generator()
            if gen_info:
                return gen_info
            if shutil.which("mingw32-make") or shutil.which("make"):
                return "MinGW Makefiles"
            return "NMake Makefiles"

        return "Unix Makefiles"

    def _get_msvc_generator(self) -> Optional[str]:
        """自动定位 MSVC (2022 > 2019)"""
        if platform.system() != "Windows":
            return None

        # 1. 检查 cl 是否已就绪
        if shutil.which("cl"):
            # 简单假设为 2022，实际由环境决定
            return "Visual Studio 17 2022"

        # 2. 检查 vswhere
        pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
        vswhere = Path(pf86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
        if vswhere.exists():
            for ver_year, ver_id in [("2022", "17"), ("2019", "16")]:
                try:
                    res = subprocess.run([
                        str(vswhere), "-latest", "-version", f"[{ver_id}, {int(ver_id)+1})",
                        "-products", "*", "-property", "installationPath"
                    ], capture_output=True, text=True)
                    if res.returncode == 0 and res.stdout.strip():
                        return f"Visual Studio {ver_id} {ver_year}"
                except:
                    pass

        # 3. 检查常见安装路径
        for ver_year, ver_id in [("2022", "17"), ("2019", "16")]:
            for edition in ["Community", "Professional", "Enterprise"]:
                p = Path(fr"C:\Program Files\Microsoft Visual Studio\{ver_year}\{edition}\VC\Tools\MSVC")
                if p.exists():
                    return f"Visual Studio {ver_id} {ver_year}"

        return None

    def _get_qt_prefix_path(self) -> Tuple[bool, Optional[str]]:
        """检测 Qt 并验证版本匹配性"""
        qt_candidates = []
        env_qt = os.environ.get("Qt6_DIR") or os.environ.get("QTDIR")
        if env_qt:
            qt_candidates.append(env_qt)

        if platform.system() == "Windows":
            for base in [r"C:\Qt", r"D:\Qt"]:
                base_path = Path(base)
                if base_path.exists():
                    versions = sorted(
                        [d for d in base_path.iterdir() if d.is_dir() and d.name.startswith("6.")],
                        key=lambda d: d.name, reverse=True
                    )
                    for v in versions:
                        for arch in ["msvc2019_64", "msvc2022_64", "mingw_64"]:
                            qt_candidates.append(str(v / arch))
        else:
            qt_candidates.extend(["/usr/lib/qt6", "/usr/local/Qt-6.10.1"])

        expected_version = str(self.config.get("qt_version", "6.10.1"))

        for qt_path in qt_candidates:
            p = Path(qt_path)
            if (p / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").exists():
                is_matched = expected_version in str(p)
                return is_matched, qt_path

        return False, None

    def check_environment(self) -> bool:
        """检查构建环境"""
        self.logger.info("=" * 60)
        self.logger.info("正在检查构建环境...")
        checks = []

        # 1. CMake
        cmake_path = shutil.which("cmake")
        if cmake_path:
            res = subprocess.run(["cmake", "--version"], capture_output=True, text=True)
            self.logger.debug(f"CMake: {res.stdout.splitlines()[0]}")
            checks.append(("CMake", True))
        else:
            checks.append(("CMake", False))

        # 2. 编译器
        if platform.system() == "Windows":
            gen = self._get_msvc_generator()
            checks.append(("MSVC编译器/生成器", bool(gen)))
            if gen: self.logger.debug(f"检测到生成器: {gen}")
        else:
            compiler = shutil.which("gcc") or shutil.which("clang")
            if compiler:
                try:
                    res = subprocess.run([compiler, "--version"], capture_output=True, text=True)
                    self.logger.debug(f"编译器: {res.stdout.splitlines()[0]}")
                except: pass
            checks.append(("C++编译器", bool(compiler)))

        # 3. Qt6 (含严格版本验证)
        is_matched, qt_path = self._get_qt_prefix_path()
        checks.append(("Qt6", bool(qt_path)))
        if qt_path:
            if is_matched:
                self.logger.info(f"  找到匹配版本的 Qt6: {qt_path}")
            else:
                self.logger.warning(f"  找到 Qt6 但版本可能不完全匹配: {qt_path}")

        failed_checks = [name for name, success in checks if not success]
        if failed_checks:
            self.logger.error(f"环境检查失败: {', '.join(failed_checks)}")
            return False

        self.logger.info("环境检查通过")
        self.logger.info("=" * 60)
        return True

    def clean_build_dir(self, force: bool = False) -> bool:
        """清理构建目录，含确认提示"""
        if not self.build_dir.exists():
            return True

        if not force:
            try:
                confirm = input(f"确定要清理构建目录 {self.build_dir} 吗? (y/N): ")
                if confirm.lower() != 'y':
                    self.logger.info("已取消清理操作")
                    return True
            except EOFError:
                # 在非交互环境下默认不清理
                self.logger.warning("非交互环境，跳过清理确认")
                return False

        self.logger.info(f"正在清理构建目录: {self.build_dir}")
        try:
            shutil.rmtree(self.build_dir)
            return True
        except Exception as e:
            self.logger.error(f"清理失败: {e}")
            return False

    def configure_cmake(self, build_type: str) -> bool:
        """配置 CMake (POSIX 路径处理)"""
        start_time = time.time()
        self.logger.info("=" * 60)
        self.logger.info(f"正在配置 CMake (类型: {build_type})")

        if not self.build_dir.exists():
            self.build_dir.mkdir(parents=True)

        os.chdir(self.build_dir)
        generator = self._get_cmake_generator()
        self.logger.info(f"选择生成器: {generator}")

        cmake_cmd = [
            "cmake",
            PurePath(self.project_root).as_posix(),
            f"-DCMAKE_BUILD_TYPE={build_type}",
            "-G", generator
        ]

        if "Visual Studio" in generator:
            cmake_cmd.extend(["-A", "x64"])

        _, qt_prefix = self._get_qt_prefix_path()
        if qt_prefix:
            posix_qt = PurePath(qt_prefix).as_posix()
            cmake_cmd.append(f"-DCMAKE_PREFIX_PATH={posix_qt}")
            self.logger.info(f"  关联 Qt6: {posix_qt}")

        for opt, val in self.cmake_options.items():
            cmake_cmd.append(f"-D{opt}={'ON' if val else 'OFF'}")

        self.logger.debug(f"执行命令: {' '.join(cmake_cmd)}")

        try:
            result = subprocess.run(cmake_cmd, capture_output=True, text=True)
            if result.returncode == 0:
                self.stats.configure_time = time.time() - start_time
                self.logger.info(f"CMake 配置成功 (耗时: {self.stats.configure_time:.2f}s)")
                self.logger.info("=" * 60)
                return True
            else:
                self.logger.error(f"CMake 配置失败:\n{result.stderr}")
                return False
        except Exception as e:
            self.logger.error(f"CMake 配置异常: {e}")
            return False

    def build_project(self, build_type: str, jobs: Optional[int] = None) -> bool:
        """执行构建，优化进度输出"""
        start_time = time.time()
        self.logger.info("=" * 60)
        self.logger.info(f"开始构建项目 (类型: {build_type})")

        if jobs is None:
            jobs = os.cpu_count() or 4
        self.logger.info(f"并行度: {jobs}")

        build_cmd = ["cmake", "--build", ".", "--parallel", str(jobs)]

        try:
            process = subprocess.Popen(
                build_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                bufsize=1,
                cwd=self.build_dir
            )

            if process.stdout:
                for line in process.stdout:
                    line = line.strip()
                    if not line: continue

                    is_progress = re.match(r"^\[\d+/\d+\]", line)

                    if is_progress:
                        if not self.verbose:
                            print(f"\r  进度: {line}", end="", flush=True)
                        else:
                            self.logger.info(line)
                    elif any(x in line.lower() for x in ["warning", "警告"]):
                        self.stats.warnings += 1
                        self.logger.warning(line)
                    elif any(x in line.lower() for x in ["error", "错误", "failed"]):
                        self.stats.errors += 1
                        self.logger.error(line)
                    else:
                        self.logger.info(line)

            process.wait()
            if not self.verbose: print() # 换行
            self.stats.build_time = time.time() - start_time

            if process.returncode == 0:
                self.logger.info("=" * 60)
                self.logger.info("构建成功完成")
                self.stats.report(self.logger)
                return True
            else:
                self.logger.error(f"构建失败，退出码: {process.returncode}")
                # 备份失败日志
                try:
                    shutil.copy(self.project_root / 'build.log', self.project_root / 'build_last_failed.log')
                    self.logger.info(f"失败日志已保存至: {self.project_root / 'build_last_failed.log'}")
                except: pass
                return False
        except Exception as e:
            self.logger.error(f"构建过程中出现异常: {e}")
            return False

    def install_project(self) -> bool:
        """安装项目"""
        self.logger.info("=" * 60)
        self.logger.info("正在安装项目...")
        install_cmd = ["cmake", "--install", ".", "--prefix", "install"]
        try:
            res = subprocess.run(install_cmd, capture_output=True, text=True, cwd=self.build_dir)
            if res.returncode == 0:
                self.logger.info("安装成功 (目录: build/install)")
                self.logger.info("=" * 60)
                return True
            else:
                self.logger.error(f"安装失败: {res.stderr}")
                return False
        except Exception as e:
            self.logger.error(f"安装异常: {e}")
            return False

    def run_tests(self) -> bool:
        """运行测试，丰富反馈信息"""
        self.logger.info("=" * 60)
        self.logger.info("正在运行项目测试...")
        try:
            res = subprocess.run(["ctest", "--output-on-failure"], capture_output=True, text=True, cwd=self.build_dir)
            if res.returncode == 0:
                self.logger.info("所有测试通过！")
                self.logger.debug(f"输出结果:\n{res.stdout}")
                self.logger.info("=" * 60)
                return True
            else:
                self.logger.error(f"测试未通过 (退出码 {res.returncode}):")
                self.logger.error(res.stdout)
                self.logger.info("=" * 60)
                return False
        except Exception as e:
            self.logger.error(f"运行测试异常: {e}")
            return False

def main():
    parser = argparse.ArgumentParser(
        description="EasyKiConverter 项目构建管理工具",
        epilog="""
示例:
  python tools/python/build_project.py                    # 默认 Debug 构建
  python tools/python/build_project.py -t Release        # Release 构建
  python tools/python/build_project.py -c -y            # 强制清理目录并编译
  python tools/python/build_project.py --check           # 仅执行环境 dependencies 检查
  python tools/python/build_project.py --config-only     # 仅执行配置，不进行编译
  python tools/python/build_project.py -t Release -i     # 构建并安装
        """,
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("-t", "--type", choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"],
                        default="Debug", help="构建类型 (默认: Debug)")
    parser.add_argument("-c", "--clean", action="store_true", help="构建前清理目录")
    parser.add_argument("-y", "--yes", action="store_true", help="自动确认所有提示 (如清理确认)")
    parser.add_argument("-i", "--install", action="store_true", help="构建后执行安装")
    parser.add_argument("--test", action="store_true", help="构建后运行测试")
    parser.add_argument("--check", action="store_true", help="仅执行环境检查")
    parser.add_argument("--config-only", action="store_true", help="仅执行配置，不执行构建")
    parser.add_argument("-j", "--jobs", type=int, help="并行任务数 (默认 CPU 核心数)")
    parser.add_argument("-v", "--verbose", action="store_true", help="显示详细输出")

    args = parser.parse_args()
    manager = BuildManager(verbose=args.verbose)

    try:
        if args.check:
            success = manager.check_environment()
            sys.exit(0 if success else 1)

        if not manager.check_environment():
            sys.exit(1)

        if args.clean:
            if not manager.clean_build_dir(force=args.yes):
                sys.exit(1)

        if not manager.configure_cmake(args.type):
            sys.exit(1)

        if args.config_only:
            manager.logger.info("仅执行配置完成。")
            sys.exit(0)

        if not manager.build_project(args.type, args.jobs):
            sys.exit(1)

        if args.test:
            manager.run_tests()

        if args.install:
            manager.install_project()

    except KeyboardInterrupt:
        print("\n构建被用户中断")
        sys.exit(1)
    except Exception as e:
        print(f"\n发生未处理的错误: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
