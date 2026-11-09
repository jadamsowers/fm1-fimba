#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The build command the shared FM-1 builder (zvenson/fm1-build@v1) runs for FiMba-1, on GitHub's Ubuntu runner:
# web.yml's attested job (every run) and release-attest.yml (on a release's tag). The package to attest goes to
# build/release-assets/.
#   tools/attested_build.sh [X.Y.Z]     (a version: a release build; none: a development build)
set -eu
python3 -m pip install --quiet -r requirements.txt
sudo apt-get install -y -qq libfribidi0          # Pillow's raqm: the fonts as web.yml's firmware job has them
python3 -c "from PIL import features; assert features.check('raqm'), 'Pillow without raqm'"
mkdir -p build/release-assets
if [ -n "${1:-}" ]; then
    python3 tools/build.py --release "$1"
    cp "build/fimba-$1.fwsc" build/release-assets/
    cp "build/fimba-$1.fwsc" build/fimba.fwsc     # (the tests check the update path against build/fimba.fwsc)
else
    python3 tools/build.py
    cp build/fimba.fwsc build/release-assets/
fi
