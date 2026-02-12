"""
代码行数分析工具
用途：扫描项目源文件，按行数降序排列，识别高耦合风险文件。
用法：python tools/python/analyze_lines.py [目录路径]
"""
import os
import sys

def analyze(src_dir, base_dir=None):
    if base_dir is None:
        base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    extensions = ('.cpp', '.h', '.qml')
    results = []

    for root, dirs, files in os.walk(src_dir):
        for f in files:
            if f.endswith(extensions):
                filepath = os.path.join(root, f)
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as fh:
                    lines = sum(1 for _ in fh)
                rel = os.path.relpath(filepath, base_dir)
                results.append((lines, rel))

    results.sort(key=lambda x: -x[0])

    print(f"{'Lines':>6}  {'File'}")
    print("-" * 80)
    for lines, path in results:
        if lines > 500:
            marker = " 🔴"
        elif lines > 300:
            marker = " 🟡"
        elif lines > 200:
            marker = " 🟢"
        else:
            marker = ""
        print(f"{lines:>6}  {path}{marker}")

    print(f"\n{'='*80}")
    print(f"Total files: {len(results)}")
    print(f"Files > 500 lines (High risk):   {sum(1 for l,_ in results if l > 500)}")
    print(f"Files > 300 lines (Medium risk): {sum(1 for l,_ in results if l > 300)}")
    print(f"Files > 200 lines (Low risk):    {sum(1 for l,_ in results if l > 200)}")
    print(f"Total lines:                     {sum(l for l,_ in results)}")

if __name__ == "__main__":
    base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if len(sys.argv) > 1:
        src_dir = sys.argv[1]
    else:
        src_dir = os.path.join(base_dir, "src")

    analyze(src_dir, base_dir)
