// SPDX-License-Identifier: GPL-3.0-only
import fs from "fs";
import {
  PANEL_BUTTONS, PANEL_KNOBS, fixedVelocityMessage, makeEmulatorState, midiKeyIndex,
  panelCcAction, panelNoteAction, readEmulatorState, stateCrc,
} from "./emu_io.js";

let fails = 0;
const check = (what, ok) => { console.log(`emu io: ${what.padEnd(66)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };

const all = [...PANEL_KNOBS, ...PANEL_BUTTONS].map((x) => x.cc);
check("all 8 knobs and 14 buttons have unique CCs", PANEL_KNOBS.length === 8 && PANEL_BUTTONS.length === 14 && new Set(all).size === 22);
check("the CC list names every physical control in panel order",
  PANEL_KNOBS.map((x) => x.name).join() === "MASTER,SELECT,PRESETS,ALGORITHM,KNOB 1,KNOB 2,KNOB 3,KNOB 4" &&
  PANEL_BUTTONS.map((x) => x.name).join() === "FX,SCL,ENV,LFO,EDIT,GLO,HOME,SAVE,ARP,SEQ,PLAY,REC,OCT-,OCT+" &&
  PANEL_BUTTONS.every((x, i) => x.bit === i));
check("the map avoids GHOULBOX's direct parameter CCs and channel-mode CCs",
  all.every((x) => ![5, 7, 10, 21, 22, 23, 24, 71, 72, 73, 74, 75, 76, 77, 78, 79, 85, 86, 91, 92, 93, 94,
    102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 120, 121, 122, 123, 124, 125, 126, 127].includes(x)));

const last = new Map();
check("MASTER is absolute from 0 to 1023", panelCcAction(112, 127, last).value === 1023 && panelCcAction(112, 0, last).value === 0);
check("an absolute encoder seeds, then moves by a bounded delta",
  panelCcAction(113, 20, last).type === "seed" && panelCcAction(113, 24, last).n === 4 && panelCcAction(113, 100, last).n === 8);
check("a relative encoder accepts 1 / 127", panelCcAction(114, 1, last, "relative").n === 1 && panelCcAction(114, 127, last, "relative").n === -1);
check("button CCs use 64 as the press threshold", !panelCcAction(25, 63, last).down && panelCcAction(25, 64, last).down);
check("unmapped CCs pass through", panelCcAction(7, 100, last) === null);
check("MIDI F3..G5 map to the 27 panel keys", midiKeyIndex(52) < 0 && midiKeyIndex(53) === 0 && midiKeyIndex(79) === 26 && midiKeyIndex(80) < 0);
const diverted = new Set(), panelDown = panelNoteAction([0x92, 60, 90], true, diverted);
const panelUp = panelNoteAction([0x82, 60, 0], false, diverted);
check("a layer diverts Note On and owns its Note Off after the layer is released",
  panelDown.down && panelDown.key === 7 && panelUp && !panelUp.down && panelUp.id === panelDown.id && !diverted.size);
check("an ordinary note is not diverted", panelNoteAction([0x90, 60, 90], false, diverted) === null);
check("fixed velocity changes Note On only",
  fixedVelocityMessage([0x91, 60, 12], 100).join() === "145,60,100" &&
  fixedVelocityMessage([0x81, 60, 12], 100).join() === "129,60,12" &&
  fixedVelocityMessage([0x91, 60, 0], 100).join() === "145,60,0");

const sector = new Uint8Array(4096); sector[0] = 42; sector[4095] = 19;
const runtime = new Uint8Array(3584); runtime[20] = 88;
const file = makeEmulatorState({ [0x97000]: sector }, runtime, "test");
const read = readEmulatorState(JSON.stringify(file));
check("state file round-trips runtime and stored sectors",
  read.runtime[20] === 88 && read.sectors[0x97000][0] === 42 && read.sectors[0x97000][4095] === 19);
const bad = structuredClone(file); bad.sectors[0].crc = stateCrc(sector) ^ 1;
let refused = false; try { readEmulatorState(bad); } catch { refused = true; }
check("a corrupt sector is rejected before import", refused);
const duplicate = structuredClone(file); duplicate.sectors.push(structuredClone(duplicate.sectors[0]));
refused = false; try { readEmulatorState(duplicate); } catch { refused = true; }
check("duplicate flash offsets are rejected", refused);
const outside = structuredClone(file); outside.sectors[0].offset = 0;
refused = false; try { readEmulatorState(outside); } catch { refused = true; }
check("flash offsets outside the emulator save areas are rejected", refused);

const page = fs.readFileSync(new URL("./index.html", import.meta.url), "utf8");
const manual = fs.readFileSync(new URL("./tools.html", import.meta.url), "utf8");
check("the tools box ends with a manual link that preserves the running synth",
  /<p class="tools-manual-link"><a href="tools\.html" target="_blank" rel="noopener">/.test(page));
check("audio export defaults to FLAC with MP3 and WAV available",
  /<select id="audio-format"><option value="flac">/.test(page) &&
  page.includes('<option value="mp3">') && page.includes('<option value="wav">') &&
  page.includes('from "./audio_encode.js"'));
check("switch-on and recovery request an audible iOS session",
  page.includes("import { preparePlaybackSession, resumeAudio, withTimeout }") &&
  (page.match(/preparePlaybackSession\(\);/g) || []).length === 2);
check("the manual covers every tool and takes its CC tables from the shared map",
  ["Quick start", "MIDI", "Portable saves", "Record audio", "FLAC", "MP3", "WAV", "Focus Mode", "Troubleshooting"].every((x) => manual.includes(x)) &&
  manual.includes('import { PANEL_BUTTONS, PANEL_KNOBS } from "./emu_io.js"'));

console.log(fails ? `emu io: ${fails} FAILED` : "emu io: all ok");
process.exit(fails ? 1 : 0);
