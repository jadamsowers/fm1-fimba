#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Put the update protocol's modules (web/fm1pkg.js, web/fm1ota.js: Felucca's) into the page where it
says /*FM1LIB*/, as plain declarations inside its <script type="module">, so the page stays one file.

  web/emu/inline_lib.py IN.html OUT.html"""
import re
import sys
from pathlib import Path

WEB = Path(__file__).resolve().parents[1]


def lib():
    out = []
    for name in ("fm1pkg.js", "fm1ota.js"):
        src = (WEB / name).read_text()
        if re.search(r"^\s*import\b", src, re.M):
            raise SystemExit(f"inline_lib: {name} imports something: inline that too")
        out.append(f"/* ---- web/{name} (inlined) ---- */\n" + re.sub(r"^export\s+", "", src, flags=re.M))
    return "\n".join(out)


def main(src, dst):
    page = Path(src).read_text()
    if page.count("/*FM1LIB*/") != 1:
        raise SystemExit("inline_lib: the page has no /*FM1LIB*/ (or more than one)")
    Path(dst).write_text(page.replace("/*FM1LIB*/", lib().replace("</script", "<\\/script")))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
