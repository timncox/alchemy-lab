#!/usr/bin/env node
// Belt's chord progression written over HostLink is what the chord sequencer
// plays -- not just what GET_LIVE reads back. (Before belt-alchemy web-chords,
// Belt took the progression from its extras only at boot.)
//
//   node tests/hostlink/belt_prog_test.mjs build/belt-trunk/emu
//
// Over HostLink: Hide and Seek (LEAD 0, voices Off), C major, Chords from =
// Seq, 240 BPM, and a ONE-chord progression: IV. The emulator's own script
// then listens: IV = F3 A3 C4 must sound at every check, and the default
// progression's chords (I = C3, iii = E3 G3 B3, vii = B3 D4 F4) must not.
import { spawn } from "node:child_process";
import { createConnection } from "node:net";
import { mkdtempSync, writeFileSync, existsSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { HostLink, encodeState } from "../../web/hostlink.js";

// Belt's extras (belt-alchemy src/extras.h), 'BLT' layout 3: the progression
// is count at +8, int8 degree[8] at +9, uint8 bars[8] at +17.
const EXTRAS_HASH = 0x424c5403;

const exe = process.argv[2];
if (!exe) { console.error("usage: belt_prog_test.mjs <belt emu exe>"); process.exit(2); }
const dir = mkdtempSync(join(tmpdir(), "hl-"));
const sockPath = join(dir, "hl.sock"), script = join(dir, "listen.emu");
const checks = [];
for (let i = 0; i < 5; i++) {
  checks.push("wait 700",
    "expect tone 174.61 > 0.01   # F3: IV",
    "expect tone 130.81 < 0.004  # not C3 (I)",
    "expect tone 246.94 < 0.004  # not B3 (iii, vii)",
    "expect tone 164.81 < 0.008  # not E3 (iii; F3 a semitone up leaks ~0.0035)");
}
writeFileSync(script, ["wait 7000", "sing 220", "wait 2500", ...checks, "expect alive", ""].join("\n"));
const emu = spawn(exe, ["--headless", "--script", script, "--flash", "none", "--usb", "mac", "--hostlink", sockPath],
  { stdio: ["ignore", "pipe", "inherit"] });
let out = "";
emu.stdout.on("data", (b) => { out += b; });
const done = new Promise((res) => emu.on("exit", (code) => res(code)));
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

try {
  let sock = null;
  for (let i = 0; i < 100 && !sock; i++) {
    if (existsSync(sockPath)) sock = await new Promise((res) => {
      const s = createConnection(sockPath); s.once("connect", () => res(s)); s.once("error", () => res(null));
    });
    if (!sock) await sleep(100);
  }
  const link = new HostLink({ write: (u8) => new Promise((r) => sock.write(u8, r)),
                              onData: (cb) => sock.on("data", (b) => cb(new Uint8Array(b))) });
  let id = null;
  for (let i = 0; i < 25 && !id; i++) id = await link.hello().catch(() => null);
  const desc = await link.descriptor();
  const ex = desc.components.find((c) => c.hash === EXTRAS_HASH);
  if (!ex) throw new Error("no Belt extras (layout 3) in the descriptor");
  let live = encodeState(desc, await link.getLive(), {
    key: 0, scale: 1 / 8, lead: 0, chord_src: 1, chord_bpm: 1, chord_oct: 1, chord_tones: 0, j3_mode: 0,
    harm1: 0, harm2: 0, harm3: 0, harm4: 0, double_amt: 0, midi_mode: 1,
  });
  live[ex.off + 8] = 1;            // one chord
  live[ex.off + 9] = 3;            // IV
  live[ex.off + 17] = 1;           // one bar
  await link.setLive(live);
  const back = await link.getLive();
  console.log(`wrote the progression: count ${back[ex.off + 8]}, degree ${back[ex.off + 9]}, bars ${back[ex.off + 17]}`);
  sock.destroy();
} catch (e) {
  console.log(`FAIL  setup: ${e.message}`);
  emu.kill("SIGKILL");
}
const code = await done;
rmSync(dir, { recursive: true, force: true });
process.stdout.write(out.split("\n").filter((l) => /PASS|FAIL/.test(l)).join("\n") + "\n");
console.log(code === 0 && !/FAIL/.test(out) ? "all passed" : "FAILED");
process.exit(code === 0 && !/FAIL/.test(out) ? 0 : 1);
