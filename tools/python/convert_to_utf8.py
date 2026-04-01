#!/usr/bin/env python3
"""
EasyKiConverter 编码转换工具
=============================

将项目文件转换为 UTF-8 (无 BOM) 编码，支持并行处理和缓存。

使用方法:
    python tools/python/convert_to_utf8.py                    # 转换所有文件
    python tools/python/convert_to_utf8.py ./src             # 转换指定目录
    python tools/python/convert_to_utf8.py --ext .cpp .h    # 指定文件类型
    python tools/python/convert_to_utf8.py --check            # 仅检查编码
    python tools/python/convert_to_utf8.py --clean           # 清除缓存

示例:
    $ python tools/python/convert_to_utf8.py --dry-run       # 预览模式
    $ python tools/python/convert_to_utf8.py --verbose        # 详细输出
    $ python tools/python/convert_to_utf8.py --ext .py        # 仅 Python 文件
"""

import os
import sys
import argparse
import hashlib
import json
import time
import threading
from pathlib import Path
from typing import Optional, Tuple, Dict, List
from concurrent.futures import ThreadPoolExecutor, as_completed

try:
    import charset_normalizer as chardet
except ImportError:
    try:
        import chardet
    except ImportError:
        chardet = None


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


class EncodingConverter:
    def __init__(
        self, dry_run: bool = False, verbose: bool = False, check: bool = False
    ):
        self.project_root = Path(__file__).parent.parent.parent
        self.cache_dir = self.project_root / "build" / ".cache"
        self.cache_file = self.cache_dir / "encoding_cache.json"
        self.dry_run = dry_run
        self.verbose = verbose
        self.check = check
        self.stats = {
            "total": 0,
            "converted": 0,
            "skipped": 0,
            "errors": 0,
            "utf8_already": 0,
        }
        self.lock = threading.Lock()
        self.cache = self._load_cache()
        self.progress_lock = threading.Lock()
        self.processed = 0

    def _load_cache(self) -> Dict[str, str]:
        if self.cache_file.exists():
            try:
                with open(self.cache_file, "r", encoding="utf-8") as f:
                    return json.load(f)
            except Exception:
                pass
        return {}

    def _save_cache(self) -> None:
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        try:
            with open(self.cache_file, "w", encoding="utf-8") as f:
                json.dump(self.cache, f, indent=2)
        except Exception:
            pass

    def _get_file_hash(self, file_path: Path) -> str:
        try:
            with open(file_path, "rb") as f:
                return hashlib.md5(f.read()).hexdigest()
        except Exception:
            return ""

    def _is_cached(self, file_path: Path) -> bool:
        relative_path = str(file_path.relative_to(self.project_root))
        cache_key = f"enc_{relative_path}"
        if cache_key in self.cache:
            return self.cache[cache_key] == self._get_file_hash(file_path)
        return False

    def _update_cache(self, file_path: Path) -> None:
        relative_path = str(file_path.relative_to(self.project_root))
        cache_key = f"enc_{relative_path}"
        self.cache[cache_key] = self._get_file_hash(file_path)

    def detect_encoding(self, file_path: Path) -> Optional[str]:
        try:
            with open(file_path, "rb") as f:
                raw_data = f.read()
        except Exception:
            return None

        if chardet:
            result = chardet.detect(raw_data)
            if isinstance(result, dict):
                return result.get("encoding")
            elif hasattr(result, "encoding"):
                return result.encoding
            elif hasattr(result, "best"):
                return result.best().encoding

        encodings = ["utf-8", "gb18030", "gbk", "utf-16", "latin-1"]
        for enc in encodings:
            try:
                raw_data.decode(enc)
                return enc
            except UnicodeDecodeError:
                continue
        return None

    def _print_progress(self, total: int) -> None:
        bar_width = 40
        percent = self.processed / total if total > 0 else 0
        filled = int(bar_width * percent)
        bar = "█" * filled + "░" * (bar_width - filled)
        prefix = "[检查]" if self.check else "[转换]"
        sys.stdout.write(
            f"\r{prefix} {bar} {self.processed}/{total} ({percent * 100:.1f}%) "
        )
        sys.stdout.flush()

    def convert_file(self, file_path: Path) -> Tuple[bool, str, str, int]:
        relative_path = str(file_path.relative_to(self.project_root))

        with self.progress_lock:
            self.processed += 1
            self._print_progress(self.stats["total"])

        if self._is_cached(file_path):
            with self.lock:
                self.stats["skipped"] += 1
            return True, "skipped", "UTF-8", 0

        encoding = self.detect_encoding(file_path)
        if not encoding:
            with self.lock:
                self.stats["errors"] += 1
            return False, "error", "unknown", 0

        if encoding.lower() in ("utf-8", "ascii") and not self._has_bom(file_path):
            with self.lock:
                self.stats["skipped"] += 1
                self.stats["utf8_already"] += 1
            self._update_cache(file_path)
            return True, "skipped", "UTF-8", 0

        file_size = file_path.stat().st_size
        original_encoding = encoding

        if self.check:
            with self.lock:
                self.stats["converted"] += 1
            return True, "needs_convert", original_encoding, file_size

        if self.dry_run:
            with self.lock:
                self.stats["converted"] += 1
            return True, "dry_run", original_encoding, file_size

        try:
            with open(file_path, "r", encoding=encoding) as f:
                content = f.read()

            with open(file_path, "w", encoding="utf-8") as f:
                f.write(content)

            self._update_cache(file_path)

            with self.lock:
                self.stats["converted"] += 1

            if self.verbose:
                status = colorize("[转换]", Colors.YELLOW)
                print(f"\n{status} {relative_path} ({original_encoding} -> UTF-8)")

            return True, "converted", original_encoding, file_size

        except Exception as e:
            with self.lock:
                self.stats["errors"] += 1
            return False, f"error: {e}", original_encoding, 0

    def _has_bom(self, file_path: Path) -> bool:
        try:
            with open(file_path, "rb") as f:
                first_bytes = f.read(3)
                return first_bytes in (b"\xef\xbb\xbf", b"\xff\xfe", b"\xfe\xff")
        except Exception:
            return False

    def find_files(self, root_path: Path, extensions: tuple) -> List[Path]:
        files = []
        exclude_dirs = {
            ".git",
            "build",
            ".qt",
            "bin",
            "lib",
            "node_modules",
            ".vscode",
            "flatpak_build",
        }

        if root_path.is_file():
            return [root_path]

        for root, dirs, filenames in os.walk(root_path):
            dirs[:] = [d for d in dirs if d not in exclude_dirs]

            for filename in filenames:
                if filename.endswith(extensions) or filename == "CMakeLists.txt":
                    files.append(Path(root) / filename)

        return files

    def run(self, root_path: Path, extensions: tuple) -> bool:
        print(colorize("=" * 60, Colors.CYAN))
        print(colorize("  EasyKiConverter 编码转换工具", Colors.BOLD + Colors.CYAN))
        print(colorize("=" * 60, Colors.CYAN))
        print()

        files = self.find_files(root_path, extensions)
        self.stats["total"] = len(files)

        if not files:
            print("没有找到需要处理的文件")
            return True

        print(f"扫描目录: {root_path}")
        print(f"文件总数: {len(files)}")
        print(f"模式: {'检查' if self.check else ('预览' if self.dry_run else '转换')}")
        print()

        if self.check:
            print(colorize("[检查] 开始检查文件编码...", Colors.BLUE))
        else:
            print(colorize("[转换] 开始转换文件编码...", Colors.YELLOW))

        start_time = time.time()

        with ThreadPoolExecutor(max_workers=min(8, os.cpu_count() or 4)) as executor:
            futures = {executor.submit(self.convert_file, f): f for f in files}
            for future in as_completed(futures):
                future.result()

        elapsed = time.time() - start_time

        print()
        print()
        print(colorize("=" * 60, Colors.CYAN))
        print(colorize("  处理结果", Colors.BOLD))
        print(colorize("=" * 60, Colors.CYAN))
        print(f"  总文件数:   {self.stats['total']}")
        print(
            f"  已处理:     {self.stats['converted']} {'(需转换)' if self.check else '(已转换)'}"
        )
        print(f"  缓存跳过:   {self.stats['skipped']}")
        print(f"  已是 UTF-8: {self.stats['utf8_already']}")
        print(f"  错误:       {self.stats['errors']}")
        print(f"  耗时:       {elapsed:.2f}s")
        print(colorize("=" * 60, Colors.CYAN))

        if not self.dry_run and not self.check:
            self._save_cache()

        if self.stats["errors"] > 0:
            return False
        return True

    def clean_cache(self) -> bool:
        if self.cache_file.exists():
            self.cache_file.unlink()
            print(colorize("[OK] 缓存已清除", Colors.GREEN))
        else:
            print("没有缓存文件")
        return True


