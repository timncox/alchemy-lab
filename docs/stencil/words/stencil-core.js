/*
 * stencil-core.js -- the web page's half of core/stencil.c: the same Morse
 * table and timing, factory patterns and Euclidean rule, plus the two USB
 * wire formats (HostLink frames for the Alchemy Lab, SysEx for the Patch).
 * test/web_parity.mjs checks it against the C engine bit for bit.
 */
(function (root) {
  'use strict';

  const LETTERS = ['.-', '-...', '-.-.', '-..', '.', '..-.', '--.', '....', '..',
    '.---', '-.-', '.-..', '--', '-.', '---', '.--.', '--.-', '.-.',
    '...', '-', '..-', '...-', '.--', '-..-', '-.--', '--..'];
  const DIGITS = ['-----', '.----', '..---', '...--', '....-',
    '.....', '-....', '--...', '---..', '----.'];
  const PUNCT = {
    '.': '.-.-.-', ',': '--..--', '?': '..--..', "'": '.----.', '!': '-.-.--',
    '/': '-..-.', '(': '-.--.', ')': '-.--.-', '&': '.-...', ':': '---...',
    ';': '-.-.-.', '=': '-...-', '+': '.-.-.', '-': '-....-', '_': '..--.-',
    '"': '.-..-.', '$': '...-..-', '@': '.--.-.'
  };
  const MAX_UNITS = 1024;
  const WORD_MAX = 48;
  const WORDS = 8;

  function morseChar(c) {
    const u = c.toUpperCase();
    if (u >= 'A' && u <= 'Z' && u.length === 1) return LETTERS[u.charCodeAt(0) - 65];
    if (c >= '0' && c <= '9' && c.length === 1) return DIGITS[c.charCodeAt(0) - 48];
    return PUNCT[c] || null;
  }

  /* -> array of 0/1, ITU timing, 7-unit gap at the loop point (as C). */
  function morseEncode(text) {
    const bits = [];
    let any = false, space = false;
    const put = (n, v) => { for (let i = 0; i < n; i++) bits.push(v); };
    for (const ch of text) {
      if (ch === ' ') { space = true; continue; }
      const m = morseChar(ch);
      if (!m) continue;
      if (any) put(space ? 7 : 3, 0);
      space = false;
      [...m].forEach((e, i) => { if (i) put(1, 0); put(e === '-' ? 3 : 1, 1); });
      any = true;
    }
    if (!any) return [];
    put(7, 0);
    return bits.slice(0, MAX_UNITS);
  }

  /* What the firmware keeps of a typed word: printable ASCII, upper case, 48. */
  function cleanWord(text) {
    let out = '';
    for (const ch of text) {
      let c = ch;
      if (c >= 'a' && c <= 'z') c = c.toUpperCase();
      const k = c.charCodeAt(0);
      if (k < 0x20 || k > 0x7e) continue;
      out += c;
      if (out.length >= WORD_MAX) break;
    }
    return out;
  }

  const BANK = [
    'xxxxxxxxxxxxxxxx', 'x.x.x.x.x.x.x.x.', '.x.x.x.x.x.x.x.x', 'x...x...x...x...',
    '..x...x...x...x.', 'x..x..x.x..x..x.', 'xx.xx.xx.xx.x.x.', 'x.xx.xx.x.xx.x.x',
    'xxx.xxx.xxx.xx.x', 'x.x..x.x.x..x.x.', 'xx..xx..xx..x.x.', 'x..x.x..x..x.x..',
    'xxxx..xx..xxxx.x', 'x.xxx.x.xxx.x.xx', 'x......x......x.', 'xx.x.xx.xx.x.x.x'
  ];

  function euclidHit(i, k, n) {
    if (n === 0 || k === 0) return 0;
    if (k >= n) return 1;
    return ((i % n) * k) % n < k ? 1 : 0;
  }

  /* How a Morse word sits in the bar (st_fit_t). */
  const FITS = ['Free', 'Pad', '1 bar', '2 bars', '4 bars', '8 bars'];

  /* -> { bits, beats }: beats > 0 when the word is stretched over n bars. */
  function fitMorse(bits, fit, rateIndex) {
    if (!bits.length) return { bits: [1], beats: 0 };
    if (fit === 1) {
      const bar = Math.round(4 * RATES[rateIndex][1]);
      let padded = Math.ceil(bits.length / bar) * bar;
      while (padded > MAX_UNITS && padded >= bar) padded -= bar;
      const out = bits.slice();
      while (out.length < padded) out.push(0);
      return { bits: out, beats: 0 };
    }
    if (fit >= 2 && fit <= 5) return { bits: bits.slice(), beats: 4 << (fit - 2) };
    return { bits: bits.slice(), beats: 0 };
  }

  const RATES = [['1/32', 8], ['1/16T', 6], ['1/16', 4], ['1/8T', 3], ['1/8', 2],
    ['1/4T', 1.5], ['1/4', 1], ['1/2', 0.5], ['1/1', 0.25]];

  /* ---- HostLink (Alchemy Lab, WebSerial) ---------------------------------- */

  const CRC_TABLE = (() => {
    const t = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
      let c = n;
      for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
      t[n] = c >>> 0;
    }
    return t;
  })();
  function crc32(bytes) {
    let c = 0xffffffff;
    for (const b of bytes) c = CRC_TABLE[(c ^ b) & 0xff] ^ (c >>> 8);
    return (c ^ 0xffffffff) >>> 0;
  }

  function cobsEncode(src) {
    const out = [0];
    let code = 1, codeAt = 0;
    for (const b of src) {
      if (b === 0) { out[codeAt] = code; codeAt = out.length; out.push(0); code = 1; }
      else {
        out.push(b); code++;
        if (code === 0xff) { out[codeAt] = code; codeAt = out.length; out.push(0); code = 1; }
      }
    }
    out[codeAt] = code;
    return out;
  }
  function cobsDecode(src) {
    const out = [];
    let i = 0;
    while (i < src.length) {
      const code = src[i++];
      if (code === 0) return null;
      for (let j = 1; j < code; j++) { if (i >= src.length) return null; out.push(src[i++]); }
      if (code < 0xff && i < src.length) out.push(0);
    }
    return out;
  }

  /* One request -> wire bytes (COBS + 0x00 delimiter). */
  function hostlinkFrame(type, seq, body) {
    const f = [1, type, seq & 0xff, (seq >> 8) & 0xff, body.length & 0xff, (body.length >> 8) & 0xff, ...body];
    const c = crc32(f);
    f.push(c & 0xff, (c >>> 8) & 0xff, (c >>> 16) & 0xff, (c >>> 24) & 0xff);
    return [...cobsEncode(f), 0];
  }
  /* Wire chunk (without the delimiter) -> {type, seq, body} or null. */
  function hostlinkParse(chunk) {
    const f = cobsDecode(chunk);
    if (!f || f.length < 10 || f[0] !== 1) return null;
    const len = f[4] | (f[5] << 8);
    if (f.length !== 6 + len + 4) return null;
    const c = crc32(f.slice(0, 6 + len));
    const got = (f[6 + len] | (f[7 + len] << 8) | (f[8 + len] << 16) | (f[9 + len] << 24)) >>> 0;
    if (c !== got) return null;
    return { type: f[1], seq: f[2] | (f[3] << 8), body: f.slice(6, 6 + len) };
  }

  /* ---- Stencil commands (the payloads st_apply_cmd() takes) --------------- */

  const CMD = { SET_WORD: 1, SET_USER: 2, SELECT: 3, SET_FIT: 4 };
  const SRC = { BANK: 0, EUCLID: 1, MORSE: 2 };

  function cmdSetWord(slot, text) {
    return [CMD.SET_WORD, slot, ...[...cleanWord(text)].map(c => c.charCodeAt(0))];
  }
  /* steps: array of 0/1, length 1..32 */
  function cmdSetUser(steps) {
    let m = 0n;
    steps.forEach((v, i) => { if (v) m |= 1n << BigInt(i); });
    const g = [];
    for (let i = 0; i < 5; i++) g.push(Number((m >> BigInt(7 * i)) & 0x7fn));
    return [CMD.SET_USER, steps.length, ...g];
  }
  function cmdSelect(source, index) { return [CMD.SELECT, source, index]; }
  function cmdSetFit(fit) { return [CMD.SET_FIT, fit]; }

  /* SysEx for the Patch: F0 7D 53 54 <cmd> <payload> F7. */
  function sysex(cmd) { return [0xf0, 0x7d, 0x53, 0x54, ...cmd, 0xf7]; }

  const api = {
    morseChar, morseEncode, cleanWord, BANK, euclidHit, RATES, FITS, fitMorse, cmdSetFit, MAX_UNITS, WORD_MAX, WORDS,
    crc32, cobsEncode, cobsDecode, hostlinkFrame, hostlinkParse,
    CMD, SRC, cmdSetWord, cmdSetUser, cmdSelect, sysex
  };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  root.StencilCore = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
