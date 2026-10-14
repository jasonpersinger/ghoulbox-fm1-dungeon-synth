// SPDX-License-Identifier: GPL-3.0-only
// Browser-emulator MIDI panel mapping and portable state files. Kept free of DOM and
// AudioWorklet APIs so every boundary can be tested in Node.

export const STORE_AREAS = [[0x97000, 0xE0000], [0xE5000, 0xE8000], [0xFC000, 0x100000]];
export const STATE_FORMAT = "ghoulbox-emulator-state";
export const STATE_VERSION = 1;

export const PANEL_KNOBS = [
  { cc: 112, name: "MASTER", role: "master" },
  { cc: 113, name: "SELECT", role: 0 },
  { cc: 114, name: "PRESETS", role: 2 },
  { cc: 115, name: "ALGORITHM", role: 1 },
  { cc: 116, name: "KNOB 1", role: 3 },
  { cc: 117, name: "KNOB 2", role: 4 },
  { cc: 118, name: "KNOB 3", role: 5 },
  { cc: 119, name: "KNOB 4", role: 6 },
];

/* Skip CC32 (Bank Select LSB) and CC38 (Data Entry LSB). None of these is in
 * GHOULBOX's direct sound-control map; panel messages are consumed by the page. */
export const PANEL_BUTTONS = [
  { cc: 25, name: "FX", bit: 0 }, { cc: 26, name: "SCL", bit: 1 },
  { cc: 27, name: "ENV", bit: 2 }, { cc: 28, name: "LFO", bit: 3 },
  { cc: 29, name: "EDIT", bit: 4 }, { cc: 30, name: "GLO", bit: 5 },
  { cc: 31, name: "HOME", bit: 6 }, { cc: 33, name: "SAVE", bit: 7 },
  { cc: 34, name: "ARP", bit: 8 }, { cc: 35, name: "SEQ", bit: 9 },
  { cc: 36, name: "PLAY", bit: 10 }, { cc: 37, name: "REC", bit: 11 },
  { cc: 39, name: "OCT-", bit: 12 }, { cc: 40, name: "OCT+", bit: 13 },
];

const KNOB_BY_CC = new Map(PANEL_KNOBS.map((x) => [x.cc, x]));
const BUTTON_BY_CC = new Map(PANEL_BUTTONS.map((x) => [x.cc, x]));

export function panelCcAction(cc, value, last, mode = "absolute") {
  cc |= 0; value = Math.max(0, Math.min(127, value | 0));
  const button = BUTTON_BY_CC.get(cc);
  if (button) return { type: "button", ...button, down: value >= 64 };
  const knob = KNOB_BY_CC.get(cc);
  if (!knob) return null;
  if (knob.role === "master") return { type: "master", ...knob, value: Math.round(value * 1023 / 127) };
  let n = 0;
  if (mode === "relative") {
    n = value > 0 && value < 64 ? value : value > 64 ? value - 128 : 0;
  } else {
    const before = last.get(cc);
    last.set(cc, value);
    if (before === undefined) return { type: "seed", ...knob };
    n = value - before;
  }
  n = Math.max(-8, Math.min(8, n));
  return n ? { type: "encoder", ...knob, n } : { type: "seed", ...knob };
}

export function midiKeyIndex(note) {
  note |= 0;
  return note >= 53 && note <= 79 ? note - 53 : -1;       // F3 .. G5, the FM-1's 27 keys
}

export function panelNoteAction(data, layerActive, diverted) {
  const d = Array.from(data || []), kind = (d[0] || 0) & 0xF0;
  if (d.length < 2 || (kind !== 0x80 && kind !== 0x90)) return null;
  const id = `mn${d[0] & 15}:${d[1] & 127}`;
  const on = kind === 0x90 && (d[2] || 0) > 0, off = kind === 0x80 || kind === 0x90 && !(d[2] || 0);
  if (off && diverted.delete(id)) return { type: "key", id, key: midiKeyIndex(d[1]), down: false };
  const key = midiKeyIndex(d[1]);
  if (on && key >= 0 && layerActive) {
    diverted.add(id);
    return { type: "key", id, key, down: true };
  }
  return null;
}

