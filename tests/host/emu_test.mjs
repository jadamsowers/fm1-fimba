// SPDX-License-Identifier: GPL-3.0-only
// The browser build in Node: boot it, play across the white keys, expect sound and screen frames;
// then a note over web_midi.
import fs from "fs";
const wasm = fs.readFileSync(process.argv[2]);
let mem;
const wasi = new Proxy({ clock_time_get: (id, p, out) => { new DataView(mem.buffer).setBigUint64(out, 0n, true); return 0; },
  fd_write: (fd, iov, n, out) => { new DataView(mem.buffer).setUint32(out, 0, true); return 0; } }, { get: (t, k) => k in t ? t[k] : () => 0 });
const { instance } = await WebAssembly.instantiate(wasm, { wasi_snapshot_preview1: wasi, env: new Proxy({}, { get: () => () => 0 }) });
const ex = instance.exports; mem = ex.memory;
if (ex._initialize) ex._initialize();
ex.web_master(2800); ex.web_boot();
let peak = 0;
const WHITE = [0,2,4,6,7,9,11,12,14,16,18,19,21,23,24,26];
for (let i = 0; i < 400; i++) {
  if (i % 20 === 0 && i < 340) ex.web_keys(1 << WHITE[(i / 20) % 16]);
  ex.web_render(128);
  const l = new Float32Array(mem.buffer, ex.web_out_l(), 128);
  for (const v of l) peak = Math.max(peak, Math.abs(v));
}
ex.web_keys(0);
for (let i = 0; i < 1400; i++) ex.web_render(128);          // ring out
ex.web_midi(0x90, 72, 110);
let midiPeak = 0;
for (let i = 0; i < 100; i++) {
  ex.web_render(128);
  for (const v of new Float32Array(mem.buffer, ex.web_out_l(), 128)) midiPeak = Math.max(midiPeak, Math.abs(v));
}
console.log("peak", peak.toFixed(3), "midi peak", midiPeak.toFixed(3), "blits", ex.web_blits());
process.exit(peak > 0.05 && peak < 1 && midiPeak > 0.05 && ex.web_blits() > 10 ? 0 : 1);
