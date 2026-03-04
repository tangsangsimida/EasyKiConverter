import os
import json
import time
import random
import requests
from concurrent.futures import ThreadPoolExecutor, as_completed
from urllib.parse import urlparse


class FileDownloader:
    def __init__(self, max_workers=5):
        self.max_workers = max_workers
        self.results = {}
        
        # 创建目录
        self.datasheet_dir = "datasheet"
        self.imgs_dir = "imgs"
        
        for dir_path in [self.datasheet_dir, self.imgs_dir]:
            if not os.path.exists(dir_path):
                os.makedirs(dir_path)
        
        # 请求头
        self.headers = {
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36",
            "Accept": "*/*",
            "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
        }
    
    def _get_filename_from_url(self, url: str, prefix: str = "") -> str:
        """从URL中提取文件名"""
        parsed = urlparse(url)
        filename = os.path.basename(parsed.path)
        
        if not filename or '.' not in filename:
            # 根据URL类型添加扩展名
            if 'pdf' in url.lower():
                filename = f"{prefix}.pdf"
            else:
                # 尝试从Content-Type推断
                ext = self._get_extension_from_url(url)
                filename = f"{prefix}{ext}"
        
        return filename
    
    def _sanitize_filename(self, filename: str) -> str:
        """清理文件名中的非法字符"""
        if not filename:
            return ""
        # 替换非法字符
        invalid_chars = '<>:"/\\|?*'
        for char in invalid_chars:
            filename = filename.replace(char, '_')
        return filename
    
    def _get_extension_from_url(self, url: str) -> str:
        """根据URL推断文件扩展名"""
        url_lower = url.lower()
        if '.pdf' in url_lower:
            return '.pdf'
        elif '.html' in url_lower:
            return '.html'
        elif '.png' in url_lower:
            return '.png'
        elif '.jpg' in url_lower or '.jpeg' in url_lower:
            return '.jpg'
        elif '.gif' in url_lower:
            return '.gif'
        elif '.webp' in url_lower:
            return '.webp'
        else:
            return '.html'  # 默认
    
    def _download_file(self, url: str, save_path: str) -> dict:
        """下载单个文件，根据Content-Type确定扩展名"""
        try:
            response = requests.get(url, headers=self.headers, timeout=30)
            response.raise_for_status()
            
            # 根据Content-Type确定文件扩展名
            content_type = response.headers.get('Content-Type', '').lower()
            if 'pdf' in content_type:
                if not save_path.endswith('.pdf'):
                    save_path = save_path.rsplit('.', 1)[0] + '.pdf'
            elif 'html' in content_type:
                if not save_path.endswith('.html'):
                    save_path = save_path.rsplit('.', 1)[0] + '.html'
            elif 'png' in content_type:
                if not save_path.endswith('.png'):
                    save_path = save_path.rsplit('.', 1)[0] + '.png'
            elif 'jpeg' in content_type or 'jpg' in content_type:
                if not save_path.endswith(('.jpg', '.jpeg')):
                    save_path = save_path.rsplit('.', 1)[0] + '.jpg'
            
            with open(save_path, 'wb') as f:
                f.write(response.content)
            
            return {
                'url': url,
                'path': save_path,
                'status': 'success',
                'size': len(response.content)
            }
            
        except requests.exceptions.RequestException as e:
            return {
                'url': url,
                'path': save_path,
                'status': 'error',
                'error': str(e)
            }
    
    def _download_datasheet(self, component_id: str, manufacturer_part: str, url: str) -> dict:
        """下载 datasheet，文件名格式: Manufacturer_Part.扩展名"""
        # 使用 Manufacturer Part 作为文件名
        safe_name = self._sanitize_filename(manufacturer_part) if manufacturer_part else component_id
        ext = self._get_extension_from_url(url)
        filename = f"{safe_name}{ext}"
        save_path = os.path.join(self.datasheet_dir, filename)
        
        # 如果文件已存在，跳过
        if os.path.exists(save_path):
            return {
                'component_id': component_id,
                'url': url,
                'path': save_path,
                'status': 'exists'
            }
        
        return self._download_file(url, save_path)
    
    def _download_image(self, component_id: str, manufacturer_part: str, image_url: str, index: int) -> dict:
        """下载图片，文件名格式: Manufacturer Part_序号.扩展名"""
        # 使用 Manufacturer Part 作为前缀
        safe_name = self._sanitize_filename(manufacturer_part) if manufacturer_part else component_id
        ext = self._get_extension_from_url(image_url)
        filename = f"{safe_name}_{index}{ext}"
        save_path = os.path.join(self.imgs_dir, filename)
        
        # 如果文件已存在，跳过
        if os.path.exists(save_path):
            return {
                'component_id': component_id,
                'url': image_url,
                'path': save_path,
                'status': 'exists'
            }
        
        return self._download_file(image_url, save_path)
    
    def download_all(self, data: dict) -> dict:
        """下载所有文件"""
        # 收集所有下载任务
        tasks = []
        
        for component_id, info in data.items():
            if info.get('status') != 'success':
                continue
            
            manufacturer_part = info.get('manufacturer_part', component_id)
            
            # 下载 datasheet
            datasheet_url = info.get('datasheet')
            if datasheet_url:
                tasks.append(('datasheet', component_id, manufacturer_part, datasheet_url, 0))
            
            # 下载图片
            images = info.get('images', [])
            for i, img_url in enumerate(images):
                tasks.append(('image', component_id, manufacturer_part, img_url, i + 1))
        
        print(f"开始下载 {len(tasks)} 个文件...")
        print(f"  - Datasheet: {sum(1 for t in tasks if t[0] == 'datasheet')} 个")
        print(f"  - 图片: {sum(1 for t in tasks if t[0] == 'image')} 个")
        print(f"使用 {self.max_workers} 个线程并行下载\n")
        
        # 执行下载
        completed = 0
        success_count = 0
        error_count = 0
        exists_count = 0
        
        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            future_to_task = {}
            
            for task in tasks:
                task_type = task[0]
                component_id = task[1]
                manufacturer_part = task[2]
                url = task[3]
                index = task[4] if len(task) > 4 else 0
                
                if task_type == 'datasheet':
                    future = executor.submit(self._download_datasheet, component_id, manufacturer_part, url)
                else:
                    future = executor.submit(self._download_image, component_id, manufacturer_part, url, index)
                
                future_to_task[future] = (task_type, component_id, url)
            
            for future in as_completed(future_to_task):
                task_type, component_id, url = future_to_task[future]
                completed += 1
                
                try:
                    result = future.result()
                    self.results[f"{component_id}_{task_type}_{url}"] = result
                    
                    if result['status'] == 'success':
                        success_count += 1
                        print(f"[{completed}/{len(tasks)}] ✓ {component_id} {task_type}: {result['path']} ({result.get('size', 0)} bytes)")
                    elif result['status'] == 'exists':
                        exists_count += 1
                        print(f"[{completed}/{len(tasks)}] = {component_id} {task_type}: 已存在")
                    else:
                        error_count += 1
                        print(f"[{completed}/{len(tasks)}] ✗ {component_id} {task_type}: {result.get('error', 'Unknown error')}")
                        
                except Exception as e:
                    error_count += 1
                    print(f"[{completed}/{len(tasks)}] ✗ {component_id} {task_type}: 异常 - {e}")
                
                # 短暂延迟避免被限流
                time.sleep(random.uniform(0.05, 0.15))
        
        print(f"\n{'='*60}")
        print(f"下载完成！")
        print(f"成功: {success_count}")
        print(f"已存在: {exists_count}")
        print(f"失败: {error_count}")
        print(f"总计: {len(tasks)}")
        
        return self.results
    
    def save_results(self, filename: str = 'download_results.json'):
        """保存下载结果"""
        with open(filename, 'w', encoding='utf-8') as f:
            json.dump(self.results, f, ensure_ascii=False, indent=2)
        print(f"\n下载结果已保存到: {filename}")


if __name__ == "__main__":
    # 读取数据
    with open('lceda_results.json', 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    print(f"读取到 {len(data)} 个元器件数据\n")
    
    # 创建下载器，使用 5 个线程
    downloader = FileDownloader(max_workers=5)
    
    # 下载所有文件
    results = downloader.download_all(data)
    
    # 保存结果
    downloader.save_results('download_results.json')
    
    # 统计
    datasheet_count = len([r for r in results.values() if 'datasheet' in r.get('path', '')])
    image_count = len([r for r in results.values() if 'imgs' in r.get('path', '')])
    
    print(f"\n文件统计:")
    print(f"  datasheet 目录: {datasheet_count} 个文件")
    print(f"  imgs 目录: {image_count} 个文件")
