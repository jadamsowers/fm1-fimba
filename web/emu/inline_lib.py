#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Put the update protocol's modules (web/fm1pkg.js, web/fm1ota.js: Felucca's) into the page where it
says /*FM1LIB*/, and fm1-reader's words.js with this build's words file where it says /*WORDSLIB*/, as plain
declarations inside its <script type="module">s, so the page stays one file.

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


def words():
    """fm1-reader's words.js and this build's words file (the one ann_words.h links), for the emulator's screen
    in words: the same words the FM-1 points a reader at, with the page, offline too"""
    out = [re.sub(r"^export\s+", "", (WEB / "emu" / "words.js").read_text(), flags=re.M)]
    h = WEB.parent / "build" / "gen" / "ann_words.h"
    m = re.search(r'/words/([0-9a-f]+\.json)"', h.read_text()) if h.exists() else None
    out.append("const WORDS_FILE = " + ((WEB / "words" / m.group(1)).read_text().strip() if m else '{"format": 1, "words": []}') + ";")
    return "\n".join(out)


def main(src, dst):
    page = Path(src).read_text()
    for mark in ("/*FM1LIB*/", "/*WORDSLIB*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"inline_lib: the page has no {mark} (or more than one)")
    page = page.replace("/*FM1LIB*/", lib().replace("</script", "<\\/script"))
    Path(dst).write_text(page.replace("/*WORDSLIB*/", words().replace("</script", "<\\/script")))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
