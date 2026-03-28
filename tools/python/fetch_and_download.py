"""
LCEDA 元器件数据获取与文件下载工具
====================================
支持从 LCEDA Pro API 获取元器件信息并下载相关文件

输出目录结构:
    output_dir/
    ├── lceda_results.json      # 元器件数据
    ├── download_results.json    # 下载结果
    ├── datasheet/              # 数据手册
    │   └── {manufacturer_part}.pdf
    └── imgs/                   # 预览图
        └── {manufacturer_part}_{index}.png

用法:
    # 获取并下载 (默认输出到 output/ 目录)
    python fetch_and_download.py --ids C8734 C52717

    # 指定输出目录
    python fetch_and_download.py --ids C8734 --output-dir ./downloads

    # 从文件读取 ID 列表
    python fetch_and_download.py --ids-file component_ids.txt

    # 仅下载（基于已有数据）
    python fetch_and_download.py --download-only --input lceda_results.json

    # 检查已有数据状态
    python fetch_and_download.py --check-status --input lceda_results.json
"""

import os
import sys
import json
import time
import random
import argparse
import shutil
import requests
from pathlib import Path
from urllib.parse import urlparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field, asdict
from typing import Optional, Dict, List
from enum import Enum


class DownloadStatus(Enum):
    SUCCESS = "success"
    EXISTS = "exists"
    ERROR = "error"
    SKIPPED = "skipped"


@dataclass
class ComponentData:
    keyword: str
    code: Optional[str] = None
    name: Optional[str] = None
    manufacturer_part: Optional[str] = None
    datasheet: Optional[str] = None
    images: List[str] = field(default_factory=list)
    status: str = "pending"
    error: Optional[str] = None


@dataclass
class DownloadResult:
    url: str
    path: str
    status: str
    size: int = 0
    error: Optional[str] = None


class LCEDAConnector:
    API_URL = "https://pro.lceda.cn/api/v2/eda/product/search"

    HEADERS = {
        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36",
        "Accept": "application/json, text/javascript, */*; q=0.01",
        "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
        "X-Requested-With": "XMLHttpRequest",
    }

    def __init__(self, max_workers: int = 5, rate_limit: float = 0.2):
        self.max_workers = max_workers
        self.rate_limit = rate_limit
        self.results: Dict[str, ComponentData] = {}

    def _search_product(self, keyword: str) -> ComponentData:
        payload = {
            "keyword": keyword,
            "currPage": 1,
            "pageSize": 50,
            "needAggs": "true",
        }

        try:
            response = requests.post(
                self.API_URL, data=payload, headers=self.HEADERS, timeout=10
            )
            response.raise_for_status()
            data = response.json()

            result = data.get("result", {})
            product_list = result.get("productList", [])

            if not product_list:
                return ComponentData(keyword=keyword, status="not_found")

            product = product_list[0]
            device_info = product.get("device_info", {})
            attributes = device_info.get("attributes", {})

            images_str = product.get("image", "")
            image_list = images_str.split("<$>") if images_str else []

            return ComponentData(
                keyword=keyword,
                code=product.get("code"),
                name=product.get("name"),
                manufacturer_part=attributes.get("Manufacturer Part"),
                datasheet=attributes.get("Datasheet"),
                images=image_list,
                status="success",
            )

        except requests.exceptions.RequestException as e:
            return ComponentData(keyword=keyword, status="error", error=str(e))
        except json.JSONDecodeError:
            return ComponentData(
                keyword=keyword, status="error", error="Invalid JSON response"
            )
        except Exception as e:
            return ComponentData(keyword=keyword, status="error", error=str(e))

    def fetch_all(self, keywords: List[str]) -> Dict[str, ComponentData]:
        print(f"开始获取 {len(keywords)} 个元器件数据... (线程数: {self.max_workers})")

        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            future_to_keyword = {
                executor.submit(self._search_product, keyword): keyword
                for keyword in keywords
            }

            completed = 0
            for future in as_completed(future_to_keyword):
                keyword = future_to_keyword[future]
                completed += 1

                try:
                    result = future.result()
                    self.results[keyword] = result

                    if result.status == "success":
                        img_count = len(result.images)
                        has_ds = "✓" if result.datasheet else "✗"
                        name_display = (result.name or "N/A")[:20]
                        print(
                            f"[{completed}/{len(keywords)}] {keyword} | {name_display}... | 图:{img_count} | DS:{has_ds}"
                        )
                    elif result.status == "not_found":
                        print(f"[{completed}/{len(keywords)}] {keyword} | 未找到")
                    else:
                        print(
                            f"[{completed}/{len(keywords)}] {keyword} | 错误: {result.error}"
                        )

                except Exception as e:
                    self.results[keyword] = ComponentData(
                        keyword=keyword, status="error", error=str(e)
                    )
                    print(f"[{completed}/{len(keywords)}] {keyword} | 异常: {e}")

                time.sleep(random.uniform(self.rate_limit, self.rate_limit * 2))

        return self.results

    def save_results(self, output_dir: Path):
        output_dir.mkdir(parents=True, exist_ok=True)
        path = output_dir / "lceda_results.json"

        serializable = {k: asdict(v) for k, v in self.results.items()}
        with open(path, "w", encoding="utf-8") as f:
            json.dump(serializable, f, ensure_ascii=False, indent=2)
        print(f"元器件数据已保存至: {path.resolve()}")

    @classmethod
    def load_results(cls, filepath: str) -> Dict[str, ComponentData]:
        with open(filepath, "r", encoding="utf-8") as f:
            data = json.load(f)
        return {k: ComponentData(**v) for k, v in data.items()}


