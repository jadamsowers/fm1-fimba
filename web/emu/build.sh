#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# FiMba-1 in the browser: the host simulator compiled to WebAssembly (Emscripten), plus its page.
#   [OM_VERSION=0.1] web/emu/build.sh  ->  build/emu/{index.html, worklet.js, kalimba.wasm}
#   and build/emu/FiMba-1.html: the same, in one file that opens from disk (file://)
# Same sources and flags as host/build_host.sh (-ffp-contract=off, like the device).
set -e
cd "$(dirname "$0")/../.."
PY="${PYTHON:-python3}"
mkdir -p build/gen build/emu
[ -f build/gen/felucca_font.h ] || "$PY" tools/gen_font.py build/gen/felucca_font.h >/dev/null
emcc -O2 -ffp-contract=off -std=gnu99 -Wall -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable \
    -DOM_HOST -DOM_WEB "-DOM_VERSION=\"$(printf %s "${OM_VERSION:-DEV}" | tr a-z A-Z)\"" -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o build/emu/kalimba.wasm web/emu/kalimba_web.c firmware/src/dsp/kalimba.c
cp web/emu/worklet.js build/emu/
"$PY" web/emu/inline_lib.py web/emu/index.html build/emu/index.html   # the installer's modules, inlined
echo "emu: build/emu ($(wc -c < build/emu/kalimba.wasm) B wasm)"
# one file that runs from the filesystem, no web server
"$PY" web/emu/make_standalone.py build/emu build/emu/FiMba-1.html
