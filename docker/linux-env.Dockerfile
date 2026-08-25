# SPDX-FileCopyrightText: Copyright (c) 2026
# SPDX-License-Identifier: BSD-2-Clause
#
# Linux image for linux-gl and linux-nvidia-rtx. No CUDA required.

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=UTC \
    LANG=C.UTF-8

RUN apt-get update -y && apt-get install -y --no-install-recommends \
        ca-certificates \
        python3 \
        cmake \
        ninja-build \
        g++ \
        pkg-config \
        git \
        xvfb \
        locales \
        qt6-base-dev \
        qt6-svg-dev \
        qt6-qpa-plugins \
        libxcb-cursor0 \
        libgl1-mesa-dev \
        libgl1-mesa-dri \
        libocct-data-exchange-dev \
        libocct-draw-dev \
        occt-misc \
        libtbb-dev \
        libxi-dev \
        libassimp-dev \
        file \
        binutils \
    && printf 'fr_FR ISO-8859-15\nfr_FR.UTF-8 UTF-8\n' >> /etc/locale.gen \
    && locale-gen \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
ENV DISPLAY=:99 \
    LIBGL_ALWAYS_SOFTWARE=1 \
    MAYO_SKIP_GL_TESTS=1 \
    MAYO_FETCHCONTENT_BASE_DIR=/src/cmake/_deps

COPY entrypoint.sh /usr/local/bin/mayo-linux-entrypoint
RUN chmod +x /usr/local/bin/mayo-linux-entrypoint

ENTRYPOINT ["/usr/local/bin/mayo-linux-entrypoint"]
CMD ["python3", "scripts/mayo-env.py", "info"]