class FileDownloader:
    FILE_EXTENSIONS = {
        ".pdf": ".pdf",
        ".html": ".html",
        ".htm": ".html",
        ".png": ".png",
        ".jpg": ".jpg",
        ".jpeg": ".jpg",
        ".gif": ".gif",
        ".webp": ".webp",
    }

    CONTENT_TYPE_MAP = {
        "pdf": ".pdf",
        "html": ".html",
        "png": ".png",
        "jpeg": ".jpg",
        "jpg": ".jpg",
        "gif": ".gif",
        "webp": ".webp",
    }

    HEADERS = {
        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36",
        "Accept": "*/*",
        "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    }

    def __init__(self, output_dir: Path, max_workers: int = 5):
        self.max_workers = max_workers
        self.output_dir = Path(output_dir)
        self.results: Dict[str, DownloadResult] = {}

        self.datasheet_dir = self.output_dir / "datasheet"
        self.imgs_dir = self.output_dir / "imgs"

        for dir_path in [self.datasheet_dir, self.imgs_dir]:
            dir_path.mkdir(parents=True, exist_ok=True)

    def _get_extension_from_url(self, url: str) -> str:
        parsed = Path(urlparse(url).path).suffix.lower()
        return self.FILE_EXTENSIONS.get(parsed, ".bin")

    def _sanitize_filename(self, filename: str) -> str:
        if not filename:
            return "unnamed"

        invalid_chars = '<>:"/\\|?*'
        for char in invalid_chars:
            filename = filename.replace(char, "_")

        filename = filename.strip().strip(".")

        if len(filename) > 100:
            name, ext = os.path.splitext(filename)
            filename = f"{name[:90]}_{hash(name) % 10000}{ext}"

        return filename or "unnamed"

    def _get_content_type_extension(self, content_type: str, current_ext: str) -> str:
        content_type = content_type.lower().split(";")[0].strip()
        for key, ext in self.CONTENT_TYPE_MAP.items():
            if key in content_type:
                if key == "html" and current_ext in [".html", ".htm"]:
                    return current_ext
                if ext == ".jpg" and current_ext in [".jpg", ".jpeg"]:
                    return current_ext
                return ext
        return current_ext

    def _download_file(
        self, url: str, save_path: Path, max_retries: int = 2
    ) -> DownloadResult:
        temp_path = save_path.with_suffix(save_path.suffix + ".tmp")

        for attempt in range(max_retries + 1):
            try:
                response = requests.get(
                    url, headers=self.HEADERS, timeout=30, stream=True
                )
                response.raise_for_status()

                content_type = response.headers.get("Content-Type", "")
                final_ext = self._get_content_type_extension(
                    content_type, save_path.suffix
                )

                if final_ext != save_path.suffix:
                    save_path = save_path.with_suffix(final_ext)
                    temp_path = temp_path.with_suffix(final_ext + ".tmp")

                with open(temp_path, "wb") as f:
                    for chunk in response.iter_content(chunk_size=8192):
                        f.write(chunk)

                temp_path.rename(save_path)

                return DownloadResult(
                    url=url,
                    path=str(save_path),
                    status=DownloadStatus.SUCCESS.value,
                    size=save_path.stat().st_size,
                )

            except requests.exceptions.RequestException as e:
                if attempt == max_retries:
                    if temp_path.exists():
                        try:
                            temp_path.unlink()
                        except:
                            pass
                    return DownloadResult(
                        url=url,
                        path=str(save_path),
                        status=DownloadStatus.ERROR.value,
                        error=str(e),
                    )
                time.sleep(1 * (attempt + 1))

            except Exception as e:
                return DownloadResult(
                    url=url,
                    path=str(save_path),
                    status=DownloadStatus.ERROR.value,
                    error=f"IO Error: {str(e)}",
                )

    def _download_datasheet(
        self, component_id: str, manufacturer_part: str, url: str
    ) -> DownloadResult:
        safe_name = self._sanitize_filename(manufacturer_part or component_id)
        ext = self._get_extension_from_url(url)
        filename = f"{safe_name}{ext}"
        save_path = self.datasheet_dir / filename

        if save_path.exists():
            return DownloadResult(
                url=url,
                path=str(save_path),
                status=DownloadStatus.EXISTS.value,
                size=save_path.stat().st_size,
            )

        return self._download_file(url, save_path)

    def _download_image(
        self, component_id: str, manufacturer_part: str, image_url: str, index: int
    ) -> DownloadResult:
        safe_name = self._sanitize_filename(manufacturer_part or component_id)
        ext = self._get_extension_from_url(image_url)
        filename = f"{safe_name}_{index}{ext}"
        save_path = self.imgs_dir / filename

        if save_path.exists():
            return DownloadResult(
                url=image_url,
                path=str(save_path),
                status=DownloadStatus.EXISTS.value,
                size=save_path.stat().st_size,
            )

        return self._download_file(image_url, save_path)

    def download_all(self, data: Dict[str, ComponentData]) -> Dict[str, DownloadResult]:
        tasks = []
        for component_id, info in data.items():
            if info.status != "success":
                continue

            manufacturer_part = info.manufacturer_part or component_id

            if info.datasheet:
                tasks.append(
                    ("datasheet", component_id, manufacturer_part, info.datasheet, 0)
                )

            for i, img_url in enumerate(info.images):
                tasks.append(("image", component_id, manufacturer_part, img_url, i + 1))

        if not tasks:
            print("没有需要下载的任务。")
            return self.results

        print(f"开始下载 {len(tasks)} 个文件... (线程数: {self.max_workers})")

        success_count = error_count = exists_count = 0

        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            future_to_task = {}
            for task in tasks:
                task_type, cid, mpart, url, idx = task
                if task_type == "datasheet":
                    future = executor.submit(self._download_datasheet, cid, mpart, url)
                else:
                    future = executor.submit(self._download_image, cid, mpart, url, idx)
                future_to_task[future] = (task_type, cid, url)

            for future in as_completed(future_to_task):
                task_type, cid, url = future_to_task[future]
                try:
                    result = future.result()
                    key = f"{cid}_{task_type}_{url}"
                    self.results[key] = result

                    if result.status == DownloadStatus.SUCCESS.value:
                        success_count += 1
                        print(f"[✓] {cid} ({task_type}): {result.path}")
                    elif result.status == DownloadStatus.EXISTS.value:
                        exists_count += 1
                    else:
                        error_count += 1
                        print(f"[✗] {cid} ({task_type}): {result.error}")

                except Exception as e:
                    error_count += 1
                    print(f"[✗] {cid} ({task_type}): 异常 - {e}")

                time.sleep(random.uniform(0.05, 0.1))

        print(
            f"\n下载完成 | 成功: {success_count} | 跳过: {exists_count} | 失败: {error_count}"
        )
        return self.results

    def save_results(self):
        path = self.output_dir / "download_results.json"
        with open(path, "w", encoding="utf-8") as f:
            json.dump(
                [asdict(r) for r in self.results.values()],
                f,
                ensure_ascii=False,
                indent=2,
            )
        print(f"下载结果已保存至: {path.resolve()}")


def parse_ids_from_file(filepath: str) -> List[str]:
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()

    ids = []
    for line in content.splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            ids.extend(word.strip() for word in line.split(",") if word.strip())

    return list(set(ids))


def check_data_status(data: Dict[str, ComponentData]):
    total = len(data)
    success = sum(1 for v in data.values() if v.status == "success")
    not_found = sum(1 for v in data.values() if v.status == "not_found")
    error = sum(1 for v in data.values() if v.status == "error")

    has_datasheet = sum(1 for v in data.values() if v.datasheet)
    has_images = sum(1 for v in data.values() if v.images)

    print(f"\n=== 数据状态统计 ===")
    print(f"总数: {total}")
    print(f"成功: {success} | 未找到: {not_found} | 错误: {error}")
    print(f"有 Datasheet: {has_datasheet} | 有图片: {has_images}")
    print(f"无 Datasheet: {total - has_datasheet} | 无图片: {total - has_images}")


def find_input_file(name: str, search_dirs: List[Path]) -> Optional[Path]:
    for d in search_dirs:
        p = d / name
        if p.exists():
            return p
        for parent in d.parents:
            p = parent / name
            if p.exists():
                return p
    return None


def main():
    parser = argparse.ArgumentParser(
        description="LCEDA 元器件数据获取与文件下载工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument("--ids", nargs="+", help="元器件 ID 列表")
    parser.add_argument("--ids-file", help="包含元器件 ID 的文件路径")
    parser.add_argument("--input", "-i", help="已有数据文件 (JSON)，支持相对路径")
    parser.add_argument(
        "--output-dir", "-o", default="output", help="输出目录 (默认: output)"
    )
    parser.add_argument(
        "--workers", "-w", type=int, default=5, help="并发线程数 (默认: 5)"
    )
    parser.add_argument(
        "--rate-limit", type=float, default=0.2, help="请求间隔秒数 (默认: 0.2)"
    )
    parser.add_argument(
        "--force", "-f", action="store_true", help="强制重新下载已存在的文件"
    )

    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--download-only", action="store_true", help="仅下载模式 (需要 --input)"
    )
    group.add_argument(
        "--check-status", action="store_true", help="检查数据状态 (需要 --input)"
    )

    args = parser.parse_args()

    output_dir = Path(args.output_dir).resolve()

    if args.download_only or args.check_status:
        input_path = None
        if args.input:
            input_path = Path(args.input)
            if not input_path.is_absolute() and not input_path.exists():
                input_path = find_input_file(
                    args.input, [Path.cwd(), Path(__file__).parent.parent.parent]
                )
            if not input_path or not input_path.exists():
                print(f"错误: 未找到输入文件 '{args.input}'")
                return 1
        else:
            default_input = output_dir / "lceda_results.json"
            if default_input.exists():
                input_path = default_input
            else:
                print(f"错误: 未找到输入文件，请使用 --input 指定")
                return 1

        try:
            data = LCEDAConnector.load_results(str(input_path))
            print(f"已加载 {len(data)} 条数据 from: {input_path}")
        except json.JSONDecodeError:
            print(f"错误: 文件 '{input_path}' 格式损坏")
            return 1

        if args.check_status:
            check_data_status(data)
            return 0

        downloader = FileDownloader(output_dir=output_dir, max_workers=args.workers)
        downloader.download_all(data)
        downloader.save_results()
        print(f"\n输出目录: {output_dir}")
        return 0

    keywords = []
    if args.ids:
        keywords = list(set(args.ids))
    elif args.ids_file:
        ids_path = Path(args.ids_file)
        if not ids_path.exists():
            print(f"错误: 未找到文件 '{args.ids_file}'")
            return 1
        keywords = parse_ids_from_file(str(ids_path))
    else:
        parser.print_help()
        return 0

    if not keywords:
        print("没有要处理的元器件 ID")
        return 0

    connector = LCEDAConnector(max_workers=args.workers, rate_limit=args.rate_limit)

    try:
        connector.fetch_all(keywords)
        connector.save_results(output_dir)

        success = sum(1 for r in connector.results.values() if r.status == "success")
        print(f"\n总览: 成功 {success}/{len(connector.results)}")

        should_download = input("\n是否下载文件? (y/N): ").strip().lower() in [
            "y",
            "yes",
        ]
        if should_download:
            downloader = FileDownloader(output_dir=output_dir, max_workers=args.workers)
            downloader.download_all(connector.results)
            downloader.save_results()
            print(f"\n输出目录: {output_dir}")
            print(f"  ├── lceda_results.json")
            print(f"  ├── download_results.json")
            print(f"  ├── datasheet/")
            print(f"  └── imgs/")
        else:
            print("跳过下载")
            print(f"\n输出目录: {output_dir}")

    except KeyboardInterrupt:
        print("\n用户中断执行")
        connector.save_results(output_dir)
        return 130

    return 0


if __name__ == "__main__":
    sys.exit(main())
