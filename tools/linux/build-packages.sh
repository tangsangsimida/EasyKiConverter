#!/bin/bash
# 本地打包脚本 - 用于在本地测试 DEB 和 AppImage 打包流程
# 用法: ./tools/linux/build-packages.sh [appimage|deb|all]

set -e

# 获取脚本所在目录的绝对路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 项目根目录是脚本目录向上两级（tools/linux -> tools -> 项目根）
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查必要的工具
check_tools() {
    print_info "检查必要的工具..."
    
    local missing_tools=()
    local need_install=false
    
    if ! command -v cmake &> /dev/null; then
        missing_tools+=("cmake")
        need_install=true
    fi
    
    if ! command -v linuxdeploy &> /dev/null; then
        missing_tools+=("linuxdeploy")
        need_install=true
    fi
    
    if ! command -v linuxdeploy-plugin-qt &> /dev/null; then
        missing_tools+=("linuxdeploy-plugin-qt")
        need_install=true
    fi
    
    if ! command -v nfpm &> /dev/null; then
        missing_tools+=("nfpm")
        need_install=true
    fi
    
    if [ "$need_install" = true ]; then
        print_error "缺少必要的工具: ${missing_tools[*]}"
        echo ""
        print_warn "尝试自动安装缺失的工具（需要 sudo 权限）..."
        echo ""
        
        # 尝试自动安装
        if command -v sudo &> /dev/null; then
            install_tools || exit 1
        else
            print_error "需要 sudo 权限才能安装工具"
            echo ""
            echo "手动安装方法："
            echo ""
            echo "1. 安装 linuxdeploy（从 AppImage 提取，与 CI 一致）:"
            echo "   sudo mkdir -p /opt/linuxdeploy"
            echo "   wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
            echo "   chmod +x linuxdeploy-x86_64.AppImage"
            echo "   ./linuxdeploy-x86_64.AppImage --appimage-extract"
            echo "   sudo cp -r squashfs-root/* /opt/linuxdeploy/"
            echo "   sudo ln -s /opt/linuxdeploy/AppRun /usr/local/bin/linuxdeploy"
            echo "   rm -rf squashfs-root linuxdeploy-x86_64.AppImage"
            echo ""
            echo "2. 安装 linuxdeploy Qt 插件:"
            echo "   sudo mkdir -p /opt/linuxdeploy-plugin-qt"
            echo "   wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
            echo "   chmod +x linuxdeploy-plugin-qt-x86_64.AppImage"
            echo "   ./linuxdeploy-plugin-qt-x86_64.AppImage --appimage-extract"
            echo "   sudo cp -r squashfs-root/* /opt/linuxdeploy-plugin-qt/"
            echo "   sudo ln -s /opt/linuxdeploy-plugin-qt/AppRun /usr/local/bin/linuxdeploy-plugin-qt"
            echo "   rm -rf squashfs-root linuxdeploy-plugin-qt-x86_64.AppImage"
            echo ""
            echo "3. 安装 nfpm（从 goreleaser 官方源）:"
            echo "   echo 'deb [trusted=yes] https://repo.goreleaser.com/apt/ /' | sudo tee /etc/apt/sources.list.d/goreleaser.list"
            echo "   sudo apt-get update"
            echo "   sudo apt-get install -y nfpm"
            echo ""
            exit 1
        fi
    fi
    
    print_info "所有必要的工具已安装"
}

