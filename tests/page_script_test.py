#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The page's scripts parse, as web/emu/inline_lib.py puts it together (fm1pkg.js and fm1ota.js inlined into the
installer's module). A script that doesn't parse leaves its part of the page dead: FoMni 0.4's installer declared
STOCK_SHA256 a second time (fm1ota.js has it), so nothing installed. (FoMni's tests/page_script_test.py.)
  python3 tests/page_script_test.py   (needs node)"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    out = Path(tmp) / "index.html"
    subprocess.run([sys.executable, str(SRC / "web" / "emu" / "inline_lib.py"), str(SRC / "web" / "emu" / "index.html"),
                    str(out)], check=True)
    page = out.read_text(encoding="utf-8")
scripts = re.findall(r"<script( type=\"module\")?>(.*?)</script>", page, re.S)
if not scripts:
    sys.exit("no scripts in the page")
for i, (module, js) in enumerate(scripts):
    r = subprocess.run(["node", "--check", "--input-type=" + ("module" if module else "commonjs")], input=js,
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"script {i + 1}{' (module)' if module else ''}: does not parse:\n{r.stderr}")
    print(f"ok script {i + 1}{' (module)' if module else ''}")
