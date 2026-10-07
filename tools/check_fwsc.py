#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check an FM-1 package (.fwsc) before installing it: take it apart the other way round from
fm1pkg_make.py and verify every CRC, then (with --ref) compare it region by region with a package
that is known to install and boot, so that the only bytes that differ are the app's.

  check_fwsc.py PACKAGE.fwsc [--app APP.bin] [--ref REF.fwsc]

What it checks:
  container   the identity (FM-1_8...), the UFW header and file list CRCs, each file's CRC
  head        flash [0, 0x4000): the JLFS entries and their CRCs, the SPL (uboot.boot) and the chip
              key blob. The update loader never writes the head; this is what keeps the chip's
              own boot and update mode, the last way back
  app area    decrypted with the chip key: app_area_head, app.bin and cfg_tool.bin CRCs, the app
              within its slot, the region descriptors, the cfg directory (EQ table)
  --app       app.bin in the package is exactly this file (padded with 0xFF to the slot)
  --ref       the head, the update loader (ota.bin), the region descriptors, cfg_tool.bin and the
              EQ table are byte-identical to the reference package; only app.bin (and the CRC over
              the block that holds it) may differ
Exit status 0 when everything holds."""
import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fm1pkg_make import APP_SLOT, FLASH_SIZE, KEY, crc16, enc, sfc, chipkey_decode  # noqa: E402
from fm1_install import logical_image, product_of  # noqa: E402

FAILS = []


def ok(cond, what):
    print(("  ok    " if cond else "  FAIL  ") + what)
    if not cond:
        FAILS.append(what)
    return cond


def dec(b, off, n, key=0xFFFF):
    x = bytearray(b[off:off + n])
    enc(x, 0, n, key)
    return bytes(x)


def entries(blob, off, n, key=0xFFFF):
    """n 32-byte JLFS entries: (crc_ok, dcrc, offset, size, flags, resvd, index, name)"""
    out = []
    for i in range(n):
        e = dec(blob, off + 32 * i, 32, key) if key is not None else blob[off + 32 * i:off + 32 * i + 32]
        crc, = struct.unpack_from("<H", e, 0)
        dcrc, offset, size, flags, resvd, index = struct.unpack_from("<HIIBBH", e, 2)
        name = e[16:32].split(b"\0")[0].split(b"\xff")[0].decode("latin-1")
        out.append((crc16(e[2:]) == crc, dcrc, offset, size, flags, resvd, index, name, e))
    return out


def unpack(path):
    raw = Path(path).read_bytes()
    product = product_of(raw)
    img = logical_image(raw)
    hdr = dec(img, 0, 0x40)
    total, nfiles = struct.unpack_from("<IH", hdr, 4)
    files = {}
    lst_ok = crc16(img[0x40:0x40 + 0x50 * nfiles]) == struct.unpack_from("<H", hdr, 2)[0]
    for i in range(nfiles):
        e = dec(img, 0x40 + 0x50 * i, 0x50)
        typ, idx, dcrc, _, off, size, _ = struct.unpack_from("<HHHHIII", e, 0)
        name = e[0x40:].split(b"\0")[0].decode()
        d = img[off:off + size]
        files[name] = (d, crc16(d) == dcrc)
    return {"product": product, "hdr_ok": crc16(hdr[2:0x40]) == struct.unpack_from("<H", hdr, 0)[0],
            "list_ok": lst_ok, "total": total, "files": files}


def app_area(flash):
    """decrypt the app area: (head entry, entries, plain region, cfg entries)"""
    blk = 0x120 + APP_SLOT + 0x200                     # enough to read the head; resized below
    head = bytearray(flash[0x4000:0x4000 + 32])
    sfc(head, 0, 32, 0, KEY)
    h = entries(bytes(head), 0, 1, None)[0]
    blk = h[3]
    region = bytearray(flash[0x4000:0x4000 + blk + 0x400])
    sfc(region, 0, len(region), 0, KEY)
    ents = entries(bytes(region), 32, 6, None)
    cfg = entries(bytes(region), blk, 2, None)
    return h, ents, bytes(region), cfg, blk


def check(path, app=None):
    print(f"{path}")
    p = unpack(path)
    ok(p["product"].startswith("FM-1_8"), f"identity {p['product']!r} (FM-1_8...: a Felucca-family package)")
    ok(p["hdr_ok"] and p["list_ok"], "UFW header and file list CRCs")
    ok(set(p["files"]) == {"flash.bin", "ota.bin"}, f"files: {sorted(p['files'])}")
    for n, (d, c) in p["files"].items():
        ok(c, f"{n}: {len(d)} B, CRC")
    flash, ota = p["files"]["flash.bin"][0], p["files"]["ota.bin"][0]
    ok(len(flash) == FLASH_SIZE, f"flash.bin is {len(flash):#x} B (expected {FLASH_SIZE:#x})")
    ok(b"FELUCCA-LOADER-1" in ota, "ota.bin carries Felucca's update-loader marker")
    # head
    top = entries(flash, 0x20, 4)
    names = [e[7] for e in top]
    ok(names == ["uboot.boot", "isd_config.ini", "app_dir_head", "key_mac"], f"head entries {names}")
    ok(all(e[0] for e in top), "head entry CRCs")
    spl = top[0]
    ok(crc16(flash[spl[2]:spl[2] + spl[3]]) == spl[1], f"SPL (uboot.boot, {spl[3]} B) CRC")
    isd = top[1]
    ok(crc16(flash[isd[2]:isd[2] + isd[3]]) == isd[1], "isd_config CRC")
    ok(chipkey_decode(flash[isd[2]:isd[2] + 32]) == KEY, f"chip key blob decodes to {KEY:#06x}")
    ok(top[2][2] == 0x4000, "the app directory starts at 0x4000, after the head")
    # app area
    h, ents, region, cfg, blk = app_area(flash)
    ok(h[0] and h[7] == "app_area_head" and h[2] == 0x02000120, f"app_area_head: XIP {h[2]:#x}, {blk} B, entry CRC")
    ok(crc16(region[32:blk]) == h[1], "app_area_head CRC over its block")
    names = [e[7] for e in ents]
    ok(names[:2] == ["app.bin", "cfg_tool.bin"], f"app area entries {names[:2]} + 4 region descriptors")
    ok(all(e[0] for e in ents), "app area entry CRCs")
    a = ents[0]
    appbin = region[a[2]:a[2] + a[3]]
    ok(a[2] == 0x120 and a[3] == APP_SLOT, f"app.bin at 0x120, the whole slot ({APP_SLOT:#x} B)")
    ok(crc16(appbin) == a[1], "app.bin CRC")
    used = len(appbin.rstrip(b"\xff"))
    ok(used <= APP_SLOT, f"app uses {used} B of {APP_SLOT} B")
    c = ents[1]
    ok(crc16(region[c[2]:c[2] + c[3]]) == c[1], "cfg_tool.bin CRC")
    ok(cfg[0][7] == "cfg" and cfg[1][7] == "eq_cfg_hw.bin" and cfg[0][0] and cfg[1][0], "cfg directory (EQ table) entries")
    if app:
        want = Path(app).read_bytes()
        ok(appbin == want + b"\xff" * (APP_SLOT - len(want)), f"app.bin is exactly {app}")
    return {"flash": flash, "ota": ota, "head": flash[:0x4000], "region": region, "ents": ents, "cfg": cfg,
            "blk": blk, "appbin": appbin}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("package")
    ap.add_argument("--app", help="the app.bin the package should carry (build/kalimba.bin)")
    ap.add_argument("--ref", help="a package known to install and boot, to compare with")
    a = ap.parse_args()
    me = check(a.package, a.app)
    if a.ref:
        print()
        ref = check(a.ref)
        print(f"\n{a.package} against {a.ref}")
        ok(me["head"] == ref["head"], "head [0, 0x4000) byte-identical: the same SPL, entries and key")
        ok(me["ota"] == ref["ota"], f"update loader (ota.bin, {len(me['ota'])} B) byte-identical")
        ok(me["blk"] == ref["blk"], "app area block the same size")
        ok([e[1:8] for e in me["ents"][1:]] == [e[1:8] for e in ref["ents"][1:]],
           "cfg_tool.bin entry and the region descriptors identical")
        b = me["blk"]
        ok(me["region"][b:] == ref["region"][b:], "cfg directory and EQ table identical")
        r0, r1 = me["region"], ref["region"]
        a0 = me["ents"][0][2]
        # allowed to differ: app.bin itself, the block head (its CRC over the block) and the app.bin
        # entry's two CRCs (bytes 32..35: the entry's own and the app's); its offset, size and name not
        diff = [i for i in range(len(r0)) if r0[i] != r1[i] and not (a0 <= i < a0 + APP_SLOT) and not (0 <= i < 36)]
        ok(not diff, "outside app.bin and the CRCs over it, the app area is identical"
           + (f": {len(diff)} bytes differ, first at {diff[0]:#x}" if diff else ""))
        fl0, fl1 = me["flash"], ref["flash"]
        tail = 0x4000 + len(r0)
        ok(fl0[tail:] == fl1[tail:], f"flash past the app area ({len(fl0) - tail} B) identical")
    print("\nall checks passed" if not FAILS else f"\n{len(FAILS)} CHECK(S) FAILED")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
