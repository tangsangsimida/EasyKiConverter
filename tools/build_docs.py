#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
文档构建脚本 - EasyKiConverter

功能：
1. 生成 Doxygen API 文档
2. 构建 MkDocs 静态站点
3. 本地预览文档

使用方法：
    python tools/build_docs.py --all          # 构建所有文档
    python tools/build_docs.py --doxygen      # 仅生成 Doxygen
    python tools/build_docs.py --mkdocs       # 仅构建 MkDocs
    python tools/build_docs.py --serve        # 本地预览
    python tools/build_docs.py --clean        # 清理生成的文档
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def get_project_root():
    """获取项目根目录"""
    return Path(__file__).parent.parent.resolve()


def check_command(cmd):
    """检查命令是否可用"""
    try:
        subprocess.run([cmd, "--version"], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def check_prerequisites():
    """检查前提条件"""
    missing = []
    
    if not check_command("doxygen"):
        missing.append("doxygen")
    
    if not check_command("mkdocs"):
        missing.append("mkdocs (pip install mkdocs-material)")
    
    if missing:
        print("❌ 缺少以下工具：")
        for tool in missing:
            print(f"   - {tool}")
        print("\n安装指南：")
        print("  Windows: choco install doxygen graphviz")
        print("  macOS:   brew install doxygen graphviz")
        print("  Linux:   sudo apt install doxygen graphviz")
        print("  Python:  pip install mkdocs-material mkdocs-macros-plugin mkdocs-minify-plugin")
        return False
    
    return True


def run_doxygen(project_root):
    """运行 Doxygen 生成 API 文档"""
    print("📚 生成 Doxygen API 文档...")
    
    doxyfile = project_root / "Doxyfile"
    api_dir = project_root / "docs" / "api"
    
    if not doxyfile.exists():
        print(f"❌ 找不到 Doxyfile: {doxyfile}")
        return False
    
    # 确保 api 目录存在
    api_dir.mkdir(parents=True, exist_ok=True)
    
    try:
        result = subprocess.run(
            ["doxygen", str(doxyfile)],
            cwd=project_root,
            capture_output=True,
            text=True
        )
        
        if result.returncode != 0:
            print(f"❌ Doxygen 执行失败：")
            print(result.stderr)
            return False
        
        # 检查警告日志
        warning_log = api_dir / "doxygen_warnings.log"
        if warning_log.exists() and warning_log.stat().st_size > 0:
            print(f"⚠️  Doxygen 有警告，请查看: {warning_log}")
        
        html_index = api_dir / "html" / "index.html"
        if html_index.exists():
            print(f"✅ Doxygen 文档生成成功: {html_index}")
            return True
        else:
            print("❌ Doxygen 输出文件未生成")
            return False
            
    except Exception as e:
        print(f"❌ Doxygen 执行异常: {e}")
        return False


def run_mkdocs_build(project_root):
    """构建 MkDocs 站点"""
    print("📖 构建 MkDocs 站点...")
    
    try:
        result = subprocess.run(
            ["mkdocs", "build", "--clean", "--strict"],
            cwd=project_root,
            capture_output=True,
            text=True
        )
        
        if result.returncode != 0:
            print(f"❌ MkDocs 构建失败：")
            print(result.stderr)
            return False
        
        site_dir = project_root / "site"
        print(f"✅ MkDocs 站点构建成功: {site_dir}")
        return True
        
    except Exception as e:
        print(f"❌ MkDocs 构建异常: {e}")
        return False


def serve_mkdocs(project_root):
    """启动 MkDocs 本地服务器"""
    print("🌐 启动 MkDocs 本地服务器...")
    print("   访问地址: http://127.0.0.1:8000")
    print("   按 Ctrl+C 停止服务\n")
    
    try:
        subprocess.run(
            ["mkdocs", "serve"],
            cwd=project_root
        )
    except KeyboardInterrupt:
        print("\n✅ 服务器已停止")
    except Exception as e:
        print(f"❌ 启动服务器失败: {e}")


def clean_docs(project_root):
    """清理生成的文档"""
    print("🧹 清理生成的文档...")
    
    dirs_to_clean = [
        project_root / "docs" / "api" / "html",
        project_root / "docs" / "api" / "latex",
        project_root / "docs" / "api" / "xml",
        project_root / "site",
    ]
    
    files_to_clean = [
        project_root / "docs" / "api" / "doxygen_warnings.log",
        project_root / "docs" / "api" / "easykiconverter.tag",
    ]
    
    for dir_path in dirs_to_clean:
        if dir_path.exists():
            shutil.rmtree(dir_path)
            print(f"   删除目录: {dir_path}")
    
    for file_path in files_to_clean:
        if file_path.exists():
            file_path.unlink()
            print(f"   删除文件: {file_path}")
    
    print("✅ 清理完成")


def main():
    parser = argparse.ArgumentParser(
        description="EasyKiConverter 文档构建工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python tools/build_docs.py --all        构建所有文档
  python tools/build_docs.py --doxygen    仅生成 Doxygen API 文档
  python tools/build_docs.py --mkdocs     仅构建 MkDocs 站点
  python tools/build_docs.py --serve      本地预览文档
  python tools/build_docs.py --clean      清理生成的文档
        """
    )
    
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--all", action="store_true", help="构建所有文档")
    group.add_argument("--doxygen", action="store_true", help="仅生成 Doxygen")
    group.add_argument("--mkdocs", action="store_true", help="仅构建 MkDocs")
    group.add_argument("--serve", action="store_true", help="本地预览文档")
    group.add_argument("--clean", action="store_true", help="清理生成的文档")
    group.add_argument("--check", action="store_true", help="检查环境依赖")
    
    args = parser.parse_args()
    
    project_root = get_project_root()
    
    if args.check:
        print("🔍 检查环境依赖...\n")
        print(f"项目根目录: {project_root}\n")
        
        tools = {
            "doxygen": check_command("doxygen"),
            "mkdocs": check_command("mkdocs"),
            "dot (graphviz)": check_command("dot"),
        }
        
        all_ok = True
        for tool, available in tools.items():
            status = "✅" if available else "❌"
            print(f"  {status} {tool}")
            if not available:
                all_ok = False
        
        print()
        if all_ok:
            print("✅ 所有依赖已安装")
        else:
            print("❌ 部分依赖缺失，请安装后重试")
        return 0 if all_ok else 1
    
    if args.serve:
        serve_mkdocs(project_root)
        return 0
    
    if args.clean:
        clean_docs(project_root)
        return 0
    
    # 检查前提条件
    if not check_prerequisites():
        return 1
    
    success = True
    
    if args.all:
        if not run_doxygen(project_root):
            success = False
        if not run_mkdocs_build(project_root):
            success = False
    elif args.doxygen:
        if not run_doxygen(project_root):
            success = False
    elif args.mkdocs:
        if not run_mkdocs_build(project_root):
            success = False
    
    if success:
        print("\n🎉 文档构建完成！")
    else:
        print("\n❌ 文档构建失败")
    
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
