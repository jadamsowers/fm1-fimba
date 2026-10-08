# Building FiMba-1

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `fimba.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `fimba.fwsc` | the installable package (app + loader) |

## Prerequisites (macOS)

- Python 3.9 or newer with the packages in `requirements.txt` (Pillow for the build; mido,
  python-rtmidi and pyusb for installing and rescue):

  ```
  python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
  source .venv/bin/activate         # then ./build.sh and the tools use it
  ```

  Pillow without its raqm layout library (Pillow 11 on Python 3.9, for one) works: the fonts'
  digits are then given equal widths by `gen_font.py` itself.
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

  Some networks block JieLi's server (`pkgman.jieliapp.com`: "Couldn't connect to server"). The
  script then takes the same toolchain from the `enix223/build-jieli` Docker image, whose
  [Dockerfile](https://github.com/enix223/jieli-docker-build-env) unpacks JieLi's package into
  `/opt/jieli` (a third-party image: the build only ever runs it inside a container).

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

  If gitee.com is blocked too, the three files are in a GitHub mirror of the same tag. The build
  compares each one's SHA-256 with the known file and warns if one differs (the GitHub Action
  refuses outright); check them yourself with `shasum -a 256` against `SDK_SHA256` in `tools/build.py`:

  ```
  T=AC79NN_SDK_V1.2.1_2023-12-13; D=~/fw-AC79_AIoT_SDK/cpu/wl82/tools; mkdir -p $D/cfg
  for f in uboot.boot cfg_tool.bin cfg/eq_cfg_hw.bin; do
      curl -fL -o $D/$f https://raw.githubusercontent.com/amitv87/fw-AC79_AIoT_SDK/$T/cpu/wl82/tools/$f
  done
  ```

- Node.js (optional, for the web tests).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

`./build.sh --release 0.1` makes a release build; the package is `build/fimba-0.1.fwsc`.

On macOS with podman instead of Docker, put a `docker` script that runs `exec podman "$@"` first
on your PATH. `OM_JOBS` (default 4) limits parallel compiles: a podman machine drops
connections when many containers start at once.

Build option: `OM_CDC=1` adds Felucca's USB serial function (off by default: one plain
MIDI interface).

The build also generates `build/gen/` (the font). It fails if a soft-double routine is linked
(a `double` crept in) and checks the image, RAM and pool sizes.

## Tests

```
tests/run_tests.sh
```

Runs, on the build machine: the maths library against libm; the instrument
(`tests/host/kalimba_test.c`, linked without libm as on the device: scales and layouts, every
material's pitch to a few cents, decay order, damping and the pedal, one voice per tine, rolls, the
delay's echo times, grains and freeze, the hole and the buzzers); flash storage; Felucca's update-path
tests against `build/fimba.fwsc` (when it exists); and the whole app in the simulator
(`tests/scenarios/*.kal`), with its screenshots and audio in `build/scenarios/`. One WAV per tine
material lands in `build/host/materials/`.

`host/build_host.sh` builds the simulator alone (`gen_font.py` needs Pillow; `PYTHON=` picks the
interpreter); `build/host/kalimba_host SCRIPT OUTDIR` runs one script (the command list is at the top
of `host/kalimba_host.c`).

## Install

From the command line (with the `requirements.txt` packages):

```
python3 tools/fm1_install.py build/fimba.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Installing firmware is at your own risk. Hold OCT− and OCT+ for 5 seconds for Felucca's update
mode. If the FM-1 no longer starts but reaches the chip's update mode (4C4A:8057 on USB),
`tools/fm1_rescue.sh` puts stock firmware back from a Mac; otherwise recovery needs
[FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## Before installing

How the FM-1 is protected, from the outermost layer in:

1. **The chip's own update mode can't be overwritten.** The first 16 KiB of flash hold JieLi's boot
   code and its update mode ("UBOOT", USB 4C4A:8057). Felucca's update loader never writes there,
   and neither does `tools/fm1_rescue.py`.
2. **A firmware that crashes while starting falls back.** After two failed starts the FM-1 comes
   up in safe mode (no sound, USB on, the installer works). After four it enters the chip's update
   mode. This code is FoMni's, unchanged.
3. **The package is checked.** `tools/check_fwsc.py` takes a `.fwsc` apart and verifies every CRC.
   Given a reference with `--ref`, it shows that everything but the app is byte-identical to that
   package: the boot head, the update loader, the region table and the EQ table. The installer
   checks the package's identity again on the device before writing.

The check, against FoMni's own package built with the same toolchain:

```
git clone --depth 1 https://github.com/charlesvestal/fm1-fomni build/fomni-ref
(cd build/fomni-ref && ./build.sh)
python3 tools/check_fwsc.py build/fimba.fwsc --app build/fimba.bin --ref build/fomni-ref/build/omni.fwsc
```

`tests/run_tests.sh` runs it too, whenever `build/fomni-ref` is there.

Before the first install, get the way back ready:

- Download M-VAVE's stock firmware (m-vave.com/download, FM-1 V15, "PC Firmware") and keep it.
  `tools/fm1_rescue.py` accepts only that exact file (it checks its SHA-256), and it is what you'd
  need if the FM-1 ever ends up in a crash loop.
- `python3 tools/fm1_install.py --info` reads the identity of the connected FM-1 without writing
  anything: run it first to see that the installer finds the device.

## Publishing a release

1. `./build.sh --release X.Y` (the identity, FM-1_8XXYYZZ, is what the installer checks).
2. `gh release create vX.Y build/fimba-X.Y.fwsc`.
