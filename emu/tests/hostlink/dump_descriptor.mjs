#!/usr/bin/env node
// Print a firmware's HostLink descriptor (pretty JSON) from the native emulator:
//   node tests/hostlink/dump_descriptor.mjs build/belt/emu > belt.json
import { spawn } from "node:child_process";
import { createConnection } from "node:net";
import { mkdtempSync, writeFileSync, existsSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { HostLink } from "../../web/hostlink.js";

const exe = process.argv[2];
if (!exe) { console.error("usage: dump_descriptor.mjs <emu exe>"); process.exit(2); }
const dir = mkdtempSync(join(tmpdir(), "hl-"));
const sockPath = join(dir, "hl.sock"), script = join(dir, "idle.emu");
writeFileSync(script, "wait 30000\n");
const emu = spawn(exe, ["--headless", "--script", script, "--flash", "none", "--usb", "mac", "--hostlink", sockPath], { stdio: "ignore" });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
try {
  let sock = null;
  for (let i = 0; i < 100 && !sock; i++) {
    if (existsSync(sockPath)) sock = await new Promise((res) => {
      const s = createConnection(sockPath); s.once("connect", () => res(s)); s.once("error", () => res(null));
    });
    if (!sock) await sleep(100);
  }
  if (!sock) throw new Error("no socket");
  const link = new HostLink({ write: (u8) => new Promise((r) => sock.write(u8, r)),
                              onData: (cb) => sock.on("data", (b) => cb(new Uint8Array(b))) });
  let id = null;
  for (let i = 0; i < 20 && !id; i++) id = await link.hello().catch(() => null);
  process.stdout.write(JSON.stringify(await link.descriptor(), null, 1) + "\n");
  sock.destroy();
} finally {
  emu.kill("SIGKILL");
  rmSync(dir, { recursive: true, force: true });
}
process.exit(0);
