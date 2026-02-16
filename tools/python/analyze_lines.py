"""
代码行数分析工具
用途：扫描项目源文件，按行数降序排列，识别高耦合风险文件。
用法：python tools/python/analyze_lines.py [目录路径] [--link]

选项:
    --link    启用终端超链接，点击文件路径可用默认程序打开
"""
import os
import sys


def detect_hyperlink_support():
    """
    检测当前终端是否支持 OSC 8 超链接

    支持的终端:
    - Windows Terminal
    - VS Code 终端
    - JetBrains 终端 (PyCharm, CLion 等)
    - iTerm2 (macOS)
    - GNOME Terminal
    - Kitty

    不支持的终端:
    - cmd.exe
    - PowerShell 5.1 及以下
    - ConEmu (默认配置)
    """
    # 检查常见支持 OSC 8 的终端环境变量
    term_program = os.environ.get('TERM_PROGRAM', '').lower()
    wt_session = os.environ.get('WT_SESSION')
    vscode_term = os.environ.get('VSCODE_GIT_IPC_HANDLE')
    jetbrains_terminal = os.environ.get('TERMINAL_EMULATOR', '').lower()

    # Windows Terminal
    if wt_session:
        return True
    # VS Code 终端
    if vscode_term:
        return True
    # iTerm2, GNOME Terminal, Kitty 等
    if term_program in ('iterm.app', 'gnome-terminal', 'kitty'):
        return True
    # JetBrains 终端
    if 'jetbrains' in jetbrains_terminal:
        return True

    return False


def make_hyperlink(path, text, base_dir):
    r"""
    生成 OSC 8 超链接

    OSC 8 格式: ESC ] 8 ; ; URL ESC \ TEXT ESC ] 8 ; ; ESC \

    参数:
        path: 相对路径
        text: 显示的文本
        base_dir: 基础目录，用于构建绝对路径

    返回:
        包含 OSC 8 转义序列的字符串，在支持的终端中可点击
    """
    abs_path = os.path.abspath(os.path.join(base_dir, path))
    # Windows 路径转 file:// URL（统一使用正斜杠）
    if os.name == 'nt':
        # Windows: C:\path -> file:///C:/path
        file_url = 'file:///' + abs_path.replace(os.sep, '/')
    else:
        # Unix: /path -> file:///path
        file_url = 'file://' + abs_path

    # OSC 8 转义序列
    # ESC = \033 (八进制) 或 \x1b (十六进制)
    osc_start = '\033]8;;'
    osc_end = '\033\\'

    return f'{osc_start}{file_url}{osc_end}{text}{osc_start}{osc_end}'


def analyze(src_dir, base_dir=None, use_links=False):
    """
    分析源文件行数

    参数:
        src_dir: 要扫描的源目录
        base_dir: 项目根目录，用于计算相对路径
        use_links: 是否启用终端超链接
    """
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

        # 启用超链接时，将路径转换为可点击链接
        display_path = make_hyperlink(path, path, base_dir) if use_links else path
        print(f"{lines:>6}  {display_path}{marker}")

    print(f"\n{'='*80}")
    print(f"Total files: {len(results)}")
    print(f"Files > 500 lines (High risk):   {sum(1 for l,_ in results if l > 500)}")
    print(f"Files > 300 lines (Medium risk): {sum(1 for l,_ in results if l > 300)}")
    print(f"Files > 200 lines (Low risk):    {sum(1 for l,_ in results if l > 200)}")
    print(f"Total lines:                     {sum(l for l,_ in results)}")


def main():
    base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    # 解析命令行参数
    use_links = '--link' in sys.argv
    args = [arg for arg in sys.argv[1:] if arg != '--link']

    if len(args) > 0:
        src_dir = args[0]
    else:
        src_dir = os.path.join(base_dir, "src")

    # 如果指定了 --link 但终端不支持，给出警告
    if use_links and not detect_hyperlink_support():
        print("⚠ 警告: 当前终端可能不支持 OSC 8 超链接", file=sys.stderr)
        print("  支持的终端: Windows Terminal, VS Code, JetBrains IDE, iTerm2 等", file=sys.stderr)
        print()

    analyze(src_dir, base_dir, use_links=use_links)


if __name__ == "__main__":
    main()
