#!/usr/bin/env node
// HostLink end to end against the emulator: the firmware's own hostlink::Host
// over --hostlink <socket>, driven by web/hostlink.js (the same client the web
// pages use).
//
//   node tests/hostlink/hostlink_test.mjs build/belt/emu [build/smack/emu ...]
//
// For each firmware: HELLO, descriptor (CRC, schema, size), GET_LIVE, a live
// edit read back, save to a slot, a second edit, load the slot back, READ_SLOT
// equals the live state. Exits 1 on any failure.
import { spawn } from "node:child_process";
import { createConnection } from "node:net";
import { mkdtempSync, writeFileSync, existsSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { HostLink, decodeState, encodeState, fieldsOf } from "../../web/hostlink.js";

let failed = 0;
function check(name, ok, got) {
  console.log(`${ok ? "PASS" : "FAIL"}  ${name}${got !== undefined ? `  (${got})` : ""}`);
  if (!ok) failed++;
}
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const near = (a, b) => Math.abs(a - b) < 1e-6;

async function connect(path) {
  for (let i = 0; i < 100; i++) {
    if (existsSync(path)) {
      const sock = await new Promise((res) => {
        const s = createConnection(path);
        s.once("connect", () => res(s));
        s.once("error", () => res(null));
      });
      if (sock) return sock;
    }
    await sleep(100);
  }
  throw new Error(`no socket at ${path}`);
}

async function testOne(exe) {
  console.log(`\n== ${exe}`);
  const dir = mkdtempSync(join(tmpdir(), "hl-"));
  const sockPath = join(dir, "hl.sock");
  const script = join(dir, "idle.emu");
  writeFileSync(script, "wait 60000\n");
  // --usb mac: Tim's firmwares serve HostLink only with Settings' USB port on
  // Mac, and Seq's default is Launchpad
  const emu = spawn(exe, ["--headless", "--script", script, "--flash", "none", "--usb", "mac", "--hostlink", sockPath],
    { stdio: ["ignore", "ignore", "inherit"] });
  try {
    const sock = await connect(sockPath);
    const link = new HostLink({
      write: (u8) => new Promise((res) => sock.write(u8, res)),
      onData: (cb) => sock.on("data", (b) => cb(new Uint8Array(b))),
    });
    // the firmware polls HostLink once its boot is done; give it a moment
    let id = null;
    for (let i = 0; i < 20 && !id; i++) id = await link.hello().catch(() => null);
    check("HELLO answers", !!id, id && `${id.moduleId} "${id.moduleName}" fw ${id.fw} sdk ${id.sdk}`);
    if (!id) return;
    check("board is V2", id.board === 2, id.board);

    const desc = await link.descriptor();
    check("descriptor parses, CRC ok", !!desc.components, `${desc.components.length} components`);
    check("descriptor has no error", !desc.error, desc.error);
    check("schema matches HELLO", desc.schemaHash === id.schemaHash, desc.schemaHash);
    check("size matches live_size", desc.size === id.liveSize, `${desc.size} / ${id.liveSize}`);

    const live = await link.getLive();
    check("GET_LIVE returns the whole state", live.length === desc.size, live.length);

    const f = fieldsOf(desc).find((x) => x.type === "f32");
    check("there is an f32 field to edit", !!f, f && `${f.id} "${f.name}" (${f.component})`);
    if (!f) return;
    const before = decodeState(desc, live)[f.id];
    const a = before > 0.5 ? 0.125 : 0.875;
    await link.setLive(encodeState(desc, live, { [f.id]: a }));
    const live2 = await link.getLive();
    check(`live edit reads back: ${f.id}`, near(decodeState(desc, live2)[f.id], a),
      `${before.toFixed(3)} -> ${decodeState(desc, live2)[f.id].toFixed(3)}`);

    const slot = 3;
    await link.saveToSlot(slot);
    const slots = await link.listSlots();
    check(`slot ${slot} valid and ours after save`, slots[slot].valid && slots[slot].ours,
      JSON.stringify(slots[slot]));

    await link.setLive(encodeState(desc, live2, { [f.id]: 0.5 }));
    const live3 = await link.getLive();
    check("second edit took", near(decodeState(desc, live3)[f.id], 0.5), decodeState(desc, live3)[f.id]);

    await link.loadFromSlot(slot);
    const live4 = await link.getLive();
    check("load slot restores the saved value", near(decodeState(desc, live4)[f.id], a),
      decodeState(desc, live4)[f.id]);
    const stored = await link.readSlot(slot);
    check("READ_SLOT equals the live state",
      stored.length === live4.length && stored.every((x, i) => x === live4[i]), `${stored.length} B`);

    // a slot write straight from the host, then load it
    await link.writeBlob(5, encodeState(desc, live4, { [f.id]: 0.25 }));
    await link.loadFromSlot(5);
    check("host-written slot loads", near(decodeState(desc, await link.getLive())[f.id], 0.25));

    sock.destroy();
  } finally {
    emu.kill("SIGKILL");
    rmSync(dir, { recursive: true, force: true });
  }
}

const exes = process.argv.slice(2);
if (!exes.length) { console.error("usage: hostlink_test.mjs <emu exe>..."); process.exit(2); }
for (const exe of exes) {
  try { await testOne(exe); } catch (e) { check(`${exe} ran`, false, e.message); }
}
console.log(failed ? `\n${failed} FAILED` : "\nall passed");
process.exit(failed ? 1 : 0);