# 自动安装缺失的工具
install_tools() {
    print_info "开始安装缺失的工具..."
    
    # 验证 sudo 权限并提示用户输入密码
    print_warn "需要 sudo 权限来安装工具，请输入密码..."
    if ! sudo -v; then
        print_error "无法获取 sudo 权限，请检查您的密码或联系系统管理员"
        return 1
    fi
    print_info "sudo 权限已验证，继续安装..."
    
    local temp_dir=$(mktemp -d)
    cd "$temp_dir"
    
    # 安装 cmake（如果缺失）
    if ! command -v cmake &> /dev/null; then
        print_info "安装 cmake..."
        sudo apt-get update -qq
        sudo apt-get install -y -qq cmake ninja-build || {
            print_error "无法安装 cmake"
            cd "$PROJECT_ROOT"
            rm -rf "$temp_dir"
            return 1
        }
    fi
    
    # 安装 linuxdeploy（从 AppImage 提取）
    if ! command -v linuxdeploy &> /dev/null; then
        print_info "安装 linuxdeploy（从 AppImage 提取）..."
        
        wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
            -O linuxdeploy-x86_64.AppImage || {
            print_error "下载 linuxdeploy 失败"
            cd "$PROJECT_ROOT"
            rm -rf "$temp_dir"
            return 1
        }
        
        chmod +x linuxdeploy-x86_64.AppImage
        ./linuxdeploy-x86_64.AppImage --appimage-extract || {
            print_error "提取 linuxdeploy 失败"
            cd "$PROJECT_ROOT"
            rm -rf "$temp_dir"
            return 1
        }
        
        sudo mkdir -p /opt/linuxdeploy
        sudo cp -r squashfs-root/* /opt/linuxdeploy/
        sudo ln -sf /opt/linuxdeploy/AppRun /usr/local/bin/linuxdeploy
        sudo chmod +x /opt/linuxdeploy/AppRun
        
        rm -rf squashfs-root linuxdeploy-x86_64.AppImage
        print_info "linuxdeploy 安装完成"
    fi
    
    # 安装 linuxdeploy-plugin-qt（从 AppImage 提取）
    if ! command -v linuxdeploy-plugin-qt &> /dev/null; then
        print_info "安装 linuxdeploy-plugin-qt（从 AppImage 提取）..."
        
        wget -q https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage \
            -O linuxdeploy-plugin-qt-x86_64.AppImage || {
            print_error "下载 linuxdeploy-plugin-qt 失败"
            cd "$PROJECT_ROOT"
            rm -rf "$temp_dir"
            return 1
        }
        
        chmod +x linuxdeploy-plugin-qt-x86_64.AppImage
        ./linuxdeploy-plugin-qt-x86_64.AppImage --appimage-extract || {
            print_error "提取 linuxdeploy-plugin-qt 失败"
            cd "$PROJECT_ROOT"
            rm -rf "$temp_dir"
            return 1
        }
        
        sudo mkdir -p /opt/linuxdeploy-plugin-qt
        sudo cp -r squashfs-root/* /opt/linuxdeploy-plugin-qt/
        sudo ln -sf /opt/linuxdeploy-plugin-qt/AppRun /usr/local/bin/linuxdeploy-plugin-qt
        sudo chmod +x /opt/linuxdeploy-plugin-qt/AppRun
        
        rm -rf squashfs-root linuxdeploy-plugin-qt-x86_64.AppImage
        print_info "linuxdeploy-plugin-qt 安装完成"
    fi
    
    # 安装 nfpm（从 goreleaser 官方源）
    if ! command -v nfpm &> /dev/null; then
        print_info "安装 nfpm（从 goreleaser 官方源）..."
        
        echo 'deb [trusted=yes] https://repo.goreleaser.com/apt/ /' | sudo tee /etc/apt/sources.list.d/goreleaser.list > /dev/null
        sudo apt-get update -qq
        sudo apt-get install -y -qq nfpm || {
            print_error "无法安装 nfpm"
            cd "$PROJECT_ROOT"
            rm -rf "$temp_dir"
            return 1
        }
        print_info "nfpm 安装完成"
    fi
    
    cd "$PROJECT_ROOT"
    rm -rf "$temp_dir"
    
    print_info "所有工具安装完成！"
}

# 获取版本信息
get_version() {
    if [ -n "$VERSION" ]; then
        # 去掉版本号中的 v 前缀（如果有）
        echo "$VERSION" | sed 's/^v//'
    else
        # 从 git tag 获取版本号，去掉 v 前缀
        git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || echo "3.0.7"
    fi
}

# 构建 AppDir
build_appdir() {
    print_info "构建 AppDir..."
    
    local VERSION=$(get_version)
    local APP_DIR="${PROJECT_ROOT}/build/EasyKiConverter.AppDir"
    local BUILD_DIR="${PROJECT_ROOT}/build/appimage-build"
    
    # 清理旧的构建
    rm -rf "$APP_DIR" "$BUILD_DIR"
    
    # 创建 AppDir 结构
    mkdir -p "$APP_DIR/usr/bin"
    mkdir -p "$APP_DIR/usr/share/applications"
    mkdir -p "$APP_DIR/usr/share/icons/hicolor/512x512/apps"
    
    # 编译项目
    print_info "编译项目..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake ../.. \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=OFF \
        -DVERSION_FROM_CI="$VERSION"
    
    cmake --build . --config RelWithDebInfo --parallel
    
    # 安装到 AppDir
    print_info "安装到 AppDir..."
    cmake --install . --prefix "$APP_DIR/usr"
    rm -rf "$APP_DIR/usr/include"
    
    cd "$PROJECT_ROOT"
    
    # 创建桌面文件
    print_info "创建桌面文件..."
    cat > "$APP_DIR/com.tangsangsimida.EasyKiConverter.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=EasyKiConverter
Name[zh_CN]=EasyKiConverter
Comment=Convert LCSC and EasyEDA components to KiCad libraries
Comment[zh_CN]=将嘉立创和 EasyEDA 元件转换为 KiCad 库
Exec=easykiconverter %F
Icon=com.tangsangsimida.EasyKiConverter
Terminal=false
Categories=Development;Electronics;Engineering;
Keywords=KiCad;LCSC;EasyEDA;Component;Converter;Electronics;
StartupNotify=true
StartupWMClass=EasyKiConverter
MimeType=application/vnd.easyeda+json;
EOF
    
    # 复制图标
    print_info "复制图标..."
    cp resources/icons/app_icon.png "$APP_DIR/com.tangsangsimida.EasyKiConverter.png"
    
    # 运行 linuxdeploy 填充依赖
    print_info "运行 linuxdeploy 填充依赖..."
    linuxdeploy --appdir "$APP_DIR" --plugin qt \
        --executable "$APP_DIR/usr/bin/EasyKiConverter" \
        --desktop-file "$APP_DIR/com.tangsangsimida.EasyKiConverter.desktop" \
        --icon-file "$APP_DIR/com.tangsangsimida.EasyKiConverter.png"
    
    # 修复：复制 QML 模块到 AppDir（linuxdeploy-plugin-qt 可能不会自动复制）
    print_info "复制 QML 模块到 AppDir..."
    local QT_INSTALL_QML=$(qmake -query QT_INSTALL_QML 2>/dev/null || echo "")
    if [ -n "$QT_INSTALL_QML" ] && [ -d "$QT_INSTALL_QML" ]; then
        rm -rf "$APP_DIR/usr/qml"
        mkdir -p "$APP_DIR/usr/qml"
        cp -r "$QT_INSTALL_QML"/* "$APP_DIR/usr/qml/"
        
        # 修复：创建插件符号链接（Qt 6.10.2 使用 lib 前缀，但 qmldir 引用无前缀名称）
        print_info "创建 QML 插件符号链接..."
        find "$APP_DIR/usr/qml" -name "lib*.so" -type f | while read libfile; do
            plugin_name=$(basename "$libfile" | sed 's/^lib//')
            plugin_dir=$(dirname "$libfile")
            if [ ! -e "$plugin_dir/$plugin_name" ]; then
                ln -sf "$(basename "$libfile")" "$plugin_dir/$plugin_name"
            fi
        done
    fi
    
    # 修复：复制所有缺失的 Qt 库
    print_info "复制缺失的 Qt 库..."
    if [ -d "$APP_DIR/usr/qml" ] && [ -n "$QT_INSTALL_QML" ]; then
        local QT_LIB_DIR="$(dirname "$QT_INSTALL_QML")/lib"
        # 查找所有实际的插件文件（不是符号链接）
        find "$APP_DIR/usr/qml" -name "lib*.so" -type f 2>/dev/null | while read plugin; do
            # 使用 ldd 查找依赖的 Qt 库
            ldd "$plugin" 2>/dev/null | grep "$QT_LIB_DIR" | awk '{print $3}' | sort -u | while read lib; do
                libname=$(basename "$lib")
                if [ ! -f "$APP_DIR/usr/lib/$libname" ]; then
                    cp "$lib" "$APP_DIR/usr/lib/" 2>/dev/null && echo "  复制: $libname" || true
                fi
            done
        done
    fi
    
    # 配置 AppRun 脚本
    print_info "配置 AppRun 脚本..."
    rm -f "$APP_DIR/AppRun"
    cat > "$APP_DIR/AppRun" <<'EOF'
#!/bin/bash

# 获取 AppDir 路径
THIS_SCRIPT=$(readlink -f "$0" 2>/dev/null)
if [ -z "$THIS_SCRIPT" ]; then
    THIS_SCRIPT="$0"
fi
APPDIR="$(dirname "$THIS_SCRIPT")"

# 清除干扰的环境变量
unset QT_PLUGIN_PATH
unset QT_QPA_PLATFORM_PLUGIN_PATH
unset LD_LIBRARY_PATH
unset QTDIR
unset QT_SELECT

# 设置 AppImage 特定的环境变量
export LD_LIBRARY_PATH="${APPDIR}/usr/lib"
export PATH="${APPDIR}/usr/bin:${PATH}"
export QT_PLUGIN_PATH="${APPDIR}/usr/plugins"
export QML2_IMPORT_PATH="${APPDIR}/usr/qml"
export QT_QPA_PLATFORM=xcb
export XDG_DATA_HOME="${XDG_DATA_HOME:-${HOME}/.local/share}"
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-${HOME}/.config}"

# 启动应用
exec "${APPDIR}/usr/bin/EasyKiConverter" "$@"
EOF
    
    chmod +x "$APP_DIR/AppRun"
    
    print_info "AppDir 构建完成: $APP_DIR"
}

# 构建 AppImage
build_appimage() {
    print_info "构建 AppImage..."
    
    local VERSION=$(get_version)
    local GIT_HASH=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
    local APP_DIR="${PROJECT_ROOT}/build/EasyKiConverter.AppDir"
    
    if [ ! -d "$APP_DIR" ]; then
        print_info "AppDir 不存在，先构建 AppDir..."
        build_appdir
    fi
    
    # 生成 AppImage（使用已配置的 AppDir）
    print_info "生成 AppImage..."
    linuxdeploy --appdir "$APP_DIR" --output appimage
    
    # 重命名
    local output_dir="${PROJECT_ROOT}/build/packages"
    mkdir -p "$output_dir"
    
    mv EasyKiConverter-*.AppImage "$output_dir/EasyKiConverter-${VERSION}-g${GIT_HASH}.x86_64.AppImage"
    
    # 生成校验和
    cd "$output_dir"
    sha256sum EasyKiConverter-${VERSION}-g${GIT_HASH}.x86_64.AppImage > EasyKiConverter-${VERSION}-g${GIT_HASH}.x86_64.AppImage.sha256sum
    
    print_info "AppImage 构建完成: $output_dir/EasyKiConverter-${VERSION}-g${GIT_HASH}.x86_64.AppImage"
}

# 构建 DEB 包
build_deb() {
    print_info "构建 DEB 包..."
    
    local VERSION=$(get_version)
    local GIT_HASH=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
    local APP_DIR="${PROJECT_ROOT}/build/EasyKiConverter.AppDir"
    
    if [ ! -d "$APP_DIR" ]; then
        print_info "AppDir 不存在，先构建 AppDir..."
        build_appdir
    fi
    
    local output_dir="${PROJECT_ROOT}/build/packages"
    mkdir -p "$output_dir"
    
    # 使用 sed 动态替换 nfpm.yaml 中的环境变量和相对路径
    # nfpm 不能直接解析环境变量，需要先替换
    local temp_nfpm_config="${output_dir}/nfpm_temp.yaml"
    sed -e "s|\${APP_DIR}|$APP_DIR|g" \
        -e "s|\${VERSION}|$VERSION|g" \
        -e "s|deploy/scripts/postinstall.sh|$PROJECT_ROOT/deploy/scripts/postinstall.sh|g" \
        -e "s|deploy/scripts/prerm.sh|$PROJECT_ROOT/deploy/scripts/prerm.sh|g" \
        -e "s|deploy/scripts/postrm.sh|$PROJECT_ROOT/deploy/scripts/postrm.sh|g" \
        -e "s|deploy/scripts/easykiconverter-wrapper.sh|$PROJECT_ROOT/deploy/scripts/easykiconverter-wrapper.sh|g" \
        -e "s|deploy/scripts/easykiconverter-register.sh|$PROJECT_ROOT/deploy/scripts/easykiconverter-register.sh|g" \
        -e "s|deploy/scripts/easykiconverter-register.desktop|$PROJECT_ROOT/deploy/scripts/easykiconverter-register.desktop|g" \
        -e "s|deploy/scripts/trigger-gnome-refresh.sh|$PROJECT_ROOT/deploy/scripts/trigger-gnome-refresh.sh|g" \
        -e "s|deploy/metainfo/com.tangsangsimida.EasyKiConverter.metainfo.xml|$PROJECT_ROOT/deploy/metainfo/com.tangsangsimida.EasyKiConverter.metainfo.xml|g" \
        "$PROJECT_ROOT/deploy/nfpm.yaml" > "$temp_nfpm_config"
    
    # 运行 nfpm 构建（必须指定 --packager）
    nfpm package --packager deb --config "$temp_nfpm_config" --target "$output_dir" || {
        print_error "nfpm 构建失败，请检查 nfpm.yaml 配置"
        print_info "临时配置文件保留在: $temp_nfpm_config"
        cd "$PROJECT_ROOT"
        return 1
    }
    
    # 清理临时配置文件
    rm -f "$temp_nfpm_config"
    
    # 重命名
    cd "$output_dir"
    if [ -f "easykiconverter_${VERSION}-1_amd64.deb" ]; then
        mv "easykiconverter_${VERSION}-1_amd64.deb" "EasyKiConverter-${VERSION}-g${GIT_HASH}.amd64.deb"
        sha256sum "EasyKiConverter-${VERSION}-g${GIT_HASH}.amd64.deb" > "EasyKiConverter-${VERSION}-g${GIT_HASH}.amd64.deb.sha256sum"
        print_info "DEB 包构建完成: $output_dir/EasyKiConverter-${VERSION}-g${GIT_HASH}.amd64.deb"
    else
        print_warn "DEB 包文件未找到，查看生成的文件："
        ls -la *.deb 2>/dev/null || print_error "没有生成 DEB 文件"
        cd "$PROJECT_ROOT"
        return 1
    fi
    
    cd "$PROJECT_ROOT"
}

# 主函数
main() {
    local build_type="${1:-all}"
    
    print_info "开始本地打包流程..."
    print_info "项目根目录: $PROJECT_ROOT"
    print_info "版本: $(get_version)"
    
    check_tools
    
    case "$build_type" in
        appimage)
            build_appimage
            ;;
        deb)
            build_deb
            ;;
        all)
            build_appimage
            build_deb
            ;;
        *)
            print_error "未知的构建类型: $build_type"
            echo "用法: $0 [appimage|deb|all]"
            exit 1
            ;;
    esac
    
    print_info "打包完成！"
    print_info "输出目录: ${PROJECT_ROOT}/build/packages"
}

# 运行主函数
main "$@"
