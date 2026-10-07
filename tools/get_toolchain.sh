#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Get JieLi's Linux toolchain (clang 4.0.1 for pi32v2) and link it as DEST/toolchain.
#   tools/get_toolchain.sh [DEST]      (default: ~/.jieli)
# From JieLi's package server; if that can't be reached (some networks block it), from the
# enix223/build-jieli Docker image, whose Dockerfile unpacks the same package into /opt/jieli
# (https://github.com/enix223/jieli-docker-build-env). JIELI_FROM_DOCKER=1 goes straight there.
set -e
DEST="${1:-$HOME/.jieli}"
IMAGE="${JIELI_TOOLCHAIN_IMAGE:-enix223/build-jieli:1.0.0}"
mkdir -p "$DEST"
TMP="$DEST/.download.tar.xz"
if [ "${JIELI_FROM_DOCKER:-0}" != 1 ] && curl -fL --retry 3 --connect-timeout 15 -o "$TMP" https://pkgman.jieliapp.com/s/linux-toolchain; then
    tar -xJf "$TMP" -C "$DEST"
    rm -f "$TMP"
    TC="$(ls -d "$DEST"/jieli-linux-toolchains-* 2>/dev/null | sort | tail -1)"
else
    rm -f "$TMP"
    echo "get_toolchain: JieLi's server can't be reached; taking the toolchain from the Docker image $IMAGE"
    docker info >/dev/null 2>&1 || { echo "get_toolchain: Docker is not running"; exit 1; }
    TC="$DEST/jieli-linux-toolchains-from-$(echo "$IMAGE" | tr '/:' '--')"
    rm -rf "$TC"
    C="$(docker create --platform linux/amd64 "$IMAGE")"
    docker cp "$C:/opt/jieli" "$TC" || { docker rm "$C" >/dev/null; exit 1; }
    docker rm "$C" >/dev/null
fi
[ -n "$TC" ] && [ -e "$TC/pi32v2/bin/clang" ] || { echo "get_toolchain: no pi32v2/bin/clang in what was downloaded"; exit 1; }
ln -sfn "$(basename "$TC")" "$DEST/toolchain"
echo "JIELI_TOOLCHAIN=$DEST/toolchain ($(basename "$TC"))"
