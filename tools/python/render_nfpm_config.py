#!/usr/bin/env python3
"""Render nfpm config with repository-local absolute paths."""

from __future__ import annotations

import argparse
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Render deploy/nfpm.yaml for packaging")
    parser.add_argument("--template", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--project-root", required=True, type=Path)
    parser.add_argument("--app-dir", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--arch", required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    project_root = args.project_root.resolve()
    app_dir = args.app_dir.resolve()

    replacements = {
        "${APP_DIR}": str(app_dir),
        "${VERSION}": args.version,
        "${NFPM_ARCH}": args.arch,
        "deploy/scripts/postinstall.sh": str(project_root / "deploy/scripts/postinstall.sh"),
        "deploy/scripts/prerm.sh": str(project_root / "deploy/scripts/prerm.sh"),
        "deploy/scripts/postrm.sh": str(project_root / "deploy/scripts/postrm.sh"),
        "deploy/scripts/uninstall-common.sh": str(project_root / "deploy/scripts/uninstall-common.sh"),
        "deploy/scripts/easykiconverter-wrapper.sh": str(project_root / "deploy/scripts/easykiconverter-wrapper.sh"),
        "deploy/scripts/easykiconverter-register.sh": str(project_root / "deploy/scripts/easykiconverter-register.sh"),
        "deploy/scripts/easykiconverter-register.desktop": str(project_root / "deploy/scripts/easykiconverter-register.desktop"),
        "deploy/scripts/trigger-gnome-refresh.sh": str(project_root / "deploy/scripts/trigger-gnome-refresh.sh"),
        "deploy/metainfo/io.github.tangsangsimida.easykiconverter.metainfo.xml": str(
            project_root / "deploy/metainfo/io.github.tangsangsimida.easykiconverter.metainfo.xml"
        ),
        "deploy/linux/io.github.tangsangsimida.easykiconverter.desktop": str(
            project_root / "deploy/linux/io.github.tangsangsimida.easykiconverter.desktop"
        ),
    }

    content = args.template.read_text(encoding="utf-8")
    for old, new in replacements.items():
        content = content.replace(old, new)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(content, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
