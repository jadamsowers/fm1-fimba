// SPDX-License-Identifier: GPL-3.0-only
// The installer on the page, end to end in headless Chrome: build/emu/FiMba-1.html served as GitHub
// Pages serves it (with firmware/latest.json and the package beside it), Web MIDI replaced by a
// simulated FM-1 (tests/web/fake_fm1.mjs), then the buttons pressed: "Install FiMba-1 <version>" (the
// latest release), and a chosen .fwsc file. Each must end "Done", with every read answered right.
//   node tests/web/installer_page_test.mjs CHROME      (after web/emu/build.sh and ./build.sh)
import { spawn } from "node:child_process";
import { createServer } from "node:http";
import { readFileSync, rmSync, mkdtempSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";

const chrome = process.argv[2];
const pkg = readFileSync("build/fimba.fwsc");
/* the identity, as the page reads it: one marker byte after each of the first 20 blocks of 0x2F */
let product = "";
for (let i = 0; i < 20; i++) { const m = pkg[(i + 1) * 0x2F + i]; if (m !== 0x7D) product += String.fromCharCode((m - i - 1) & 0xFF); }
const files = {
  "/": [readFileSync("build/emu/FiMba-1.html"), "text/html"],
  "/firmware/latest.json": [JSON.stringify({ version: "9.9.9", product, file: "fimba.fwsc" }), "application/json"],
  "/firmware/fimba.fwsc": [pkg, "application/octet-stream"],
};
const server = createServer((q, r) => {
  const f = files[q.url.split("?")[0]];
  if (!f) { r.writeHead(404); r.end(); return; }
  r.writeHead(200, { "content-type": f[1] });
  r.end(f[0]);
}).listen(0, "127.0.0.1");
await new Promise((r) => server.on("listening", r));
const url = `http://127.0.0.1:${server.address().port}/`;

/* the simulated FM-1, in the page before its own scripts run, as navigator.requestMIDIAccess */
const fake = readFileSync("tests/web/fake_fm1.mjs", "utf8").replace(/^export\s+/m, "");
const inject = fake + `
navigator.requestMIDIAccess = async () => {
  if (!window.__fm1) {
    const raw = new Uint8Array(await (await fetch("firmware/fimba.fwsc")).arrayBuffer());
    const img = raw.filter((_, i) => !(i < 20 * 0x30 && i % 0x30 === 0x2F));   /* the markers out */
    window.__fm1 = new FakeFM1(img, { after: ${JSON.stringify(product)}, reads: 24 });
  }
  return window.__fm1.access;
};`;

const profile = mkdtempSync(join(tmpdir(), "fimba-chrome-"));
const port = 9300 + Math.floor(Math.random() * 400);
const p = spawn(chrome, ["--headless=new", "--disable-gpu", "--no-first-run", "--autoplay-policy=no-user-gesture-required",
  `--user-data-dir=${profile}`, `--remote-debugging-port=${port}`, "about:blank"], { stdio: "ignore" });
let failed = 0;
const ok = (cond, what) => { console.log(`${what.padEnd(70)} ${cond ? "ok" : "FAIL"}`); if (!cond) failed++; };
const finish = (code) => { p.kill("SIGKILL"); server.close(); rmSync(profile, { recursive: true, force: true }); process.exit(code); };
setTimeout(() => { console.log("TIMEOUT"); finish(2); }, 150000);

let target;
for (let i = 0; i < 50 && !target; i++) {
  await new Promise((r) => setTimeout(r, 200));
  try { target = (await (await fetch(`http://127.0.0.1:${port}/json`)).json()).find((t) => t.type === "page"); } catch {}
}
const ws = new WebSocket(target.webSocketDebuggerUrl);
let id = 0; const pending = new Map(); const errors = [];
const call = (method, params = {}) => new Promise((res) => { const i = ++id; pending.set(i, res); ws.send(JSON.stringify({ id: i, method, params })); });
ws.onmessage = (e) => { const m = JSON.parse(e.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m.result); pending.delete(m.id); }
  if (m.method === "Runtime.exceptionThrown") errors.push(m.params.exceptionDetails.exception?.description || m.params.exceptionDetails.text); };
await new Promise((r) => (ws.onopen = r));
await call("Runtime.enable");
await call("Page.enable");
await call("DOM.enable");
await call("Page.addScriptToEvaluateOnNewDocument", { source: inject });
await call("Page.navigate", { url });
const js = async (expr) => (await call("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true }))?.result?.value;
const until = async (expr, ms) => { const end = Date.now() + ms; for (;;) { const v = await js(expr); if (v || Date.now() > end) return v; await new Promise((r) => setTimeout(r, 250)); } };

const label = await until(`!document.getElementById("instLatest").disabled && document.getElementById("instLatest").textContent`, 10000);
ok(label === "Install FiMba-1 9.9.9", `the latest release found: "${label}"`);
await js(`document.getElementById("instLatest").click()`);
let st = await until(`(s => /^Done|Error|not |lost|stopped/.test(s) && s)(document.getElementById("instStatus").textContent)`, 60000);
ok(/^Done: the FM-1 has restarted with FiMba-1 9\.9\.9/.test(st || ""), `install the latest: "${st}"`);
ok(await js(`window.__fm1.bad === 0 && window.__fm1.served > 20`), "every read answered with the package's bytes");
ok(await js(`document.getElementById("instBar").value === 100`), "the progress bar full");

/* a chosen file: the file input, then its button */
const doc = await call("DOM.getDocument");
const q = await call("DOM.querySelector", { nodeId: doc.root.nodeId, selector: "#instFile" });
await call("DOM.setFileInputFiles", { nodeId: q.nodeId, files: [resolve("build/fimba.fwsc")] });
const ready = await until(`!document.getElementById("instFileGo").disabled && document.getElementById("instStatus").textContent`, 5000);
ok(/fimba\.fwsc: FM-1_8/.test(ready || ""), `a chosen file read: "${ready}"`);
await js(`window.__fm1 = null; document.getElementById("instFileGo").click()`);
st = await until(`(s => /^Done|Error|not |lost|stopped/.test(s) && s)(document.getElementById("instStatus").textContent)`, 60000);
ok(/^Done/.test(st || "") && await js(`window.__fm1.bad === 0`), `install a chosen file: "${st}"`);
ok(errors.length === 0, `no script errors${errors.length ? ": " + errors.join(" | ") : ""}`);
console.log(failed ? `INSTALLER PAGE TESTS FAILED (${failed})` : "installer page tests passed");
finish(failed ? 1 : 0);
