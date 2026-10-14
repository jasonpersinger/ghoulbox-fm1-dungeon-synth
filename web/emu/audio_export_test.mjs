// SPDX-License-Identifier: GPL-3.0-only
import {
  Pcm24Recorder, WAV_FRAME_BYTES, recordingTime, wavHeader, writePcm24Stereo,
} from "./audio_export.js";

let fails = 0;
const check = (what, ok) => { console.log(`audio export: ${what.padEnd(59)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const text = (u8, a, n) => String.fromCharCode(...u8.slice(a, a + n));
const int24 = (u8, p) => {
  let x = u8[p] | u8[p + 1] << 8 | u8[p + 2] << 16;
  if (x & 0x800000) x |= ~0xFFFFFF;
  return x;
};

const pcm = new Uint8Array(3 * WAV_FRAME_BYTES);
const stat = writePcm24Stereo(pcm, 0, new Float32Array([-1, 0, 0.5]), new Float32Array([1, -0.5, 2]));
check("stereo samples are signed 24-bit little-endian",
  int24(pcm, 0) === -8388608 && int24(pcm, 3) === 8388607 && int24(pcm, 6) === 0 &&
  int24(pcm, 9) === -4194304 && int24(pcm, 12) === 4194304 && int24(pcm, 15) === 8388607);
check("conversion reports the peak and clipping", stat.frames === 3 && stat.peak === 2 && stat.clipped === 2);

const rec = new Pcm24Recorder(2);
rec.start(5);
const a = rec.write(new Float32Array([0.1, 0.2, 0.3]), new Float32Array([-0.1, -0.2, -0.3]));
const b = rec.write(new Float32Array([0.4, 0.5, 0.6]), new Float32Array([-0.4, -0.5, -0.6]));
check("the recorder batches chunks without exceeding its bound",
  a.chunks.length === 1 && a.chunks[0].frames === 2 && !a.stopped && b.stopped &&
  b.frames === 5 && b.chunks.length === 2 && b.chunks[0].frames === 2 && b.chunks[1].frames === 1 &&
  [...a.chunks, ...b.chunks].reduce((n, x) => n + x.bytes.length, 0) === 5 * WAV_FRAME_BYTES);

const manual = new Pcm24Recorder(4);
manual.start(10);
manual.write(new Float32Array([0.1, 0.2, 0.3]));
const stopped = manual.stop();
check("manual stop flushes a partial chunk exactly once",
  stopped.frames === 3 && stopped.chunk?.frames === 3 &&
  stopped.chunk.bytes.length === 3 * WAV_FRAME_BYTES && manual.stop().chunk === null);

const h = wavHeader(44100, 5), v = new DataView(h.buffer);
check("the WAV header describes 24-bit stereo PCM",
  text(h, 0, 4) === "RIFF" && text(h, 8, 4) === "WAVE" && text(h, 12, 4) === "fmt " &&
  v.getUint16(20, true) === 1 && v.getUint16(22, true) === 2 && v.getUint32(24, true) === 44100 &&
  v.getUint32(28, true) === 44100 * 6 && v.getUint16(32, true) === 6 && v.getUint16(34, true) === 24);
check("the RIFF and data sizes match the captured frames",
  text(h, 36, 4) === "data" && v.getUint32(40, true) === 5 * 6 && v.getUint32(4, true) === 36 + 5 * 6);
check("recording time is stable at minute boundaries",
  recordingTime(0, 44100) === "00:00" && recordingTime(59 * 44100, 44100) === "00:59" &&
  recordingTime(60 * 44100, 44100) === "01:00");

let refused = 0;
for (const [rate, frames] of [[0, 1], [44100, 0], [44100, Math.ceil(0xFFFFFFFF / WAV_FRAME_BYTES)]]) {
  try { wavHeader(rate, frames); } catch { refused++; }
}
check("invalid and RIFF-overflow dimensions are refused", refused === 3);

let Worklet;
const posted = [];
globalThis.sampleRate = 48000;
globalThis.currentTime = 0;
globalThis.AudioWorkletProcessor = class {
  constructor() { this.port = { postMessage: (message, transfer = []) => posted.push({ message, transfer }) }; }
};
globalThis.registerProcessor = (name, processor) => { if (name === "felucca") Worklet = processor; };
await import(`./worklet.js?audio-export-test=${Date.now()}`);
const memory = { buffer: new ArrayBuffer(4096) };
const processor = new Worklet();
processor.ex = {
  memory,
  web_sample_rate: () => 44100,
  web_render: (frames) => {
    new Float32Array(memory.buffer, 0, frames).fill(0.25);
    new Float32Array(memory.buffer, 1024, frames).fill(-0.5);
  },
  web_out_l: () => 0,
  web_out_r: () => 1024,
};
await processor.onMessage({ type: "record-start" });
const output = [[new Float32Array(128), new Float32Array(128)]];
processor.process([], output);
await processor.onMessage({ type: "record-stop" });
const protocol = posted.map((x) => x.message.type);
const captured = posted.find((x) => x.message.type === "record-chunk");
check("the worklet sends ordered start, PCM chunk and stop messages",
  protocol.join(",") === "record-started,record-chunk,record-stopped" &&
  posted[0].message.rate === 48000 && captured.message.frames === 128 &&
  captured.message.bytes.length === 128 * WAV_FRAME_BYTES && captured.transfer[0] === captured.message.bytes.buffer &&
  output[0][0][0] === 0.25 && output[0][1][0] === -0.5);

console.log(fails ? `audio export: ${fails} FAILED` : "audio export: all ok");
process.exit(fails ? 1 : 0);
