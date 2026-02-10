# syntax=docker/dockerfile:1
# 使用 Ubuntu 20.04 作为基础镜像
FROM ubuntu:20.04 AS builder

LABEL maintainer="EasyKiConverter Team"
LABEL description="Build environment for EasyKiConverter with Qt 6 and LinuxDeploy"

# 避免交互式前端
ENV DEBIAN_FRONTEND=noninteractive

# 构建参数
ARG QT_VERSION=6.8.0
ARG QT_MODULES="qtimageformats qtshadertools"
ARG QT_HOST=linux
ARG QT_TARGET=desktop
ARG QT_ARCH=linux_gcc_64

# 1. 安装系统依赖 & 工具 (合并层以减小体积)
# - build-essential, cmake, ninja: 构建工具
# - ccache: 编译加速
# - runtime libs: libgl1... (Qt 依赖)
# - fuse: AppImage 依赖
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    ccache \
    git \
    curl \
    wget \
    python3 \
    python3-pip \
    python3-dev \
    libgl1-mesa-dev \
    libxkbcommon-x11-0 \
    libxkbcommon-dev \
    libvulkan1 \
    libwayland-cursor0 \
    libglib2.0-0 \
    libsm6 \
    libice6 \
    libxext6 \
    libxrender1 \
    libfontconfig1-dev \
    libfreetype6-dev \
    libx11-dev \
    libx11-xcb-dev \
    libxcb-cursor0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-randr0 \
    libxcb-render-util0 \
    libxcb-shape0 \
    libxcb-sync1 \
    libxcb-xfixes0 \
    libxcb-xinerama0 \
    libxcb-xkb1 \
    libssl-dev \
    file \
    fuse \
    libfuse2 \
    rpm \
    ca-certificates \
    locales \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* \
    && locale-gen en_US.UTF-8

# 设置环境变量
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8
ENV PATH="/usr/lib/ccache:${PATH}"

# 2. 安装 Qt 6
# 使用 pip 缓存挂载点 (如果使用 BuildKit) 或者手动清理
RUN pip3 install --no-cache-dir aqtinstall \
    && aqt install-qt ${QT_HOST} ${QT_TARGET} ${QT_VERSION} ${QT_ARCH} \
       --outputdir /opt/qt \
       --modules ${QT_MODULES} \
    && pip3 uninstall -y aqtinstall

# 设置 Qt 环境变量
ENV Qt6_DIR=/opt/qt/${QT_VERSION}/gcc_64
ENV PATH=$Qt6_DIR/bin:$PATH
ENV LD_LIBRARY_PATH=$Qt6_DIR/lib:$LD_LIBRARY_PATH

# 3. 预下载 QXlsx 源码（用于 Excel BOM 解析）
RUN git clone https://github.com/QtExcel/QXlsx.git /opt/QXlsx

# 设置 QXlsx 环境变量（指向源码目录）
ENV QXLSX_ROOT=/opt/QXlsx/QXlsx

# 4. 安装 LinuxDeploy & 插件
# 直接下载并提取，减少临时文件
RUN mkdir -p /opt/linuxdeploy /opt/linuxdeploy-plugin-qt \
    && wget -q -O /tmp/linuxdeploy https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
    && chmod +x /tmp/linuxdeploy \
    && cd /tmp && ./linuxdeploy --appimage-extract \
    && cp -r squashfs-root/* /opt/linuxdeploy/ \
    && ln -s /opt/linuxdeploy/AppRun /usr/local/bin/linuxdeploy \
    && wget -q -O /tmp/linuxdeploy-plugin-qt https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage \
    && chmod +x /tmp/linuxdeploy-plugin-qt \
    && cd /tmp && ./linuxdeploy-plugin-qt --appimage-extract \
    && cp -r squashfs-root/* /opt/linuxdeploy-plugin-qt/ \
    && ln -s /opt/linuxdeploy-plugin-qt/AppRun /usr/local/bin/linuxdeploy-plugin-qt \
    && rm -rf /tmp/linuxdeploy* /tmp/squashfs-root

# 4. 安装 nfpm (用于生成 Arch Linux 包等)
RUN echo 'deb [trusted=yes] https://repo.goreleaser.com/apt/ /' | tee /etc/apt/sources.list.d/goreleaser.list \
    && apt-get update \
    && apt-get install -y nfpm

# 验证环境
RUN cmake --version && ninja --version && qmake --version && ccache --version && nfpm --version

WORKDIR /build
CMD ["/bin/bash"]
