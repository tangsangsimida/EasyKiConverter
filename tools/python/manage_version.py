import argparse
import json
import re
import os
import sys

# 文件路径配置 (相对于项目根目录)
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
VCPKG_JSON_PATH = os.path.join(PROJECT_ROOT, "vcpkg.json")
CMAKE_LISTS_PATH = os.path.join(PROJECT_ROOT, "CMakeLists.txt")
MAIN_CPP_PATH = os.path.join(PROJECT_ROOT, "src", "main.cpp")

def get_current_version():
    """
    从 vcpkg.json 读取当前版本号

    Requirements:
    - 必须在项目 tools/python 目录下运行，或通过其完整路径调用
    - 依赖于根目录下的 vcpkg.json 作为版本源
    """
    if not os.path.exists(VCPKG_JSON_PATH):
        print(f"错误: 找不到文件 {VCPKG_JSON_PATH}")
        return None

    try:
        with open(VCPKG_JSON_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
            return data.get("version-string")
    except Exception as e:
        print(f"读取 vcpkg.json 失败: {e}")
        return None

def update_vcpkg_json(new_version):
    """更新 vcpkg.json 中的版本号"""
    print(f"正在更新 {VCPKG_JSON_PATH}...")
    try:
        with open(VCPKG_JSON_PATH, "r", encoding="utf-8") as f:
            content = f.read()

        # 使用正则替换，保留原有格式
        # "version-string": "3.0.5",
        pattern = r'("version-string":\s*")([^"]+)(")'
        replacement = f'\\g<1>{new_version}\\g<3>'

        new_content = re.sub(pattern, replacement, content)

        if content == new_content:
            print(f"警告: {VCPKG_JSON_PATH} 未发生变化 (可能版本号未变或格式不匹配)")
        else:
            with open(VCPKG_JSON_PATH, "w", encoding="utf-8") as f:
                f.write(new_content)
            print(f"成功更新 {VCPKG_JSON_PATH}")
            return True
    except Exception as e:
        print(f"更新 {VCPKG_JSON_PATH} 失败: {e}")
    return False

def update_cmake_lists(new_version):
    """更新 CMakeLists.txt 中的版本号"""
    print(f"正在更新 {CMAKE_LISTS_PATH}...")
    try:
        with open(CMAKE_LISTS_PATH, "r", encoding="utf-8") as f:
            content = f.read()

        # 1. 更新 VERSION_FROM_CI默认值
        # set(VERSION_FROM_CI "0.1")
        pattern1 = r'(set\(VERSION_FROM_CI\s+")([^"]+)("\))'
        replacement1 = f'\\g<1>{new_version}\\g<3>'
        content = re.sub(pattern1, replacement1, content)

        # 2. 更新 qt_add_qml_module 中的 VERSION (只保留 Major.Minor)
        # VERSION 1.0
        major_minor = ".".join(new_version.split(".")[:2])
        pattern2 = r'(qt_add_qml_module\(appEasyKiconverter_Cpp_Version.*?VERSION\s+)(\d+\.\d+)(.*?\))'
        # 注意：这里需要使用 re.DOTALL 因为 qt_add_qml_module跨越多行
        # 但简单的逐行正则可能更安全，或者寻找特定上下文
        # 简单起见，我们假设 VERSION 1.0 是独立的一行或者容易匹配
        # 实际上 qt_add_qml_module 内部的 VERSION 参数通常是 VERSION X.Y
        # 我们只替换 VERSION 之后的数字

        # 更精确的匹配：找到 qt_add_qml_module 块，然后替换里面的 VERSION
        # 鉴于文件结构，我们可以尝试直接替换 `VERSION 1.0` 如果它在 qt_add_qml_module 附近
        # 但为了避免误伤 project(VERSION ...)，我们需要小心

        # 策略：直接读取文件行，找到 qt_add_qml_module 开始，然后在其中找 VERSION
        lines = content.splitlines()
        in_qml_module = False
        new_lines = []
        for line in lines:
            if "qt_add_qml_module(appEasyKiconverter_Cpp_Version" in line:
                in_qml_module = True

            if in_qml_module and "VERSION" in line and not "MACOSX_BUNDLE_BUNDLE_VERSION" in line:
                # 替换 VERSION 1.0 -> VERSION X.Y
                # 假设格式为 "    VERSION 1.0"
                line = re.sub(r'(VERSION\s+)(\d+\.\d+)', f'\\g<1>{major_minor}', line)
                in_qml_module = False # 假设 VERSION 只有一行，或者我们只替换一次

            if ")" in line and in_qml_module:
                 # 简单的结束判断，可能不准确但对于标准格式足够
                 pass

            new_lines.append(line)

        new_content = "\n".join(new_lines) + "\n" # 保持末尾换行

        # 上面的循环逻辑有点复杂且可能脆弱，我们用简单的正则替换尝试一下
        # 针对: set(VERSION_FROM_CI "0.1") 已由 pattern1 处理

        # 针对 QML VERSION，考虑到它通常是 Major.Minor
        # 我们可以只更新 pattern1，QML VERSION 如果是固定的 1.0 可能不需要每次都动，
        # 除非我们想让 QML 模块版本也跟进。
        # 之前的 plan 说 "update VERSION 1.0 in qt_add_qml_module to the new version (major.minor)"
        # 所以我们必须做。

        # 重新读取 content (因为 splitlines 处理比较复杂，我们用正则再试一次)
        # 我们已经用 pattern1 替换了一部分

        pattern_qml = r'(VERSION\s+)(\d+\.\d+)(\s+QML_FILES)'
        # 这要求 VERSION 和 QML_FILES 在同一行或者我们可以匹配多行
        # 让我们再看一眼原文件:
        # qt_add_qml_module(appEasyKiconverter_Cpp_Version
        #     URI EasyKiconverter_Cpp_Version
        #     VERSION 1.0
        #     QML_FILES

        # 用 replace 直接替换特定字符串可能更安全，如果上下文允许
        # 但 "VERSION 1.0" 太通用了。

        # 让我们用 regex 匹配多行
        pattern_qml_block = r'(qt_add_qml_module\(.*?VERSION\s+)(\d+\.\d+)'
        new_content = re.sub(pattern_qml_block, f'\\g<1>{major_minor}', content, flags=re.DOTALL)

        if content == new_content and new_version not in content:
             print(f"警告: {CMAKE_LISTS_PATH} update可能不完整")

        with open(CMAKE_LISTS_PATH, "w", encoding="utf-8") as f:
            f.write(new_content)
        print(f"成功更新 {CMAKE_LISTS_PATH}")
        return True

    except Exception as e:
        print(f"更新 {CMAKE_LISTS_PATH} 失败: {e}")
        import traceback
        traceback.print_exc()
    return False

def update_main_cpp(new_version):
    """更新 src/main.cpp 中的版本号"""
    print(f"正在更新 {MAIN_CPP_PATH}...")
    try:
        with open(MAIN_CPP_PATH, "r", encoding="utf-8") as f:
            content = f.read()

        # app.setApplicationVersion("3.0.2");
        pattern = r'(app\.setApplicationVersion\(")([^"]+)("\);)'
        replacement = f'\\g<1>{new_version}\\g<3>'

        new_content = re.sub(pattern, replacement, content)

        if content == new_content:
            print(f"警告: {MAIN_CPP_PATH} 未发生变化")
        else:
            with open(MAIN_CPP_PATH, "w", encoding="utf-8") as f:
                f.write(new_content)
            print(f"成功更新 {MAIN_CPP_PATH}")
            return True
    except Exception as e:
        print(f"更新 {MAIN_CPP_PATH} 失败: {e}")
    return False

def main():
    parser = argparse.ArgumentParser(description="EasyKiConverter 版本管理工具")
    parser.add_argument("version", nargs="?", help="新的版本号 (例如 3.0.6)")
    parser.add_argument("--check", action="store_true", help="仅检查当前版本")

    args = parser.parse_args()

    current_version = get_current_version()
    print(f"当前版本 (vcpkg.json): {current_version}")

    if args.check:
        return

    if not args.version:
        print("请输入新的版本号，例如: python manage_version.py 3.0.6")
        return

    new_version = args.version
    if not re.match(r'^\d+\.\d+\.\d+$', new_version):
        print("错误: 版本号格式必须为 X.Y.Z (例如 3.0.6)")
        return

    print(f"准备将版本更新为: {new_version}")

    if update_vcpkg_json(new_version):
        update_cmake_lists(new_version)
        update_main_cpp(new_version)
        print("\n所有文件更新完成！")
    else:
        print("\n更新中断。")

if __name__ == "__main__":
    main()
