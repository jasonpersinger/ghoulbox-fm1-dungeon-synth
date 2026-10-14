// SPDX-License-Identifier: GPL-3.0-only
// Streaming compressed-audio export. Encoding runs in a worker; the caller keeps PCM Blobs for WAV fallback.

export const AUDIO_FORMATS = Object.freeze({
  flac: Object.freeze({ label: "FLAC", extension: "flac", type: "audio/flac" }),
  mp3: Object.freeze({ label: "MP3", extension: "mp3", type: "audio/mpeg" }),
  wav: Object.freeze({ label: "WAV", extension: "wav", type: "audio/wav" }),
});

function pcm24(bytes, at) {
  let value = bytes[at] | bytes[at + 1] << 8 | bytes[at + 2] << 16;
  if (value & 0x800000) value |= ~0xFFFFFF;
  return value;
}

export function pcm24ToInt32(bytes) {
  if (!(bytes instanceof Uint8Array) || bytes.length % 3) throw new TypeError("Invalid 24-bit PCM");
  const samples = new Int32Array(bytes.length / 3);
  for (let i = 0, p = 0; i < samples.length; i++, p += 3) samples[i] = pcm24(bytes, p);
  return samples;
}

export function pcm24ToPlanarFloat(bytes) {
  if (!(bytes instanceof Uint8Array) || bytes.length % 6) throw new TypeError("Invalid stereo 24-bit PCM");
  const frames = bytes.length / 6, left = new Float32Array(frames), right = new Float32Array(frames);
  for (let i = 0, p = 0; i < frames; i++, p += 6) {
    left[i] = pcm24(bytes, p) / 8388608;
    right[i] = pcm24(bytes, p + 3) / 8388608;
  }
  return [left, right];
}

function put24be(bytes, at, value) {
  bytes[at] = value / 65536 & 255;
  bytes[at + 1] = value / 256 & 255;
  bytes[at + 2] = value & 255;
}

export function patchFlacStreamInfo(header, metadata) {
  if (!(header instanceof Uint8Array) || header.length < 42 ||
      String.fromCharCode(...header.subarray(0, 4)) !== "fLaC" || (header[4] & 0x7F) !== 0)
    throw new Error("The FLAC encoder returned an invalid STREAMINFO header");
  const out = header.slice(), total = Number(metadata?.total_samples);
  if (!Number.isSafeInteger(total) || total < 0 || total >= 2 ** 36)
    throw new Error("The FLAC encoder returned an invalid sample count");
  put24be(out, 12, Number(metadata.min_framesize) || 0);
  put24be(out, 15, Number(metadata.max_framesize) || 0);
  out[21] = out[21] & 0xF0 | Math.floor(total / 2 ** 32) & 0x0F;
  out[22] = Math.floor(total / 2 ** 24) & 255;
  out[23] = Math.floor(total / 2 ** 16) & 255;
  out[24] = Math.floor(total / 2 ** 8) & 255;
  out[25] = total & 255;
  if (!/^[0-9a-f]{32}$/i.test(metadata.md5sum || "")) throw new Error("The FLAC encoder returned an invalid checksum");
  for (let i = 0; i < 16; i++) out[26 + i] = parseInt(metadata.md5sum.slice(i * 2, i * 2 + 2), 16);
  return out;
}

export class StreamingAudioEncoder {
  static async open(format, sampleRate, workerFactory) {
    if (!AUDIO_FORMATS[format] || format === "wav") throw new RangeError("Invalid compressed audio format");
    if (!Number.isInteger(sampleRate) || sampleRate < 8000 || sampleRate > 192000) throw new RangeError("Invalid sample rate");
    const session = new StreamingAudioEncoder(format, sampleRate, workerFactory);
    await session.ready;
    return session;
  }

  constructor(format, sampleRate, workerFactory) {
    const makeWorker = workerFactory || (() => new Worker(new URL("./audio_encode_worker.js", import.meta.url), { type: "module" }));
    this.format = format;
    this.info = AUDIO_FORMATS[format];
    this.parts = [];
    this.bytes = 0;
    this.failure = null;
    this.finishing = false;
    this.worker = makeWorker();
    this.ready = new Promise((resolve, reject) => { this.readyResolve = resolve; this.readyReject = reject; });
    this.worker.onmessage = (event) => this.message(event.data);
    this.worker.onerror = (event) => this.fail(new Error(event.message || "Audio encoder worker failed"));
    this.readyTimer = setTimeout(() => this.fail(new Error("Audio encoder initialization timed out")), 20000);
    this.worker.postMessage({ type: "init", format, sampleRate });
  }

  message(message) {
    if (!message || typeof message.type !== "string") return;
    if (message.type === "ready") {
      clearTimeout(this.readyTimer);
      this.readyResolve();
    } else if (message.type === "chunk") {
      if (!(message.bytes instanceof Uint8Array)) return this.fail(new Error("Audio encoder returned an invalid chunk"));
      this.parts.push(new Blob([message.bytes]));
      this.bytes += message.bytes.length;
    } else if (message.type === "done") {
      if (!this.finishResolve) return this.fail(new Error("Audio encoder stopped unexpectedly"));
      const prefix = message.header instanceof Uint8Array ? [new Blob([message.header])] : [];
      this.bytes += message.header?.length || 0;
      clearTimeout(this.finishTimer);
      this.finishResolve(new Blob([...prefix, ...this.parts], { type: this.info.type }));
      this.close();
    } else if (message.type === "error") {
      this.fail(new Error(message.message || "Audio encoding failed"));
    }
  }

  encode(bytes, frames) {
    if (this.failure) throw this.failure;
    if (this.finishing) throw new Error("Audio encoder is already finishing");
    this.worker.postMessage({ type: "encode", bytes, frames }, [bytes.buffer]);
  }

  finish() {
    if (this.failure) return Promise.reject(this.failure);
    if (this.finishing) return this.finished;
    this.finishing = true;
    this.finished = new Promise((resolve, reject) => { this.finishResolve = resolve; this.finishReject = reject; });
    this.finishTimer = setTimeout(() => this.fail(new Error("Audio encoder finalization timed out")), 30000);
    this.worker.postMessage({ type: "finish" });
    return this.finished;
  }

  fail(error) {
    if (this.failure) return;
    this.failure = error;
    clearTimeout(this.readyTimer);
    clearTimeout(this.finishTimer);
    this.readyReject(error);
    if (this.finishReject) this.finishReject(error);
    if (this.onfailure) this.onfailure(error);
    this.close();
  }

  close() {
    if (!this.worker) return;
    this.worker.terminate();
    this.worker = null;
  }
}
