// SPDX-License-Identifier: GPL-3.0-only
// Web Audio startup helpers kept separate so the user-gesture timing can be tested without a browser DOM.

export function withTimeout(task, timeoutMs, message) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(message)), timeoutMs);
    Promise.resolve(task).then(
      (value) => { clearTimeout(timer); resolve(value); },
      (error) => { clearTimeout(timer); reject(error); },
    );
  });
}

export function resumeAudio(context, timeoutMs = 3000) {
  let resumed;
  try {
    // Both calls happen before this function returns. In particular, do not move them beyond an await:
    // iOS Safari may only release Web Audio during the button's transient user activation.
    resumed = context.resume();
    const source = context.createBufferSource();
    source.buffer = context.createBuffer(1, 1, context.sampleRate);
    source.connect(context.destination);
    source.onended = () => source.disconnect();
    source.start(0);
  } catch (error) {
    return Promise.reject(error);
  }
  return withTimeout(resumed, timeoutMs, "The browser did not release audio").then(() => {
    if (context.state !== "running") throw new Error(`Audio remained ${context.state || "unavailable"}`);
  });
}
