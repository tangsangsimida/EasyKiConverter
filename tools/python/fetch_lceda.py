import requests
import json
import time
import random
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed

class LCEImageFetcher:
    def __init__(self, max_workers=5, output_dir: str = "."):
        self.max_workers = max_workers
        self.results = {}
        self.output_dir = Path(output_dir)
        
        # 【优化点】确保响应保存目录存在
        self.response_dir = self.output_dir / "response"
        self.response_dir.mkdir(parents=True, exist_ok=True)
        
    def _search_product(self, keyword: str) -> dict:
        url = "https://pro.lceda.cn/api/v2/eda/product/search"
        payload = {
            "keyword": keyword,
            "currPage": 1,
            "pageSize": 50,
            "needAggs": "true"
        }
        headers = {
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36",
            "Accept": "application/json, text/javascript, */*; q=0.01",
            "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
            "X-Requested-With": "XMLHttpRequest",
        }
        
        try:
            response = requests.post(url, data=payload, headers=headers, timeout=5)
            response.raise_for_status()
            data = response.json()
            
            # 保存原始响应 (现在目录已确保存在，不会报错)
            resp_file = self.response_dir / f"{payload.get('keyword')}_response.json"
            with open(resp_file, 'w', encoding='utf-8') as f:
                json.dump(data, f, ensure_ascii=False, indent=2)
            
            result = data.get('result', {})
            product_list = result.get('productList', [])
            
            if product_list and len(product_list) > 0:
                product = product_list[0]
                device_info = product.get('device_info', {})
                attributes = device_info.get('attributes', {})
                
                manufacturer_part = attributes.get('Manufacturer Part')
                datasheet = attributes.get('Datasheet')
                
                images_str = product.get('image', '')
                image_list = images_str.split('<$>') if images_str else []
                
                return {
                    'keyword': keyword,
                    'code': product.get('code'),
                    'name': product.get('name'),
                    'manufacturer_part': manufacturer_part,
                    'datasheet': datasheet,
                    'images': image_list,
                    'status': 'success'
                }
            else:
                return {'keyword': keyword, 'status': 'not_found'}
                
        except requests.exceptions.RequestException as e:
            return {'keyword': keyword, 'status': 'error', 'error': str(e)}
        except json.JSONDecodeError:
            return {'keyword': keyword, 'status': 'error', 'error': 'Invalid JSON response'}
        except Exception as e:
            return {'keyword': keyword, 'status': 'error', 'error': str(e)}
    
    def fetch_all(self, keywords: list) -> dict:
        print(f"开始获取 {len(keywords)} 个元器件的数据... (线程数: {self.max_workers})")
        
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
                    
                    if result['status'] == 'success':
                        img_count = len(result.get('images', []))
                        has_ds = '✓' if result.get('datasheet') else '✗'
                        name_display = (result.get('name') or 'N/A')[:20]
                        print(f"[{completed}/{len(keywords)}] {keyword} | {name_display}... | 图:{img_count} | DS:{has_ds}")
                    elif result['status'] == 'not_found':
                        print(f"[{completed}/{len(keywords)}] {keyword} | 未找到")
                    else:
                        print(f"[{completed}/{len(keywords)}] {keyword} | 错误: {result.get('error', 'Unknown')}")
                        
                except Exception as e:
                    self.results[keyword] = {'keyword': keyword, 'status': 'error', 'error': str(e)}
                    print(f"[{completed}/{len(keywords)}] {keyword} | 异常: {e}")
                
                time.sleep(random.uniform(0.1, 0.3))
        
        return self.results
    
    def save_results(self, filename: str = 'lceda_results.json'):
        path = Path(filename)
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(self.results, f, ensure_ascii=False, indent=2)
        print(f"\n结果已保存至: {path.resolve()}")

if __name__ == "__main__":
    component_ids = [
        "C8734", "C52717", "C8323", "C28730", "C35556", "C724040",
        "C432211", "C14877", "C12345", "C2040", "C915663", "C33993",
        "C2858491", "C19156", "C80713", "C13622", "C60420", "C507118",
        "C3018718", "C414042", "C7420375"
    ]
    
    unique_ids = list(set(component_ids))
    fetcher = LCEImageFetcher(max_workers=10)
    
    try:
        results = fetcher.fetch_all(unique_ids)
        fetcher.save_results('lceda_results.json')
        
        # 简单统计
        success = sum(1 for r in results.values() if r['status'] == 'success')
        print(f"\n总览: 成功 {success}/{len(results)}")
    except KeyboardInterrupt:
        print("\n用户中断执行。")