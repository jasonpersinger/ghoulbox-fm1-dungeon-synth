// SPDX-License-Identifier: GPL-3.0-only
// Dedicated FLAC / MP3 encoder worker. Third-party codecs are self-hosted under vendor/.

import { patchFlacStreamInfo, pcm24ToInt32, pcm24ToPlanarFloat } from "./audio_encode.js";

let format = null, encoder = null, metadata = null, failed = false;
let headerParts = [], headerBytes = 0;

function sendBytes(bytes) {
  if (!bytes?.length) return;
  const copy = bytes.slice();
  postMessage({ type: "chunk", bytes: copy }, [copy.buffer]);
}

function flacOutput(bytes) {
  let data = bytes.slice(), first = 0;
  if (headerBytes < 42) {
    const count = Math.min(42 - headerBytes, data.length);
    headerParts.push(data.slice(0, count));
    headerBytes += count;
    first = count;
  }
  if (first < data.length) sendBytes(data.subarray(first));
}

function flacHeader() {
  if (headerBytes !== 42) throw new Error("FLAC encoder did not produce a complete header");
  const header = new Uint8Array(42);
  let at = 0;
  for (const part of headerParts) { header.set(part, at); at += part.length; }
  return patchFlacStreamInfo(header, metadata);
}

function waitForFlac() {
  if (self.Flac?.isReady()) return Promise.resolve();
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error("FLAC encoder did not initialize")), 15000);
    self.Flac.onready = () => { clearTimeout(timer); resolve(); };
  });
}

async function init(message) {
  format = message.format;
  if (format === "flac") {
    self.FLAC_SCRIPT_LOCATION = new URL("./vendor/libflac/", import.meta.url).href;
    await import("./vendor/libflac/libflac.min.wasm.js");
    await waitForFlac();
    encoder = self.Flac.create_libflac_encoder(message.sampleRate, 2, 24, 5, 0, false);
    if (!encoder) throw new Error("Could not create the FLAC encoder");
    const status = self.Flac.init_encoder_stream(encoder, flacOutput, (value) => { metadata = value; });
    if (status !== 0) throw new Error(`Could not initialize the FLAC encoder (${status})`);
  } else if (format === "mp3") {
    await import("./vendor/mp3/WasmMediaEncoder.min.js");
    const response = await fetch(new URL("./vendor/mp3/mp3.wasm", import.meta.url));
    if (!response.ok) throw new Error(`Could not load the MP3 encoder (${response.status})`);
    encoder = await self.WasmMediaEncoder.createEncoder("audio/mpeg", await response.arrayBuffer());
    encoder.configure({ sampleRate: message.sampleRate, channels: 2, vbrQuality: 2 });
  } else {
    throw new Error("Unknown audio format");
  }
  postMessage({ type: "ready" });
}

function encode(message) {
  if (!(message.bytes instanceof Uint8Array) || !Number.isInteger(message.frames) ||
      message.frames <= 0 || message.bytes.length !== message.frames * 6)
    throw new Error("Invalid PCM audio chunk");
  if (format === "flac") {
    if (!self.Flac.FLAC__stream_encoder_process_interleaved(encoder, pcm24ToInt32(message.bytes), message.frames))
      throw new Error("FLAC encoding failed");
  } else {
    sendBytes(encoder.encode(pcm24ToPlanarFloat(message.bytes)));
  }
}

function finish() {
  if (format === "flac") {
    if (!self.Flac.FLAC__stream_encoder_finish(encoder)) throw new Error("Could not finish the FLAC file");
    self.Flac.FLAC__stream_encoder_delete(encoder);
    encoder = null;
    const header = flacHeader();
    postMessage({ type: "done", header }, [header.buffer]);
  } else {
    sendBytes(encoder.finalize());
    encoder = null;
    postMessage({ type: "done" });
  }
  close();
}

function report(error) {
  if (failed) return;
  failed = true;
  if (format === "flac" && encoder) self.Flac.FLAC__stream_encoder_delete(encoder);
  postMessage({ type: "error", message: error?.message || String(error) });
  close();
}

self.onmessage = (event) => {
  if (failed) return;
  try {
    if (event.data?.type === "init") void init(event.data).catch(report);
    else if (event.data?.type === "encode" && encoder) encode(event.data);
    else if (event.data?.type === "finish" && encoder) finish();
    else throw new Error("Audio encoder received an invalid command");
  } catch (error) { report(error); }
};
