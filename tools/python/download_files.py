import os
import json
import time
import random
import requests
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
from urllib.parse import urlparse
from typing import Optional, Dict, Any

class FileDownloader:
    def __init__(self, max_workers=5, base_dir: str = "."):
        self.max_workers = max_workers
        self.base_dir = Path(base_dir)
        self.results = {}
        
        # 定义目录路径 (使用 pathlib)
        self.datasheet_dir = self.base_dir / "datasheet"
        self.imgs_dir = self.base_dir / "imgs"
        
        # 【优化点 1】健壮地创建目录，exist_ok=True 避免目录已存在时报错
        # 这会递归创建所有不存在的父目录
        for dir_path in [self.datasheet_dir, self.imgs_dir]:
            try:
                dir_path.mkdir(parents=True, exist_ok=True)
                print(f"确认目录存在: {dir_path}")
            except PermissionError:
                print(f"错误: 无法创建目录 {dir_path}，请检查权限。")
                raise

        # 请求头
        self.headers = {
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36",
            "Accept": "*/*",
            "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
        }
    
    def _get_extension_from_url(self, url: str) -> str:
        """根据 URL 推断文件扩展名"""
        url_lower = url.lower()
        if '.pdf' in url_lower: return '.pdf'
        elif '.html' in url_lower: return '.html'
        elif '.png' in url_lower: return '.png'
        elif any(ext in url_lower for ext in ['.jpg', '.jpeg']): return '.jpg'
        elif '.gif' in url_lower: return '.gif'
        elif '.webp' in url_lower: return '.webp'
        else: return '.bin'  # 默认二进制文件
    
    def _sanitize_filename(self, filename: str) -> str:
        """清理文件名中的非法字符"""
        if not filename:
            return "unnamed"
        # 替换非法字符
        invalid_chars = '<>:\"/\\|?*'
        for char in invalid_chars:
            filename = filename.replace(char, '_')
        # 去除首尾空格和点
        filename = filename.strip().strip('.')
        # 限制长度 (可选，防止 Windows 路径过长)
        if len(filename) > 100:
            name, ext = os.path.splitext(filename)
            filename = f"{name[:90]}_{hash(name) % 10000}{ext}"
        return filename if filename else "unnamed"

    def _download_file(self, url: str, save_path: Path, max_retries: int = 2) -> dict:
        """下载单个文件，包含重试机制和原子写入"""
        final_path = save_path
        temp_path = save_path.with_suffix(save_path.suffix + '.tmp')
        
        for attempt in range(max_retries + 1):
            try:
                response = requests.get(url, headers=self.headers, timeout=30, stream=True)
                response.raise_for_status()
                
                # 动态调整扩展名 (基于 Content-Type)
                content_type = response.headers.get('Content-Type', '').lower()
                current_ext = final_path.suffix.lower()
                
                new_ext = None
                if 'pdf' in content_type and current_ext != '.pdf': new_ext = '.pdf'
                elif 'html' in content_type and current_ext not in ['.html', '.htm']: new_ext = '.html'
                elif 'png' in content_type and current_ext != '.png': new_ext = '.png'
                elif 'jpeg' in content_type or 'jpg' in content_type: 
                    if current_ext not in ['.jpg', '.jpeg']: new_ext = '.jpg'
                
                if new_ext:
                    final_path = final_path.with_suffix(new_ext)
                    temp_path = temp_path.with_suffix(new_ext + '.tmp')

                # 【优化点 2】原子写入：先写临时文件，成功后再重命名
                with open(temp_path, 'wb') as f:
                    for chunk in response.iter_content(chunk_size=8192):
                        f.write(chunk)
                
                # 重命名临时文件为正式文件
                temp_path.rename(final_path)
                
                return {
                    'url': url,
                    'path': str(final_path),
                    'status': 'success',
                    'size': final_path.stat().st_size
                }
                
            except requests.exceptions.RequestException as e:
                if attempt == max_retries:
                    # 清理可能留下的损坏临时文件
                    if temp_path.exists():
                        try: temp_path.unlink()
                        except: pass
                    return {
                        'url': url,
                        'path': str(final_path),
                        'status': 'error',
                        'error': str(e)
                    }
                # 重试前短暂等待
                time.sleep(1 * (attempt + 1))
            except Exception as e:
                return {
                    'url': url,
                    'path': str(final_path),
                    'status': 'error',
                    'error': f"IO Error: {str(e)}"
                }

    def _download_datasheet(self, component_id: str, manufacturer_part: str, url: str) -> dict:
        safe_name = self._sanitize_filename(manufacturer_part) if manufacturer_part else component_id
        ext = self._get_extension_from_url(url)
        filename = f"{safe_name}{ext}"
        save_path = self.datasheet_dir / filename
        
        if save_path.exists():
            return {'component_id': component_id, 'url': url, 'path': str(save_path), 'status': 'exists'}
        
        return self._download_file(url, save_path)

    def _download_image(self, component_id: str, manufacturer_part: str, image_url: str, index: int) -> dict:
        safe_name = self._sanitize_filename(manufacturer_part) if manufacturer_part else component_id
        ext = self._get_extension_from_url(image_url)
        filename = f"{safe_name}_{index}{ext}"
        save_path = self.imgs_dir / filename
        
        if save_path.exists():
            return {'component_id': component_id, 'url': image_url, 'path': str(save_path), 'status': 'exists'}
        
        return self._download_file(image_url, save_path)

    def download_all(self, data: dict) -> dict:
        tasks = []
        for component_id, info in data.items():
            if info.get('status') != 'success':
                continue
            
            manufacturer_part = info.get('manufacturer_part', component_id)
            
            if info.get('datasheet'):
                tasks.append(('datasheet', component_id, manufacturer_part, info['datasheet'], 0))
            
            for i, img_url in enumerate(info.get('images', [])):
                tasks.append(('image', component_id, manufacturer_part, img_url, i + 1))
        
        if not tasks:
            print("没有需要下载的任务。")
            return self.results

        print(f"开始下载 {len(tasks)} 个文件... (线程数: {self.max_workers})")
        
        success_count = error_count = exists_count = 0
        
        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            future_to_task = {}
            for task in tasks:
                task_type, cid, mpart, url, idx = task
                if task_type == 'datasheet':
                    future = executor.submit(self._download_datasheet, cid, mpart, url)
                else:
                    future = executor.submit(self._download_image, cid, mpart, url, idx)
                future_to_task[future] = (task_type, cid, url)
            
            for future in as_completed(future_to_task):
                task_type, cid, url = future_to_task[future]
                try:
                    result = future.result()
                    self.results[f"{cid}_{task_type}_{url}"] = result
                    
                    if result['status'] == 'success':
                        success_count += 1
                        print(f"[✓] {cid} ({task_type}): {result['path']}")
                    elif result['status'] == 'exists':
                        exists_count += 1
                    else:
                        error_count += 1
                        print(f"[✗] {cid} ({task_type}): {result.get('error', 'Unknown')}")
                except Exception as e:
                    error_count += 1
                    print(f"[✗] {cid} ({task_type}): 异常 - {e}")
                
                time.sleep(random.uniform(0.05, 0.1))

        print(f"\n下载完成 | 成功: {success_count} | 跳过: {exists_count} | 失败: {error_count}")
        return self.results

    def save_results(self, filename: str = 'download_results.json'):
        path = Path(filename)
        # 确保结果文件所在的目录也存在
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(self.results, f, ensure_ascii=False, indent=2)
        print(f"结果已保存至: {path.resolve()}")

if __name__ == "__main__":
    try:
        with open('lceda_results.json', 'r', encoding='utf-8') as f:
            data = json.load(f)
        print(f"读取到 {len(data)} 个元器件数据\n")
        
        downloader = FileDownloader(max_workers=5)
        downloader.download_all(data)
        downloader.save_results('download_results.json')
    except FileNotFoundError:
        print("错误: 未找到 'lceda_results.json' 文件。请先运行 fetch_lceda.py。")
    except json.JSONDecodeError:
        print("错误: 'lceda_results.json' 文件格式损坏。")