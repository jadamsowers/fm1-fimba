#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Get the three JieLi AC79 SDK files the package needs (uboot.boot, cfg_tool.bin, cfg/eq_cfg_hw.bin)
# into DEST/cpu/wl82/tools, where the build looks (AC79_SDK=DEST). From JieLi's own repository
# (gitee) when it can be reached, else from a GitHub mirror of the same tag. Each must match the
# SHA-256 the build knows (tools/build.py, SDK_SHA256), or nothing is kept.
#   tools/get_sdk.sh [DEST]      (default: ~/fw-AC79_AIoT_SDK)
set -e
DEST="${1:-$HOME/fw-AC79_AIoT_SDK}"
TAG=AC79NN_SDK_V1.2.1_2023-12-13
D="$DEST/cpu/wl82/tools"
mkdir -p "$D/cfg"
for base in "https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/raw/$TAG" \
            "https://raw.githubusercontent.com/amitv87/fw-AC79_AIoT_SDK/$TAG"; do
    ok=1
    for f in uboot.boot cfg_tool.bin cfg/eq_cfg_hw.bin; do
        curl -fsL --retry 2 --connect-timeout 15 -o "$D/$f" "$base/cpu/wl82/tools/$f" || { ok=0; break; }
    done
    [ $ok = 1 ] && break
    echo "get_sdk: $base unreachable, trying the next source"
done
[ $ok = 1 ] || { echo "get_sdk: no source reachable"; exit 1; }
python3 - "$D" <<'PY'
import hashlib, re, sys
from pathlib import Path
want = dict(re.findall(r'"([\w./]+)": "([0-9a-f]{64})"', Path("tools/build.py").read_text()))
bad = [f for f, h in want.items() if hashlib.sha256((Path(sys.argv[1]) / f).read_bytes()).hexdigest() != h]
if bad:
    for f in want:
        (Path(sys.argv[1]) / f).unlink(missing_ok=True)
    sys.exit(f"get_sdk: {', '.join(bad)} do not match the known SHA-256: removed")
print(f"get_sdk: {len(want)} files, SHA-256 checked -> {sys.argv[1]}")
PY
echo "AC79_SDK=$DEST"
