// SPDX-License-Identifier: GPL-3.0-only
import { preparePlaybackSession, resumeAudio, withTimeout } from "./audio_session.js";

let fails = 0;
const check = (what, ok) => { console.log(`audio session: ${what.padEnd(59)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };

const mobile = { audioSession: { type: "ambient" } };
check("iOS is put on the audible playback session",
  preparePlaybackSession(mobile) && mobile.audioSession.type === "playback");
check("browsers without AudioSession remain supported", !preparePlaybackSession({}));
const guarded = { audioSession: { get type() { return "ambient"; }, set type(value) { throw new Error(value); } } };
check("a refused AudioSession change does not block startup", !preparePlaybackSession(guarded));

function context(resume) {
  const calls = [], source = {
    connect: () => calls.push("connect"),
    disconnect: () => calls.push("disconnect"),
    start: () => calls.push("start"),
    onended: null,
  };
  return {
    calls, source, state: "suspended", sampleRate: 44100, destination: {},
    resume() { calls.push("resume"); return resume(this); },
    createBufferSource() { calls.push("source"); return source; },
    createBuffer(channels, frames, rate) { calls.push(`buffer:${channels}:${frames}:${rate}`); return {}; },
  };
}

const ready = context((ctx) => Promise.resolve().then(() => { ctx.state = "running"; }));
const waking = resumeAudio(ready);
check("resume and silent source start synchronously in the gesture",
  ready.calls.join(",") === "resume,source,buffer:1:1:44100,connect,start");
await waking;
check("a running context completes the unlock", ready.state === "running");
ready.source.onended();
check("the one-frame unlock source disconnects after use", ready.calls.at(-1) === "disconnect");

const interrupted = context(() => Promise.resolve());
interrupted.state = "interrupted";
let rejectedState = false;
try { await resumeAudio(interrupted); } catch (error) { rejectedState = /interrupted/.test(error.message); }
check("a fulfilled resume must still reach running", rejectedState);

let timedOut = false;
try { await withTimeout(new Promise(() => {}), 5, "timed out"); } catch (error) { timedOut = error.message === "timed out"; }
check("a stalled browser promise is bounded", timedOut);

console.log(fails ? `audio session: ${fails} FAILED` : "audio session: all ok");
process.exit(fails ? 1 : 0);