export function fixedVelocityMessage(data, velocity) {
  const out = Array.from(data || [], (x) => x & 0xFF);
  if (out.length >= 3 && (out[0] & 0xF0) === 0x90 && out[2] && velocity)
    out[2] = Math.max(1, Math.min(127, velocity | 0));
  return out;
}

export function storedSectorOffset(off) {
  return Number.isInteger(off) && !(off & 4095) && STORE_AREAS.some(([a, b]) => off >= a && off + 4096 <= b);
}

export function stateCrc(bytes) {
  let c = 0xFFFFFFFF;
  for (const b of bytes) {
    c ^= b;
    for (let k = 0; k < 8; k++) c = c >>> 1 ^ (c & 1 ? 0xEDB88320 : 0);
  }
  return (~c) >>> 0;
}

export function bytesToBase64(bytes) {
  let s = "";
  for (let i = 0; i < bytes.length; i += 4096)
    s += String.fromCharCode.apply(null, bytes.subarray(i, i + 4096));
  return btoa(s);
}

export function base64ToBytes(data, size) {
  if (typeof data !== "string" || data.length !== 4 * Math.ceil(size / 3) ||
      !/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(data))
    throw new Error("Invalid state bytes");
  const bytes = Uint8Array.from(atob(data), (c) => c.charCodeAt(0));
  if (bytes.length !== size) throw new Error("Wrong state-object size");
  return bytes;
}

const stateObject = (bytes, extra = {}) => ({
  ...extra, size: bytes.length, crc: stateCrc(bytes), data: bytesToBase64(bytes),
});

export function makeEmulatorState(sectors, runtime, firmware = "") {
  if (!(runtime instanceof Uint8Array) || !runtime.length || runtime.length > 3840)
    throw new Error("No live project to export");
  const saved = Object.entries(sectors || {}).map(([key, bytes]) => {
    const offset = +key;
    if (!storedSectorOffset(offset) || !(bytes instanceof Uint8Array) || bytes.length !== 4096)
      throw new Error("Invalid saved flash sector");
    return stateObject(bytes, { offset });
  }).sort((a, b) => a.offset - b.offset);
  return {
    format: STATE_FORMAT, version: STATE_VERSION, firmware,
    created: new Date().toISOString(), runtime: stateObject(runtime), sectors: saved,
  };
}

function readObject(o, max, exact = 0) {
  if (!o || !Number.isInteger(o.size) || o.size <= 0 || o.size > max || (exact && o.size !== exact) ||
      !Number.isInteger(o.crc) || o.crc < 0 || o.crc > 0xFFFFFFFF)
    throw new Error("Invalid state object");
  const bytes = base64ToBytes(o.data, o.size);
  if (stateCrc(bytes) !== o.crc) throw new Error("State checksum mismatch");
  return bytes;
}

export function readEmulatorState(file) {
  if (typeof file === "string") file = JSON.parse(file);
  if (!file || file.format !== STATE_FORMAT || file.version !== STATE_VERSION ||
      !Array.isArray(file.sectors) || !file.runtime)
    throw new Error("Not a GHOULBOX emulator state file");
  const sectors = {}, seen = new Set();
  let maxSectors = 0;
  for (const [a, b] of STORE_AREAS) maxSectors += (b - a) / 4096;
  if (file.sectors.length > maxSectors) throw new Error("Too many flash sectors");
  for (const o of file.sectors) {
    if (!o || !storedSectorOffset(o.offset) || seen.has(o.offset)) throw new Error("Invalid flash-sector offset");
    seen.add(o.offset);
    sectors[o.offset] = readObject(o, 4096, 4096);
  }
  return { file, sectors, runtime: readObject(file.runtime, 3840) };
}
