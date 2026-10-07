// SPDX-License-Identifier: GPL-3.0-only
// The one-file emulator (build/emu/FiMba-1.html) opened from file:// in headless Chrome: it must
// switch on, sound when a white key is pressed and when a MIDI note comes in, and draw its screen.
//   node tests/host/standalone_test.mjs CHROME   (exit 0: all of that happened)
import { spawn } from "node:child_process";
import { readFileSync, writeFileSync, rmSync } from "node:fs";
import { resolve } from "node:path";
const chrome = process.argv[2];
const page = resolve("build/emu/standalone_test.html"), profile = resolve("build/emu/standalone_profile");
const PROBE = "<script>\n// test probe: switch on, play w7, report the audio level and the screen\n(async () => {\n  const out = (t) => { const p = document.createElement(\"pre\"); p.id = \"probe\"; p.textContent = t; document.body.appendChild(p); };\n  try {\n    const AC = window.AudioContext;\n    let analyser = null;\n    window.AudioContext = function (o) { const ac = new AC(o); analyser = ac.createAnalyser(); analyser.fftSize = 2048;\n      const conn = AudioNode.prototype.connect;\n      AudioNode.prototype.connect = function (d, ...r) { if (d === ac.destination && this !== analyser) { conn.call(this, analyser); } return conn.call(this, d, ...r); };\n      return ac; };\n    document.getElementById(\"go\").click();\n    for (let i = 0; i < 100 && !document.getElementById(\"start\").hidden; i++) await new Promise((r) => setTimeout(r, 100));\n    if (!document.getElementById(\"start\").hidden) { out(\"FAIL did not switch on: \" + document.getElementById(\"status\").textContent); return; }\n    await new Promise((r) => setTimeout(r, 800));\n    node.port.postMessage({ type: \"keys\", mask: 1 << 12 });          // w7: C4\n    let peak = 0; const buf = new Float32Array(2048);\n    for (let i = 0; i < 30; i++) { await new Promise((r) => setTimeout(r, 30)); analyser.getFloatTimeDomainData(buf); for (const v of buf) peak = Math.max(peak, Math.abs(v)); }\n    node.port.postMessage({ type: \"keys\", mask: 0 });\n    node.port.postMessage({ type: \"midi\", st: 0x90, d1: 76, d2: 110 });\n    let mpeak = 0;\n    await new Promise((r) => setTimeout(r, 400));\n    for (let i = 0; i < 10; i++) { await new Promise((r) => setTimeout(r, 30)); analyser.getFloatTimeDomainData(buf); for (const v of buf) mpeak = Math.max(mpeak, Math.abs(v)); }\n    const px = document.getElementById(\"screen\").getContext(\"2d\").getImageData(0, 0, 240, 240).data;\n    let lit = 0; for (let i = 0; i < px.length; i += 4) lit += (px[i] + px[i + 1] + px[i + 2]) > 60;\n    out(`status=${document.getElementById(\"status\").textContent} peak=${peak.toFixed(3)} midi_peak=${mpeak.toFixed(3)} screen_lit=${lit} protocol=${location.protocol}`);\n  } catch (e) { out(\"FAIL \" + e); }\n})();\n</script>\n";
writeFileSync(page, readFileSync("build/emu/FiMba-1.html", "utf8").replace("</body>", PROBE + "</body>"));
rmSync(profile, { recursive: true, force: true });
const url = "file://" + page;
const p = spawn(chrome, ["--headless=new", "--disable-gpu", "--autoplay-policy=no-user-gesture-required", "--no-first-run",
  `--user-data-dir=${profile}`, "--remote-debugging-port=9333", "about:blank"], { stdio: "ignore" });
const done = (code, msg) => { console.log(msg); p.kill("SIGKILL"); process.exit(code); };
setTimeout(() => done(2, "TIMEOUT"), 60000);
let target;
for (let i = 0; i < 50 && !target; i++) {
  await new Promise((r) => setTimeout(r, 200));
  try { target = (await (await fetch("http://127.0.0.1:9333/json")).json()).find((t) => t.type === "page"); } catch {}
}
const ws = new WebSocket(target.webSocketDebuggerUrl);
let id = 0; const pending = new Map(); const logs = [];
const call = (method, params = {}) => new Promise((res) => { const i = ++id; pending.set(i, res); ws.send(JSON.stringify({ id: i, method, params })); });
ws.onmessage = (e) => { const m = JSON.parse(e.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m.result); pending.delete(m.id); }
  if (m.method === "Runtime.consoleAPICalled") logs.push(m.params.args.map((a) => a.value).join(" "));
  if (m.method === "Runtime.exceptionThrown") logs.push("EXCEPTION " + m.params.exceptionDetails.text + " " + (m.params.exceptionDetails.exception?.description || "")); };
await new Promise((r) => (ws.onopen = r));
await call("Runtime.enable");
await call("Page.navigate", { url });
for (let i = 0; i < 100; i++) {
  await new Promise((r) => setTimeout(r, 300));
  const r = await call("Runtime.evaluate", { expression: "document.getElementById('probe')?.textContent || ''" });
  const v = r?.result?.value;
  if (v) {
    const m = /peak=([\d.]+) midi_peak=([\d.]+) screen_lit=(\d+) protocol=file:/.exec(v);
    done(m && +m[1] > 0.05 && +m[2] > 0.02 && +m[3] > 10000 ? 0 : 1, v + (logs.length ? "\nconsole: " + logs.join(" | ") : ""));
  }
}
done(2, "no result; console: " + logs.join(" | "));
