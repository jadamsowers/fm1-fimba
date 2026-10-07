#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The browser emulator as one file that runs from the filesystem (file://, no web server).

A page opened from disk may not fetch() files next to it, nor load an AudioWorklet module from
one, so this puts both inside the page as base64: the WebAssembly, decoded in the page, and the
worklet, loaded from a data: URL. Everything else is the page web/emu/build.sh builds (index.html).

  web/emu/make_standalone.py build/emu OUT.html
"""
import base64
import sys
from pathlib import Path

FETCH = ('fetch("kalimba.wasm").then((r) => { if (!r.ok) throw new Error(`kalimba.wasm: ${r.status}`); '
         'return r.arrayBuffer(); })')
ADD = 'ac.audioWorklet.addModule("worklet.js")'
HEAD = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
"""


def main(emu, out):
    emu = Path(emu)
    page = (emu / "index.html").read_text()
    wasm = base64.b64encode((emu / "kalimba.wasm").read_bytes()).decode()
    worklet = (emu / "worklet.js").read_text()
    for needle in (FETCH, ADD):
        if page.count(needle) != 1:
            raise SystemExit(f"make_standalone: the page no longer has {needle[:40]!r}: update this script")
    page = page.replace(FETCH, "Promise.resolve(Uint8Array.from(atob(EMBED_WASM), (c) => c.charCodeAt(0)).buffer)")
    # a data: URL, not a Blob URL: Chrome loads no worklet from a blob: of a file:// page (its origin
    # is opaque), but does from data:
    page = page.replace(ADD, 'ac.audioWorklet.addModule("data:text/javascript;base64," + EMBED_WORKLET_B64)')
    embed = ("<script>\n// embedded by web/emu/make_standalone.py: the emulator and its audio worklet\n"
             f"const EMBED_WASM = \"{wasm}\";\n"
             f"const EMBED_WORKLET_B64 = \"{base64.b64encode(worklet.encode()).decode()}\";\n</script>\n")
    # the published page is wrapped in a document skeleton by its host; on disk it needs its own
    page = HEAD + page.replace("</style>", "</style>\n</head>\n<body>", 1)
    i = page.index("<script>")
    page = page[:i] + embed + page[i:] + "\n</body>\n</html>\n"
    Path(out).write_text(page)
    print(f"standalone: {out} ({len(page) // 1024} KiB, open it straight from disk)")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
