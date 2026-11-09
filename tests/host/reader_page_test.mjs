// SPDX-License-Identifier: GPL-3.0-only
// The emulator's screen reader and keyboard access (build/emu/FiMba-1.html, from file:// in headless Chrome): the
// panel is inert until switched on; then the screen is said in words (the live region), a button held from the
// keyboard (Space on its target) changes the page and the new page is said, MASTER has a value, a lit button says so,
// and muted, a knob turned from the keyboard is still said (Mute must not stop the firmware).
//   node tests/host/reader_page_test.mjs CHROME   (exit 0: all of that happened)
import { spawn } from "node:child_process";
import { readFileSync, writeFileSync, rmSync } from "node:fs";
import { resolve } from "node:path";
const chrome = process.argv[2];
const page = resolve("build/emu/reader_page_test.html"), profile = resolve("build/emu/reader_page_profile");
const PROBE = `<script>
// test probe
(async () => {
  const out = (t) => { const p = document.createElement("pre"); p.id = "probe"; p.textContent = t; document.body.appendChild(p); };
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  const said = () => document.getElementById("said").textContent;
  const fails = [];
  try {
    if (!document.getElementById("controls").inert) fails.push("panel not inert before switching on");
    document.getElementById("go").click();
    for (let i = 0; i < 100 && !document.getElementById("start").hidden; i++) await wait(100);
    if (document.getElementById("controls").inert) fails.push("panel still inert");
    for (let i = 0; i < 40 && !said().includes("Tine page"); i++) await wait(100);
    if (!said().includes("Tine page: Material Steel")) fails.push("first screen not said: " + JSON.stringify(said()));
    const hits = [...document.querySelectorAll("#controls .hit")];
    const env = hits.find((h) => h.getAttribute("aria-label") === "ENV");
    const home = hits.find((h) => h.getAttribute("aria-label") === "HOME, lit");
    const mast = hits.find((h) => h.getAttribute("aria-label") === "MASTER");
    if (!home) fails.push("HOME not named lit: " + hits.slice(0, 20).map((h) => h.getAttribute("aria-label")).join("|"));
    if (!mast || !mast.getAttribute("aria-valuetext")) fails.push("MASTER has no value");
    if (!env || env.tabIndex !== 0) fails.push("ENV not focusable");
    env.focus();
    env.dispatchEvent(new KeyboardEvent("keydown", { key: " ", bubbles: true }));
    await wait(120);
    env.dispatchEvent(new KeyboardEvent("keyup", { key: " ", bubbles: true }));
    for (let i = 0; i < 30 && !said().includes("Body page"); i++) await wait(100);
    if (!said().includes("Body page")) fails.push("page from the keyboard not said: " + JSON.stringify(said()));
    // muted, the firmware keeps running: a knob turned from the keyboard is still said
    document.getElementById("mute").click();
    const k1 = hits.find((h) => h.getAttribute("aria-label") === "KNOB2");
    k1.focus();
    k1.dispatchEvent(new KeyboardEvent("keydown", { key: "ArrowUp", bubbles: true }));
    for (let i = 0; i < 30 && !said().includes("Buzz 1"); i++) await wait(100);
    if (!said().includes("Buzz 1")) fails.push("muted: a knob not said: " + JSON.stringify(said()));
    out(fails.length ? "FAIL " + fails.join("; ") : "ok said=" + said());
  } catch (e) { out("FAIL " + e); }
})();
</script>
`;
writeFileSync(page, readFileSync("build/emu/FiMba-1.html", "utf8").replace("</body>", PROBE + "</body>"));
rmSync(profile, { recursive: true, force: true });
const p = spawn(chrome, ["--headless=new", "--disable-gpu", "--autoplay-policy=no-user-gesture-required", "--no-first-run",
  `--user-data-dir=${profile}`, "--remote-debugging-port=9334", "about:blank"], { stdio: "ignore" });
const done = (code, msg) => { console.log(msg); p.kill("SIGKILL"); process.exit(code); };
setTimeout(() => done(2, "TIMEOUT"), 60000);
let target;
for (let i = 0; i < 50 && !target; i++) {
  await new Promise((r) => setTimeout(r, 200));
  try { target = (await (await fetch("http://127.0.0.1:9334/json")).json()).find((t) => t.type === "page"); } catch {}
}
const ws = new WebSocket(target.webSocketDebuggerUrl);
let id = 0; const pending = new Map(); const logs = [];
const call = (method, params = {}) => new Promise((res) => { const i = ++id; pending.set(i, res); ws.send(JSON.stringify({ id: i, method, params })); });
ws.onmessage = (e) => { const m = JSON.parse(e.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m.result); pending.delete(m.id); }
  if (m.method === "Runtime.exceptionThrown") logs.push("EXCEPTION " + m.params.exceptionDetails.text + " " + (m.params.exceptionDetails.exception?.description || "")); };
await new Promise((r) => (ws.onopen = r));
await call("Runtime.enable");
await call("Page.navigate", { url: "file://" + page });
for (let i = 0; i < 100; i++) {
  await new Promise((r) => setTimeout(r, 300));
  const v = (await call("Runtime.evaluate", { expression: "document.getElementById('probe')?.textContent || ''" }))?.result?.value;
  if (v) done(v.startsWith("ok") && !logs.length ? 0 : 1, v + (logs.length ? "\nconsole: " + logs.join(" | ") : ""));
}
done(2, "no result; exceptions: " + logs.join(" | "));
