// SPDX-License-Identifier: GPL-3.0-only
// The installer as the page ships it: the update protocol inlined in build/emu/FiMba-1.html, taken out
// of the page and run against a simulated FM-1 (tests/web/fake_fm1.mjs; after Felucca's
// web/test_web.mjs), and the real package read the way the page reads it.
//   node tests/web/updater_test.mjs        (after web/emu/build.sh; uses build/fimba.fwsc if there)
import { existsSync, readFileSync } from "node:fs";
import { FakeFM1 } from "./fake_fm1.mjs";

let failed = 0;
const ok = (cond, what) => { console.log(`${what.padEnd(70)} ${cond ? "ok" : "FAIL"}`); if (!cond) failed++; };

const page = readFileSync("build/emu/FiMba-1.html", "utf8");
const a = page.indexOf("/* ---- web/fm1pkg.js (inlined) ---- */"), b = page.indexOf("const $ = (id) =>", a);
ok(a > 0 && b > a, "the page carries the inlined update protocol");
const lib = page.slice(a, b) + "\nexport { Updater, productOf, logicalImage };";
const { Updater, productOf, logicalImage } = await import("data:text/javascript;base64," + Buffer.from(lib).toString("base64"));

const image = Uint8Array.from({ length: 0x2000 }, (_, i) => (i * 7) & 0xFF);
const steps = [];
const dev = new FakeFM1(image, { after: "FM-1_8000100" });
const got = await new Updater(dev.access).install(image, "FM-1_8000100", (k) => steps.push(k));
ok(got === "FM-1_8000100" && dev.bad === 0 && steps.includes("write") && steps.at(-1) === "done",
  `install: running firmware -> loader -> the new firmware (${dev.served} reads, all right)`);

const dev2 = new FakeFM1(image, { unplugAfter: 3 });
dev2.boot("ota-FM-1_900", "Felucca Update");
const t0 = Date.now();
ok(await new Updater(dev2.access).resume(image) === false && Date.now() - t0 < 6000, "unplugged during the write: stops at once");

const dev3 = new FakeFM1(image, { unplugAfter: 2 });
const e = await new Updater(dev3.access).install(image, "FM-1_800").then(() => null, (x) => x);
ok(e && e.code === "lost", "unplugged in step 1: 'lost', nothing written");
const e2 = await new Updater({ inputs: new Map(), outputs: new Map() }).install(image, "FM-1_800").then(() => null, (x) => x);
ok(e2 && e2.code === "notfound", "no FM-1 connected: 'notfound'");
const dev4 = new FakeFM1(image, { after: "FM-1_015" });
const e3 = await new Updater(dev4.access).install(image, "FM-1_800").then(() => null, (x) => x);
ok(e3 && e3.code === "mismatch" && e3.detail === "FM-1_015", "it comes back as something else: 'mismatch', with what it reports");

if (existsSync("build/fimba.fwsc")) {
  const raw = new Uint8Array(readFileSync("build/fimba.fwsc"));
  const img = logicalImage(raw);
  ok(/^FM-1_8\d*$/.test(productOf(raw)) && img.length === raw.length - 20, `build/fimba.fwsc: ${productOf(raw)}, its image ${img.length} B`);
} else {
  console.log(`${"build/fimba.fwsc (no firmware build)".padEnd(70)} skip`);
}
console.log(failed ? `UPDATER TESTS FAILED (${failed})` : "updater tests passed");
process.exit(failed ? 1 : 0);
