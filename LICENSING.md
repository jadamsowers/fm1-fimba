# KALIMBA licensing

KALIMBA is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`,
full text in `LICENSE`). It is built on FoMni (and through it X0X and Felucca), and keeps their
licence. If you distribute KALIMBA, or firmware derived from it, you must give your recipients its
complete corresponding source under the same licence.

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| Platform: `firmware/hal/`, `firmware/loader/`, `firmware/src/{libc,lcd,gfx,usb,storage,ota,midi_uart}.c`, `firmware/crt0.S`, `firmware/app.ld`, `tools/` (build, package, install, rescue, font), `web/fm1*.js` | [Felucca](https://github.com/hugelton/Felucca), Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, as changed for [X0X](https://github.com/charlesvestal/fm1-x0x) and [FoMni](https://github.com/charlesvestal/fm1-fomni) (USB names changed here) | GPL-3.0-only |
| `firmware/src/app/{panel,plat_fm1,main_fm1}.c`, `firmware/src/main.c`, `host/kalimba_host.c`, the frame of `firmware/src/app/ui.c` (bands, knobs, autosave) and `project.c` | adapted from FoMni (itself from X0X and Felucca) | GPL-3.0-only |
| `firmware/src/dsp/fastmath.h`; the plate reverb in `firmware/src/dsp/kalimba.c` (Dattorro's design) | FoMni / X0X | GPL-3.0-only |
| Platform tests (`tests/host/{fastmath,encoder,uac,trs}_test.c`, `tests/{storage,ota,ldr}_test.c`, `tests/install_test.py`) | FoMni / Felucca | GPL-3.0-only |
| Everything else: the tine, body, buzz, grain and delay engine and the music (`firmware/src/dsp/kalimba.{c,h}`), the UI's instrument, `tests/host/kalimba_test.c`, `tests/scenarios/` | KALIMBA | GPL-3.0-only |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Barlow Semi Condensed (The Barlow Project Authors), the UI face | SIL OFL 1.1 | `assets/fonts/BarlowSemiCondensed-*.ttf`, `assets/fonts/Barlow-OFL.txt` |
| Terminus (Dimitar Toshkov Zhekov), an alternative font set | SIL OFL 1.1 | `assets/fonts/ter-u*.bdf`, `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. "M-VAVE" and "FM-1" are
trademarks of their respective owners. KALIMBA is independent firmware, not affiliated with, endorsed
by or supported by any of them.

## Radio

KALIMBA never enables the Bluetooth / Wi-Fi radio of the hardware.
