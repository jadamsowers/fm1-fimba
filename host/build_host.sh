#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the FiMba-1 host simulator (the whole app on this machine) into build/host/kalimba_host.
# Same sources as the firmware, -ffp-contract=off like the device (no fused multiply-add).
set -e
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
PY="${PYTHON:-python3}"
mkdir -p build/gen build/host
[ -f build/gen/felucca_font.h ] || "$PY" tools/gen_font.py build/gen/felucca_font.h >/dev/null
$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
    -DOM_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen -o build/host/kalimba_host host/kalimba_host.c \
    firmware/src/dsp/kalimba.c -lm
echo "host: build/host/kalimba_host"
