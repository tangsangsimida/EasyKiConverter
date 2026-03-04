import requests
import json
import time
import random
from concurrent.futures import ThreadPoolExecutor, as_completed


class LCEImageFetcher:
    def __init__(self, max_workers=5):
        self.max_workers = max_workers
        self.results = {}
        
    def _search_product(self, keyword: str) -> dict:
        """搜索产品"""
        url = "https://pro.lceda.cn/api/v2/eda/product/search"
        
        payload = {
            "keyword": keyword,
            "currPage": 1,
            "pageSize": 50,
            # "path": "0819f05c4eef4c71ace90d822a990e87",
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
            with open(f'./respose/{payload.get("keyword")}_response.json', 'w', encoding='utf-8') as f:
                json.dump(data, f, ensure_ascii=False, indent=2)
            result = data.get('result', {})
            product_list = result.get('productList', [])
            
            if product_list and len(product_list) > 0:
                product = product_list[0]
                
                manufacturer_part = None
                datasheet = None
                if product.get('device_info') and product['device_info'].get('attributes'):
                    attrs = product['device_info']['attributes']
                    manufacturer_part = attrs.get('Manufacturer Part')
                    datasheet = attrs.get('Datasheet')
                
                images = product.get('image', '')
                image_list = images.split('<$>') if images else []
                
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
                return {
                    'keyword': keyword,
                    'status': 'not_found'
                }
                
        except requests.exceptions.RequestException as e:
            return {
                'keyword': keyword,
                'status': 'error',
                'error': str(e)
            }
    
    def fetch_all(self, keywords: list) -> dict:
        """并行批量获取"""
        print(f"开始获取 {len(keywords)} 个元器件的数据...")
        print(f"使用 {self.max_workers} 个线程并行获取\n")
        
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
                        print(f"[{completed}/{len(keywords)}] {keyword} ✓ 名称: {result.get('name', 'N/A')[:30]}... | 图片: {img_count} | DS: {has_ds}")
                    elif result['status'] == 'not_found':
                        print(f"[{completed}/{len(keywords)}] {keyword} ✗ 未找到")
                    else:
                        print(f"[{completed}/{len(keywords)}] {keyword} ✗ 错误: {result.get('error', 'Unknown')}")
                        
                except Exception as e:
                    self.results[keyword] = {'keyword': keyword, 'status': 'error', 'error': str(e)}
                    print(f"[{completed}/{len(keywords)}] {keyword} ✗ 异常: {e}")
                
                time.sleep(random.uniform(0.1, 0.3))
        
        return self.results
    
    def save_results(self, filename: str = 'lceda_results.json'):
        """保存结果到 JSON 文件"""
        with open(filename, 'w', encoding='utf-8') as f:
            json.dump(self.results, f, ensure_ascii=False, indent=2)
        print(f"\n结果已保存到: {filename}")
    
    def print_summary(self):
        """打印汇总信息"""
        success = sum(1 for r in self.results.values() if r['status'] == 'success')
        not_found = sum(1 for r in self.results.values() if r['status'] == 'not_found')
        error = sum(1 for r in self.results.values() if r['status'] == 'error')
        
        print(f"\n{'='*60}")
        print(f"获取完成！")
        print(f"成功: {success}")
        print(f"未找到: {not_found}")
        print(f"错误: {error}")
        print(f"总计: {len(self.results)}")


if __name__ == "__main__":
    # 元器件列表
    component_ids = [
        "C8734", "C52717", "C8323", "C28730", "C35556", "C724040",
        "C432211", "C14877", "C12345", "C2040", "C915663", "C33993",
        "C2858491", "C19156", "C80713", "C13622", "C60420", "C507118",
        "C3018718", "C414042", "C7420375", "C432211", "C2040"
    ]
    
    # 去重
    unique_ids = list(set(component_ids))
    
    # 创建 fetcher，使用 10 个线程
    fetcher = LCEImageFetcher(max_workers=10)
    
    # 获取所有数据
    results = fetcher.fetch_all(unique_ids)
    
    # 打印汇总
    fetcher.print_summary()
    
    # 保存结果
    fetcher.save_results('lceda_results.json')
    