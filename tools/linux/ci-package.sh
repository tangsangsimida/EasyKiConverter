#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

usage() {
    cat <<'EOF'
Usage:
  ci-package.sh install-linuxdeploy <x86_64|aarch64>
  ci-package.sh fix-permissions <appdir>
  ci-package.sh package-appimage <appdir> <product> <version> <git_hash> <appimage_arch>
  ci-package.sh package-nfpm <deb|rpm|archlinux> <appdir> <product> <version> <git_hash> <nfpm_arch> <output_arch> <extension>
EOF
}

install_linuxdeploy() {
    local linuxdeploy_arch="$1"

    sudo mkdir -p /opt/linuxdeploy /opt/linuxdeploy-plugin-qt

    wget -q -O /tmp/linuxdeploy \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${linuxdeploy_arch}.AppImage"
    chmod +x /tmp/linuxdeploy
    /tmp/linuxdeploy --appimage-extract
    sudo cp -r squashfs-root/* /opt/linuxdeploy/
    rm -rf squashfs-root
    sudo ln -sf /opt/linuxdeploy/AppRun /usr/local/bin/linuxdeploy

    wget -q -O /tmp/linuxdeploy-plugin-qt \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${linuxdeploy_arch}.AppImage"
    chmod +x /tmp/linuxdeploy-plugin-qt
    /tmp/linuxdeploy-plugin-qt --appimage-extract
    sudo cp -r squashfs-root/* /opt/linuxdeploy-plugin-qt/
    rm -rf squashfs-root
    sudo ln -sf /opt/linuxdeploy-plugin-qt/AppRun /usr/local/bin/linuxdeploy-plugin-qt

    which linuxdeploy || echo "ERROR: linuxdeploy not found"
    /opt/linuxdeploy/AppRun --version || echo "ERROR: linuxdeploy not executable"

    {
        echo "/opt/linuxdeploy"
        echo "/opt/linuxdeploy-plugin-qt"
    } >> "$GITHUB_PATH"
    echo "DEPLOY_MODE=unpackaged" >> "$GITHUB_ENV"
}

fix_permissions() {
    local appdir="$1"

    find "$appdir" -type f -name "easykiconverter" -exec chmod 755 {} \;
    find "$appdir/usr/bin" -type f -exec chmod 755 {} \; 2>/dev/null || true
    find "$appdir/usr/libexec" -type f -exec chmod 755 {} \; 2>/dev/null || true
    chmod 755 "$appdir/AppRun" 2>/dev/null || true
}

render_nfpm_config() {
    local appdir="$1"
    local version="$2"
    local nfpm_arch="$3"
    local output="$4"

    python3 "$PROJECT_ROOT/tools/python/render_nfpm_config.py" \
        --template "$PROJECT_ROOT/deploy/nfpm.yaml" \
        --output "$output" \
        --project-root "$PROJECT_ROOT" \
        --app-dir "$appdir" \
        --version "$version" \
        --arch "$nfpm_arch"
}

package_appimage() {
    local appdir="$1"
    local product="$2"
    local version="$3"
    local git_hash="$4"
    local appimage_arch="$5"
    local output_name="${product}-${version}-g${git_hash}.${appimage_arch}.AppImage"

    for qt_lib_dir in /opt/qt/*/*/plugins/sqldrivers; do
        if [ -d "$qt_lib_dir" ]; then
            echo "Removing host Qt SQL drivers from: $qt_lib_dir"
            rm -f "$qt_lib_dir"/libqsql*.so* 2>/dev/null || true
        fi
    done

    fix_permissions "$appdir"

    /opt/linuxdeploy/AppRun --appdir "$appdir" \
        --executable "$appdir/usr/bin/easykiconverter" \
        --desktop-file "$appdir/io.github.tangsangsimida.easykiconverter.desktop" \
        --output appimage

    sed -i "s|^Exec=.*easykiconverter|Exec=AppRun|" "$appdir/io.github.tangsangsimida.easykiconverter.desktop"
    sed -i "s|^Exec=.*easykiconverter|Exec=AppRun|" "$appdir/usr/share/applications/io.github.tangsangsimida.easykiconverter.desktop"

    /opt/linuxdeploy/AppRun --appdir "$appdir" --output appimage

    chmod +x "${product}"-*.AppImage
    mv "${product}"-*.AppImage "$output_name"
    sha256sum "$output_name" | tee "${output_name}.sha256sum"
}

package_nfpm() {
    local packager="$1"
    local appdir="$2"
    local product="$3"
    local version="$4"
    local git_hash="$5"
    local nfpm_arch="$6"
    local output_arch="$7"
    local extension="$8"
    local nfpm_config="nfpm_temp.yaml"
    local output_name="${product}-${version}-g${git_hash}_${output_arch}.${extension}"

    fix_permissions "$appdir"
    render_nfpm_config "$appdir" "$version" "$nfpm_arch" "$nfpm_config"
    nfpm pkg --packager "$packager" --target . --config "$nfpm_config"

    find . -maxdepth 1 -name "*.${extension}" -exec mv {} "$output_name" \;
    sha256sum "$output_name" | tee "${output_name}.sha256sum"
}

main() {
    if [ "$#" -lt 1 ]; then
        usage
        exit 2
    fi

    local command="$1"
    shift

    case "$command" in
        install-linuxdeploy)
            [ "$#" -eq 1 ] || { usage; exit 2; }
            install_linuxdeploy "$1"
            ;;
        fix-permissions)
            [ "$#" -eq 1 ] || { usage; exit 2; }
            fix_permissions "$1"
            ;;
        package-appimage)
            [ "$#" -eq 5 ] || { usage; exit 2; }
            package_appimage "$@"
            ;;
        package-nfpm)
            [ "$#" -eq 8 ] || { usage; exit 2; }
            package_nfpm "$@"
            ;;
        *)
            usage
            exit 2
            ;;
    esac
}

main "$@"