def main():
    script_dir = Path(__file__).parent.parent.parent

    parser = argparse.ArgumentParser(
        description="EasyKiConverter 编码转换工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python tools/python/convert_to_utf8.py                    # 转换所有文件
  python tools/python/convert_to_utf8.py ./src             # 转换指定目录
  python tools/python/convert_to_utf8.py --ext .cpp .h     # 指定文件类型
  python tools/python/convert_to_utf8.py --check            # 仅检查编码
  python tools/python/convert_to_utf8.py --clean           # 清除缓存
        """,
    )

    parser.add_argument("path", nargs="?", default=str(script_dir), help="扫描根路径")
    parser.add_argument(
        "--ext",
        nargs="+",
        default=[".cpp", ".h", ".c", ".hpp", ".qml", ".py", ".txt", ".cmake"],
        help="文件扩展名",
    )
    parser.add_argument("--dry-run", action="store_true", help="预览模式（不实际修改）")
    parser.add_argument("--check", action="store_true", help="仅检查编码，不修改")
    parser.add_argument("--verbose", action="store_true", help="详细输出")
    parser.add_argument("--clean", action="store_true", help="清除缓存并退出")
    parser.add_argument("--exclude", nargs="+", help="排除的目录")

    args = parser.parse_args()

    if args.clean:
        converter = EncodingConverter()
        sys.exit(0 if converter.clean_cache() else 1)

    root_path = Path(args.path).resolve()
    if not root_path.exists():
        print(colorize(f"[错误] 路径不存在: {root_path}", Colors.RED))
        sys.exit(1)

    extensions = tuple(args.ext)

    converter = EncodingConverter(
        dry_run=args.dry_run, verbose=args.verbose, check=args.check
    )

    success = converter.run(root_path, extensions)

    if args.dry_run:
        print()
        print(colorize("[提示] 当前为预览模式，未实际修改文件", Colors.YELLOW))

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    sys.exit(main())
