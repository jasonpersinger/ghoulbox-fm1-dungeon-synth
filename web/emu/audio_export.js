// SPDX-License-Identifier: GPL-3.0-only
// Browser-emulator audio recording: bounded stereo PCM capture and a standard 24-bit WAV header.
// This stays free of DOM and AudioWorklet APIs so its byte-level invariants can be tested in Node.

export const WAV_CHANNELS = 2;
export const WAV_BITS = 24;
export const WAV_FRAME_BYTES = WAV_CHANNELS * WAV_BITS / 8;
export const RECORD_CHUNK_FRAMES = 16384;
export const RECORD_MAX_SECONDS = 10 * 60;

function pcm24(x) {
  x = Number.isFinite(x) ? x : 0;
  return Math.max(-8388608, Math.min(8388607, Math.round(x * 8388608)));
}

function put24(out, p, value) {
  out[p] = value & 255;
  out[p + 1] = value >> 8 & 255;
  out[p + 2] = value >> 16 & 255;
}

export function writePcm24Stereo(out, frame, left, right, first = 0, count = left.length - first) {
  if (!(out instanceof Uint8Array) || !left || !right) throw new TypeError("Invalid PCM buffer");
  count = Math.max(0, Math.min(count | 0, left.length - first, right.length - first,
                              Math.floor(out.length / WAV_FRAME_BYTES) - frame));
  let peak = 0, clipped = 0, p = frame * WAV_FRAME_BYTES;
  for (let i = 0; i < count; i++, p += WAV_FRAME_BYTES) {
    const l = Number.isFinite(left[first + i]) ? left[first + i] : 0;
    const r = Number.isFinite(right[first + i]) ? right[first + i] : 0;
    peak = Math.max(peak, Math.abs(l), Math.abs(r));
    if (l >= 1 || l < -1) clipped++;
    if (r >= 1 || r < -1) clipped++;
    put24(out, p, pcm24(l));
    put24(out, p + 3, pcm24(r));
  }
  return { frames: count, peak, clipped };
}

export class Pcm24Recorder {
  constructor(chunkFrames = RECORD_CHUNK_FRAMES) {
    if (!Number.isInteger(chunkFrames) || chunkFrames <= 0) throw new RangeError("Invalid recording chunk size");
    this.chunkFrames = chunkFrames;
    this.active = false;
    this.buffer = null;
    this.used = this.frames = this.maxFrames = this.clipped = 0;
    this.peak = 0;
  }

  start(maxFrames) {
    if (!Number.isInteger(maxFrames) || maxFrames <= 0) throw new RangeError("Invalid recording duration");
    this.active = true;
    this.buffer = new Uint8Array(this.chunkFrames * WAV_FRAME_BYTES);
    this.used = this.frames = this.clipped = 0;
    this.maxFrames = maxFrames;
    this.peak = 0;
  }

  take() {
    if (!this.used) return null;
    const frames = this.used, n = frames * WAV_FRAME_BYTES;
    const bytes = n === this.buffer.length ? this.buffer : this.buffer.slice(0, n);
    this.buffer = new Uint8Array(this.chunkFrames * WAV_FRAME_BYTES);
    this.used = 0;
    return { bytes, frames };
  }

  write(left, right = left) {
    const chunks = [];
    if (!this.active || !left || !right) return { chunks, stopped: !this.active };
    let first = 0;
    const available = Math.min(left.length, right.length, this.maxFrames - this.frames);
    while (first < available) {
      const n = Math.min(available - first, this.chunkFrames - this.used);
      const stat = writePcm24Stereo(this.buffer, this.used, left, right, first, n);
      this.used += stat.frames;
      this.frames += stat.frames;
      this.peak = Math.max(this.peak, stat.peak);
      this.clipped += stat.clipped;
      first += stat.frames;
      if (this.used === this.chunkFrames) chunks.push(this.take());
    }
    if (this.frames >= this.maxFrames) {
      this.active = false;
      const tail = this.take();
      if (tail) chunks.push(tail);
    }
    return { chunks, stopped: !this.active, frames: this.frames, peak: this.peak, clipped: this.clipped };
  }

  stop() {
    if (!this.active) return { chunk: null, frames: this.frames, peak: this.peak, clipped: this.clipped };
    this.active = false;
    return { chunk: this.take(), frames: this.frames, peak: this.peak, clipped: this.clipped };
  }
}

function ascii(view, at, s) {
  for (let i = 0; i < s.length; i++) view.setUint8(at + i, s.charCodeAt(i));
}

export function wavHeader(sampleRate, frames) {
  if (!Number.isInteger(sampleRate) || sampleRate < 8000 || sampleRate > 384000 ||
      !Number.isInteger(frames) || frames <= 0)
    throw new RangeError("Invalid WAV dimensions");
  const dataBytes = frames * WAV_FRAME_BYTES;
  if (dataBytes > 0xFFFFFFFF - 36) throw new RangeError("Recording is too long for a WAV file");
  const bytes = new Uint8Array(44), v = new DataView(bytes.buffer);
  ascii(v, 0, "RIFF"); v.setUint32(4, 36 + dataBytes, true); ascii(v, 8, "WAVE");
  ascii(v, 12, "fmt "); v.setUint32(16, 16, true); v.setUint16(20, 1, true);
  v.setUint16(22, WAV_CHANNELS, true); v.setUint32(24, sampleRate, true);
  v.setUint32(28, sampleRate * WAV_FRAME_BYTES, true); v.setUint16(32, WAV_FRAME_BYTES, true);
  v.setUint16(34, WAV_BITS, true); ascii(v, 36, "data"); v.setUint32(40, dataBytes, true);
  return bytes;
}

export function recordingTime(frames, sampleRate) {
  const seconds = sampleRate > 0 ? Math.floor(frames / sampleRate) : 0;
  const m = Math.floor(seconds / 60), s = seconds % 60;
  return `${String(m).padStart(2, "0")}:${String(s).padStart(2, "0")}`;
}
