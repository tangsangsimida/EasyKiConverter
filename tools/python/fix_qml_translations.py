"""
翻译上下文修复工具 - 将 QML 文件中的 qsTr() 替换为 qsTranslate()
"""
import argparse
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


def main():
    parser = argparse.ArgumentParser(
        description="翻译上下文修复工具 - 将 QML 文件中的 qsTr() 替换为 qsTranslate()",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
功能说明:
  将指定 QML 文件中的 qsTr("text") 替换为 qsTranslate("Context", "text")
  这有助于 Qt 翻译系统区分不同组件中的相同文本

示例:
  python tools/python/fix_qml_translations.py MainWindow src/ui/qml/components/HeaderSection.qml
  python tools/python/fix_qml_translations.py MyCard src/ui/qml/components/MyCard.qml src/ui/qml/components/OtherCard.qml
        """
    )
    parser.add_argument(
        "context",
        help="翻译上下文名称 (如 MainWindow, MyCard 等)"
    )
    parser.add_argument(
        "files",
        nargs="+",
        help="要处理的 QML 文件路径 (支持多个文件)"
    )

    args = parser.parse_args()
    fix_translations(args.context, args.files)


if __name__ == "__main__":
    main()
