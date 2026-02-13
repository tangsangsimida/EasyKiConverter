"""
翻译上下文修复工具
用途：将指定 QML 文件中的 qsTr(...) 替换为 qsTranslate("上下文", ...)
用法：python tools/python/fix_qml_translations.py <context> <file1.qml> [file2.qml ...]
示例：python tools/python/fix_qml_translations.py MainWindow src/ui/qml/components/HeaderSection.qml
"""
import os
import sys

def fix_translations(context, filepaths):
    for filepath in filepaths:
        if not os.path.exists(filepath):
            print(f"File not found: {filepath}")
            continue

        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        old = 'qsTr('
        new = f'qsTranslate("{context}", '

        count = content.count(old)
        if count == 0:
            print(f"No qsTr() found: {filepath}")
            continue

        new_content = content.replace(old, new)
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)

        print(f"Updated {filepath}: {count} replacements")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python fix_qml_translations.py <context> <file1.qml> [file2.qml ...]")
        print('Example: python fix_qml_translations.py MainWindow src/ui/qml/components/MyCard.qml')
        sys.exit(1)

    context = sys.argv[1]
    files = sys.argv[2:]
    fix_translations(context, files)
