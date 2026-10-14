// SPDX-License-Identifier: GPL-3.0-only
import { createHash } from "node:crypto";
import { createRequire } from "node:module";
import { copyFileSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import {
  StreamingAudioEncoder, patchFlacStreamInfo, pcm24ToInt32, pcm24ToPlanarFloat,
} from "./audio_encode.js";
import { WAV_FRAME_BYTES, writePcm24Stereo } from "./audio_export.js";

let fails = 0;
const check = (what, ok) => { console.log(`audio codecs: ${what.padEnd(58)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const here = dirname(fileURLToPath(import.meta.url));

const pcm = new Uint8Array(3 * WAV_FRAME_BYTES);
writePcm24Stereo(pcm, 0, new Float32Array([-1, 0, 0.5]), new Float32Array([1, -0.5, 0.25]));
const ints = pcm24ToInt32(pcm), floats = pcm24ToPlanarFloat(pcm);
check("24-bit PCM becomes exact interleaved FLAC integers",
  ints.length === 6 && ints[0] === -8388608 && ints[1] === 8388607 && ints[4] === 4194304);
check("24-bit PCM becomes normalized planar MP3 input",
  floats[0].length === 3 && floats[1].length === 3 && floats[0][0] === -1 &&
  Math.abs(floats[1][0] - (8388607 / 8388608)) < 1e-7 && floats[1][1] === -0.5);

const rawHeader = new Uint8Array(42);
rawHeader.set([102, 76, 97, 67, 0, 0, 0, 34]); // fLaC + STREAMINFO(34 bytes)
rawHeader[21] = 0xA0;
const patched = patchFlacStreamInfo(rawHeader, {
  min_framesize: 0x010203, max_framesize: 0x040506, total_samples: 0x123456789,
  md5sum: "00112233445566778899aabbccddeeff",
});
check("FLAC STREAMINFO patches frame sizes and all 36 sample bits",
  patched[12] === 1 && patched[13] === 2 && patched[14] === 3 && patched[15] === 4 &&
  patched[21] === 0xA1 && patched[22] === 0x23 && patched[23] === 0x45 && patched[24] === 0x67 && patched[25] === 0x89);
check("FLAC STREAMINFO patches the final MD5 without mutating input",
  patched[26] === 0 && patched[27] === 0x11 && patched[41] === 0xff && rawHeader[21] === 0xA0);

class FakeWorker {
  postMessage(message) {
    if (message.type === "init") queueMicrotask(() => this.onmessage({ data: { type: "ready" } }));
    if (message.type === "encode") this.encoded = message;
    if (message.type === "finish") queueMicrotask(() => {
      this.onmessage({ data: { type: "chunk", bytes: new Uint8Array([2, 3]) } });
      this.onmessage({ data: { type: "done", header: new Uint8Array([1]) } });
    });
  }
  terminate() { this.terminated = true; }
}
let fake;
const session = await StreamingAudioEncoder.open("flac", 44100, () => (fake = new FakeWorker()));
session.encode(new Uint8Array(6), 1);
const fakeBlob = await session.finish();
check("the worker client streams PCM and assembles header before chunks",
  fake.encoded.frames === 1 && fakeBlob.type === "audio/flac" &&
  [...new Uint8Array(await fakeBlob.arrayBuffer())].join() === "1,2,3" && fake.terminated);

let failedWorker, reported;
const failedSession = await StreamingAudioEncoder.open("mp3", 48000, () => (failedWorker = new FakeWorker()));
failedSession.onfailure = (error) => { reported = error.message; };
failedWorker.onmessage({ data: { type: "error", message: "codec stopped" } });
let finishRefused = false;
try { await failedSession.finish(); } catch (error) { finishRefused = error.message === "codec stopped"; }
check("a worker failure is reported and prevents a corrupt download",
  reported === "codec stopped" && finishRefused && failedWorker.terminated);

const expectedHashes = new Map([
  ["vendor/libflac/libflac.min.wasm.js", "76892e974c95adc489910bc991e1d234bb48fb9f1bcc3f76ab471a1493c30f7e"],
  ["vendor/libflac/libflac.min.wasm.wasm", "dd145880539e836501e3c779aae19e0d3915a90fbf83b50c8a5cde819c6450b0"],
  ["vendor/mp3/WasmMediaEncoder.min.js", "dd4e17abf5377dfecc726d6ec5e7b72dab01cf3522974278e5347f4e68480fb3"],
  ["vendor/mp3/mp3.wasm", "85e81719250b9a667b1258143f689dda70e3e57a7e7c29ab0b4cef65c8f6eb9a"],
]);
check("vendored codec artifacts match the reviewed releases", [...expectedHashes].every(([file, wanted]) =>
  createHash("sha256").update(readFileSync(join(here, file))).digest("hex") === wanted));

// Load the browser UMD builds as CommonJS from a temporary directory so their actual WASM encoders are exercised in Node.
const scratch = mkdtempSync(join(tmpdir(), "ghoulbox-codecs-"));
try {
  const require = createRequire(import.meta.url);
  const flacJs = join(here, "vendor/libflac/libflac.min.wasm.js");
  const flacWasm = join(here, "vendor/libflac/libflac.min.wasm.wasm");
  const flacCjs = join(scratch, "libflac.cjs");
  copyFileSync(flacJs, flacCjs);
  copyFileSync(flacWasm, join(scratch, basename(flacWasm)));
  process.env.FLAC_SCRIPT_LOCATION = scratch + "/";
  const nativeFetch = globalThis.fetch;
  globalThis.fetch = undefined; // this older Emscripten build otherwise gives Node fetch a filesystem path
  const Flac = require(flacCjs);
  try {
    await new Promise((resolve, reject) => {
      if (Flac.isReady()) return resolve();
      const timer = setTimeout(() => reject(new Error("FLAC test initialization timed out")), 10000);
      Flac.onready = () => { clearTimeout(timer); resolve(); };
    });
  } finally { globalThis.fetch = nativeFetch; }
  const flacParts = [];
  let flacMetadata;
  const flac = Flac.create_libflac_encoder(44100, 2, 24, 5, 0, false);
  const initStatus = Flac.init_encoder_stream(flac, (bytes) => flacParts.push(bytes.slice()), (value) => { flacMetadata = value; });
  const encoded = initStatus === 0 && Flac.FLAC__stream_encoder_process_interleaved(flac, ints, 3);
  const finished = encoded && Flac.FLAC__stream_encoder_finish(flac);
  Flac.FLAC__stream_encoder_delete(flac);
  const flacLength = flacParts.reduce((n, part) => n + part.length, 0), flacFile = new Uint8Array(flacLength);
  let at = 0;
  for (const part of flacParts) { flacFile.set(part, at); at += part.length; }
  flacFile.set(patchFlacStreamInfo(flacFile.subarray(0, 42), flacMetadata), 0);
  check("the vendored FLAC WASM encodes and finalizes real PCM",
    finished && flacLength > 42 && String.fromCharCode(...flacFile.subarray(0, 4)) === "fLaC" && flacMetadata.total_samples === 3);

  const mp3Js = join(here, "vendor/mp3/WasmMediaEncoder.min.js"), mp3Cjs = join(scratch, "mp3.cjs");
  copyFileSync(mp3Js, mp3Cjs);
  const Mp3 = require(mp3Cjs), mp3Wasm = readFileSync(join(here, "vendor/mp3/mp3.wasm"));
  const mp3 = await Mp3.createEncoder("audio/mpeg", mp3Wasm);
  mp3.configure({ sampleRate: 44100, channels: 2, vbrQuality: 2 });
  const mp3Parts = [mp3.encode(floats).slice(), mp3.finalize().slice()];
  const mp3Bytes = new Uint8Array(mp3Parts.reduce((n, part) => n + part.length, 0));
  at = 0; for (const part of mp3Parts) { mp3Bytes.set(part, at); at += part.length; }
  const hasId3 = String.fromCharCode(...mp3Bytes.subarray(0, 3)) === "ID3";
  const hasFrame = mp3Bytes.some((value, i) => i + 1 < mp3Bytes.length && value === 0xFF && (mp3Bytes[i + 1] & 0xE0) === 0xE0);
  check("the vendored LAME WASM encodes and finalizes real PCM", mp3Bytes.length > 0 && (hasId3 || hasFrame));
} finally {
  delete process.env.FLAC_SCRIPT_LOCATION;
  rmSync(scratch, { recursive: true, force: true });
}

console.log(fails ? `audio codecs: ${fails} FAILED` : "audio codecs: all ok");
process.exit(fails ? 1 : 0);
