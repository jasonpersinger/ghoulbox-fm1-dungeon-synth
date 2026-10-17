/* SPDX-License-Identifier: GPL-3.0-only */
/* (GHOULBOX 2.1: the lessons as 2.1 works: PHRASES on SAVE, the MIXER on HOME, LFO DEST after LFO 2, the SYSTEM tab
 * the fifth, and RESTORE LAST, ON since 2.1, OFF for the lessons' fresh starts and ON again at the end) */
/* GHOULBOX lessons: a lesson played on the firmware's own input, audio AND screen code (tests/ui_render.c), as a
 * player would, one action at a time: buttons tapped and held and knobs turned go through ui_input() like the panel's.
 *   build/host/lesson_render N OUT.wav [FRAMES.rgb]   -> the mix (and the screen, 240x240 RGB24 at 30 fps);
 *   OUT.wav.acts: one line per action, "t0<TAB>t1<TAB>control<TAB>kind (tap, hold, turn+ clockwise, turn-, say)<TAB>step<TAB>step title<TAB>what to do",
 *   for tools/make_lesson_video.py (the sword points at the control). Not a test. */
#define UI_RENDER_NO_MAIN
#include "ui_render.c"

/* the controls, as the video names them: buttons (panel.c B_*), then the knobs (EN_*), then none */
enum { C_FX, C_SCL, C_ENV, C_LFO, C_EDIT, C_GLO, C_HOME, C_SAVE, C_ARP, C_SEQ, C_PLAY, C_REC, C_OCTDN, C_OCTUP,
       C_SELECT, C_ALGO, C_PRESETS, C_K1, C_K2, C_K3, C_K4, C_POWER, C_KEY, C_NONE };
static const char *const CNAME[] = {"FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY",
                                    "REC", "OCT-", "OCT+", "SELECT", "ALGORITHM", "PRESETS", "KNOB1", "KNOB2",
                                    "KNOB3", "KNOB4", "POWER", "KEY", "-"};
enum { K_TAP, K_HOLD, K_TURN, K_SAY, K_KEY, K_GLOKEY, K_CHORD, K_PRESS, K_LETGO, K_CRANK };   /* K_CHORD (lesson 7): three keys, arg a | b << 5 | c << 10;
   * (lesson 11) K_PRESS: the same three put down and kept down (the left hand), K_LETGO: they come up */   /* K_KEY: key arg held dur - 0.6 s; K_GLOKEY: GLO held,
                                                             * key arg tapped (a mute, track until-1 toggles) */
/* what an action turns or taps until: */
enum { U_CLICKS, U_TRACK, U_SOUND, U_PICK, U_PAGE, U_ROW, U_TAB, U_VALUE, U_SCENE, U_RESTORE,   /* U_RESTORE: arg 1 OFF */
       U_AFTERNOTE, U_EVADD, U_EVNOTE, U_EVCHANCE, U_ACT, U_STEPTIME };   /* U_STEPTIME: the cursor step's TIME */   /* (lesson 6) U_AFTERNOTE: the SEQ cursor on a rest just
                                                    * after a note; on AUTOMATION: + ADD's row, its step a note, CHANCE;
                                                    * U_ACT: the action of KNOB arg picked. A K_TURN with U_CLICKS turns arg
                                                    * clicks (signed) */
typedef struct {
    uint8_t ctl, kind, until; int16_t arg;            /* U_CLICKS: arg clicks (signed); U_VALUE: the knob's value */
    const char *name;                                  /* U_SOUND / U_PAGE: the name to reach */
    const char *say;                                   /* the caption */
    float dur;                                         /* seconds on screen (0: 3.5) */
} act_t;
typedef struct { const char *title; const act_t *acts; } lstep_t;
typedef struct { const lstep_t *steps; float tail; uint8_t scene; } lesson_t;   /* scene: the one the FM-1 has */

#define TAP(c, s) {c, K_TAP, U_CLICKS, 1, 0, s, 0}
#define TAPS(c, page, s) {c, K_TAP, U_PAGE, 0, page, s, 0}
#define HOLD(c, s) {c, K_HOLD, U_CLICKS, 0, 0, s, 0}
#define TURN_TO(c, u, a, nm, s) {c, K_TURN, u, a, nm, s, 0}
#define SAY(c, s, d) {c, K_SAY, U_CLICKS, 0, 0, s, d}
#define KEY(k, s, d) {C_KEY, K_KEY, U_CLICKS, k, 0, s, d}
#define MUTE(k, trk, s) {C_KEY, K_GLOKEY, (trk) + 1, k, 0, s, 0}
#define END {C_NONE, 0, 0, 0, 0, 0, 0}
#define TURN(c, n, s) {c, K_TURN, U_CLICKS, n, 0, s, 0}
#define CHORD_(a, b, c, s, d) {C_KEY, K_CHORD, U_CLICKS, (a) | (b) << 5 | (c) << 10, 0, s, d}
#define CHORD(...) CHORD_(__VA_ARGS__)                   /* (so a chord #define spreads into its three keys) */
#define PRESS_(a, b, c, s, d) {C_KEY, K_PRESS, U_CLICKS, (a) | (b) << 5 | (c) << 10, 0, s, d}
#define PRESS(...) PRESS_(__VA_ARGS__)
#define LETGO(s) {C_NONE, K_LETGO, U_CLICKS, 0, 0, s, 1.2f}
#define CRANK(rate, s, d) {C_SELECT, K_CRANK, U_CLICKS, rate, 0, s, d}   /* (lesson 12) SELECT turned rate clicks a second */
#define LATCH(k, trk, s) {C_KEY, K_GLOKEY, 0x10 | ((trk) + 1), k, 0, s, 0}   /* (lesson 11) GLO + a black key: the latch */

/* ---------------------------------------------------------------- LESSON 1: THE DRONE */
#define PEDAL 16                                       /* PATTERNS[16]: one low D, held (engines.c) */
static const act_t L1_START[] = {
    HOLD(C_HOME, "Hold HOME: the menu opens"),
    TURN_TO(C_ALGO, U_TAB, MTAB_SYSTEM, 0, "Turn ALGORITHM to the SYSTEM tab"),
    TURN_TO(C_PRESETS, U_ROW, MI_SCENE, 0, "PRESETS picks the row: SCENE is the first"),
    TURN_TO(C_OCTUP, U_SCENE, 4, 0, "Press OCT+ until it says BLANK"),
    TURN_TO(C_PRESETS, U_ROW, MI_RESTORE, 0, "Turn PRESETS down to RESTORE LAST"),
    TURN_TO(C_OCTDN, U_RESTORE, 1, 0, "Press OCT- until it says OFF: a fresh start"),
    TAP(C_HOME, "Tap HOME to close the menu"),
    SAY(C_POWER, "Switch the FM-1 off, then on again", 5), END};
static const act_t L1_NOTE[] = {
    TURN_TO(C_ALGO, U_TRACK, 3, 0, "Turn ALGORITHM until the red number is 4"),
    SAY(C_NONE, "Track 4 already has the sound DRONE WHEEL", 4),
    TAPS(C_SAVE, "PHRASES", "Tap SAVE until PHRASES"),
    TURN_TO(C_K1, U_PICK, PEDAL, 0, "Turn KNOB 1 to 17 PEDAL"),
    TAP(C_OCTUP, "Press OCT+ to load it"),
    TAP(C_PLAY, "Press PLAY"),
    SAY(C_NONE, "One low D that never stops: a drone", 6), END};
static const act_t L1_TUNE[] = {
    TAP(C_EDIT, "Tap EDIT"),
    SAY(C_K1, "KNOB 1 is DRN, the drone's note: D. Leave it", 4.5f),
    TURN_TO(C_K3, U_VALUE, 0, 0, "Turn KNOB 3 (5TH) all the way down"),
    SAY(C_NONE, "Listen: one note alone", 5),
    TURN_TO(C_K3, U_VALUE, 110, 0, "Turn KNOB 3 back up"),
    SAY(C_NONE, "An A joins below: an open fifth", 6), END};
static const act_t L1_BREATHE[] = {
    TURN_TO(C_K4, U_VALUE, 127, 0, "Turn KNOB 4 (WHL) up: lean on the wheel"),
    SAY(C_NONE, "Brighter and rougher", 4),
    TURN_TO(C_K4, U_VALUE, 45, 0, "Turn KNOB 4 back down"),
    TAP(C_EDIT, "Tap EDIT again"),
    TURN_TO(C_K4, U_VALUE, 0, 0, "Turn KNOB 4 (WOBL) down: dead still"),
    SAY(C_NONE, "Listen: no movement at all", 4.5f),
    TURN_TO(C_K4, U_VALUE, 90, 0, "Turn KNOB 4 up: the wheel wanders"),
    SAY(C_NONE, "Now it breathes", 5), END};
static const act_t L1_HALL[] = {
    TAP(C_FX, "Tap FX"),
    TURN_TO(C_K4, U_VALUE, 0, 0, "Turn KNOB 4 (REV) all the way down: dry"),
    SAY(C_NONE, "Listen: a small, close sound", 4.5f),
    TURN_TO(C_K4, U_VALUE, 120, 0, "Turn KNOB 4 up: into the reverb"),
    TAPS(C_FX, "REVERB", "Tap FX until REVERB"),
    TURN_TO(C_K2, U_VALUE, 20, 0, "Turn KNOB 2 (SIZE) down: a stone chamber"),
    SAY(C_NONE, "A small room", 4),
    TURN_TO(C_K2, U_VALUE, 127, 0, "Turn KNOB 2 all the way up: a cathedral"),
    SAY(C_NONE, "The drone fills the hall", 6), END};
static const act_t L1_VOICE[] = {
    TURN_TO(C_ALGO, U_TRACK, 2, 0, "Turn ALGORITHM to 3"),
    TURN_TO(C_PRESETS, U_SOUND, 0, "LOW DRONE", "Turn PRESETS to LOW DRONE"),
    TAPS(C_HOME, "MIXER", "Tap HOME until the MIXER"),
    TURN_TO(C_K1, U_VALUE, 0, 0, "Turn KNOB 1 (LEVEL) all the way down"),
    TAPS(C_SAVE, "PHRASES", "Tap SAVE until PHRASES"),
    TURN_TO(C_K1, U_PICK, PEDAL, 0, "KNOB 1 is already on 17 PEDAL: LOW DRONE picks it"),
    TAP(C_OCTUP, "Press OCT+ to load it"),
    TAPS(C_HOME, "MIXER", "Tap HOME until the MIXER again"),
    TURN_TO(C_K1, U_VALUE, 92, 0, "Turn KNOB 1 slowly up: a sung D below"),
    SAY(C_NONE, "Two drones", 5), END};
static const act_t L1_CAVE[] = {
    TURN_TO(C_ALGO, U_TRACK, 1, 0, "Turn ALGORITHM to 2"),
    TURN_TO(C_PRESETS, U_SOUND, 0, "CAVE WIND", "Turn PRESETS to CAVE WIND"),
    TAPS(C_HOME, "MIXER", "Tap HOME until the MIXER"),
    TURN_TO(C_K1, U_VALUE, 0, 0, "Turn KNOB 1 (LEVEL) all the way down"),
    TAPS(C_SAVE, "PHRASES", "Tap SAVE until PHRASES"),
    TURN_TO(C_K1, U_PICK, PEDAL, 0, "Turn KNOB 1 to 17 PEDAL"),
    TAP(C_OCTUP, "Press OCT+ to load it"),
    TAPS(C_HOME, "MIXER", "Tap HOME until the MIXER again"),
    TURN_TO(C_K1, U_VALUE, 70, 0, "Turn KNOB 1 up until you barely hear it"),
    SAY(C_NONE, "Two drones, a hall and the wind: dark ambient", 9),
    TAP(C_PLAY, "Press PLAY to stop"), END};
static const lstep_t L1_STEPS[] = {
    {"Start empty", L1_START}, {"One low note", L1_NOTE}, {"Tune it", L1_TUNE}, {"Let it breathe", L1_BREATHE},
    {"Into the hall", L1_HALL}, {"A second voice", L1_VOICE}, {"The cave", L1_CAVE}, {0, 0}};

/* the drone again, for the lessons after the first (the FM-1 forgets what was not saved when it is switched off) */
static const act_t SETUP[] = {
    HOLD(C_HOME, "Hold HOME: the menu"),
    TURN_TO(C_ALGO, U_TAB, MTAB_SYSTEM, 0, "Turn ALGORITHM to the SYSTEM tab"),
    TURN_TO(C_PRESETS, U_ROW, MI_SCENE, 0, "PRESETS to the SCENE row"),
    TURN_TO(C_OCTUP, U_SCENE, 4, 0, "SCENE should say BLANK (OCT+ / OCT- change it)"),
    TURN_TO(C_PRESETS, U_ROW, MI_RESTORE, 0, "PRESETS down to RESTORE LAST"),
    TURN_TO(C_OCTDN, U_RESTORE, 1, 0, "RESTORE LAST should say OFF (OCT- sets it)"),
    TAP(C_HOME, "Tap HOME to close the menu"),
    SAY(C_POWER, "Switch the FM-1 off, then on again", 5),
    TURN_TO(C_ALGO, U_TRACK, 3, 0, "Turn ALGORITHM to 4: DRONE WHEEL"),
    TAPS(C_SAVE, "PHRASES", "Tap SAVE until PHRASES"),
    TURN_TO(C_K1, U_PICK, PEDAL, 0, "Turn KNOB 1 to 17 PEDAL"),
    TAP(C_OCTUP, "Press OCT+ to load it"),
    TAP(C_PLAY, "Press PLAY: the drone from Lesson 1"),
    SAY(C_NONE, "Now we build on it", 4), END};

/* ---------------------------------------------------------------- LESSON 2: A MELODY IN THE DARK */
#define BALLAD 14
static const act_t L2_GURDY[] = {
    TURN_TO(C_ALGO, U_TRACK, 0, 0, "Turn ALGORITHM to 1: HURDY GURDY"),
    SAY(C_NONE, "Track 1 is a hurdy-gurdy with no notes yet", 4.5f), END};
static const act_t L2_SCALE[] = {
    TAP(C_SCL, "Tap SCL: the scale"),
    TURN_TO(C_K1, U_VALUE, 2, 0, "KNOB 1 (ROOT) to D"),
    TURN_TO(C_K2, U_VALUE, 2, 0, "KNOB 2 (SCL) to MIN: minor"),
    TURN_TO(C_K3, U_VALUE, 2, 0, "Turn KNOB 3 (QNT) to WHITE"),
    SAY(C_NONE, "Now the white keys play only D minor: no wrong notes", 5.5f),
    SAY(C_NONE, "The black keys go silent. Middle C plays D", 5), END};
static const act_t L2_PLAY[] = {
    SAY(C_NONE, "Play slowly. The sword shows each key", 4),
    KEY(14, "A", 1.6f), KEY(12, "G", 1.4f), KEY(11, "F", 1.4f), KEY(9, "E", 1.4f), KEY(7, "D: home", 2.6f),
    KEY(11, "F", 1.4f), KEY(9, "E", 1.4f), KEY(6, "C, below", 1.6f), KEY(7, "D: home", 3.0f),
    SAY(C_NONE, "A falling line over the drone: that is the sound", 5.5f), END};
static const act_t L2_BALLAD[] = {
    TAPS(C_SAVE, "PHRASES", "Tap SAVE until PHRASES"),
    TURN_TO(C_K1, U_PICK, BALLAD, 0, "Turn KNOB 1 to 15 BALLAD"),
    TAP(C_OCTUP, "Press OCT+ to load it"),
    SAY(C_NONE, "A melody that plays itself, over the drone", 12), END};
static const act_t L2_BUZZ[] = {
    TAPS(C_EDIT, "EDIT 2", "Tap EDIT twice"),
    TURN_TO(C_K1, U_VALUE, 0, 0, "Turn KNOB 1 (BUZZ) all the way down"),
    SAY(C_NONE, "A plain, sad string", 6),
    TURN_TO(C_K1, U_VALUE, 110, 0, "Turn KNOB 1 back up: the buzzing bridge"),
    TURN_TO(C_K2, U_VALUE, 0, 0, "Turn KNOB 2 (COUP) to HOLD: it buzzes all the time"),
    SAY(C_NONE, "A constant snarl", 5),
    TURN_TO(C_K2, U_VALUE, 2, 0, "KNOB 2 back to 1/8: the buzz strikes in time"),
    SAY(C_NONE, "The hurdy-gurdy's dance rhythm", 7),
    TAP(C_PLAY, "Press PLAY to stop"), END};
static const lstep_t L2_STEPS[] = {
    {"Set up", SETUP}, {"The hurdy-gurdy", L2_GURDY}, {"No wrong notes", L2_SCALE}, {"Play a phrase", L2_PLAY},
    {"A melody that plays itself", L2_BALLAD}, {"The buzzing bridge", L2_BUZZ}, {0, 0}};

/* ---------------------------------------------------------------- LESSON 3: THE CRYPT PAD */
#define DIRGE 13
static const act_t L3_CHORDS[] = {
    TURN_TO(C_ALGO, U_TRACK, 1, 0, "Turn ALGORITHM to 2: CRYPT PAD"),
    TAPS(C_SAVE, "PHRASES", "Tap SAVE until PHRASES"),
    TURN_TO(C_K1, U_PICK, DIRGE, 0, "Turn KNOB 1 to 14 DIRGE"),
    TAP(C_OCTUP, "Press OCT+ to load it"),
    SAY(C_NONE, "Slow chords: D minor, B flat, C, D minor", 12), END};
static const act_t L3_ATTACK[] = {
    TAP(C_ENV, "Tap ENV: the envelope"),
    TURN_TO(C_K1, U_VALUE, 0, 0, "Turn KNOB 1 (ATK) all the way down"),
    SAY(C_NONE, "Each chord starts at once", 7),
    TURN_TO(C_K1, U_VALUE, 110, 0, "Turn KNOB 1 up: each chord swells in"),
    SAY(C_NONE, "Like a choir taking a breath", 8), END};
static const act_t L3_RELEASE[] = {
    TURN_TO(C_K4, U_VALUE, 0, 0, "Turn KNOB 4 (REL) all the way down"),
    SAY(C_NONE, "Each chord stops dead", 7),
    TURN_TO(C_K4, U_VALUE, 120, 0, "Turn KNOB 4 up: they ring on and blur"),
    SAY(C_NONE, "Into the hall", 7), END};
static const act_t L3_HARP[] = {
    TURN_TO(C_K3, U_VALUE, 0, 0, "Turn KNOB 3 (SUS) all the way down"),
    TURN_TO(C_K1, U_VALUE, 0, 0, "Turn KNOB 1 (ATK) all the way down"),
    TURN_TO(C_K2, U_VALUE, 40, 0, "Turn KNOB 2 (DEC) to about a third"),
    SAY(C_NONE, "The same sound, plucked: a harp", 9),
    TURN_TO(C_K3, U_VALUE, 127, 0, "Turn KNOB 3 (SUS) all the way up"),
    TURN_TO(C_K1, U_VALUE, 90, 0, "Turn KNOB 1 (ATK) up again: a pad"),
    SAY(C_NONE, "Four knobs: from a harp to a choir", 6), END};
static const act_t L3_ALL[] = {
    TURN_TO(C_ALGO, U_TRACK, 0, 0, "Turn ALGORITHM to 1: HURDY GURDY"),
    TAPS(C_SAVE, "PHRASES", "Tap SAVE until PHRASES"),
    TURN_TO(C_K1, U_PICK, BALLAD, 0, "Turn KNOB 1 to 15 BALLAD"),
    TAP(C_OCTUP, "Press OCT+ to load it"),
    SAY(C_NONE, "Drone, chords and melody: a dungeon, by hand", 14),
    TAP(C_PLAY, "Press PLAY to stop"), END};
static const lstep_t L3_STEPS[] = {
    {"Set up", SETUP}, {"Chords", L3_CHORDS}, {"How a note starts", L3_ATTACK}, {"How a note ends", L3_RELEASE},
    {"Pad into harp", L3_HARP}, {"All together", L3_ALL}, {0, 0}};

/* ---------------------------------------------------------------- LESSON 4: MOVEMENT */
#define CHANT 15
static const act_t L4_MONKS[] = {
    TURN_TO(C_ALGO, U_TRACK, 2, 0, "Turn ALGORITHM to 3: MONKS"),
    TAPS(C_SAVE, "PHRASES", "Tap SAVE until PHRASES"),
    TURN_TO(C_K1, U_PICK, CHANT, 0, "Turn KNOB 1 to 16 CHANT"),
    TAP(C_OCTUP, "Press OCT+ to load it"),
    SAY(C_NONE, "Low voices: steady, unmoving", 8), END};
static const act_t L4_TREMOLO[] = {
    TAPS(C_LFO, "LFO DEST", "Tap LFO until LFO DEST"),
    SAY(C_NONE, "The LFO is a slow, hidden hand on a knob", 5),
    TURN_TO(C_K4, U_VALUE, 80, 0, "Turn KNOB 4 (AMP) up: the voices pulse"),
    SAY(C_NONE, "Loudness rising and falling: tremolo", 6),
    TAPS(C_LFO, "LFO", "Tap LFO until LFO (the rate page)"),
    TURN_TO(C_K1, U_VALUE, 25, 0, "Turn KNOB 1 (RATE) down: slower, like breathing"),
    SAY(C_NONE, "Slow movement feels alive", 8), END};
static const act_t L4_VIBRATO[] = {
    TURN_TO(C_ALGO, U_TRACK, 0, 0, "Turn ALGORITHM to 1: HURDY GURDY"),
    TAPS(C_SAVE, "PHRASES", "Tap SAVE until PHRASES"),
    TURN_TO(C_K1, U_PICK, BALLAD, 0, "Turn KNOB 1 to 15 BALLAD"),
    TAP(C_OCTUP, "Press OCT+ to load it"),
    TAPS(C_LFO, "LFO DEST", "Tap LFO until LFO DEST"),
    TURN_TO(C_K1, U_VALUE, 10, 0, "Turn KNOB 1 (PIT) up a little: vibrato"),
    SAY(C_NONE, "The pitch wavers, as a player's hand would", 7),
    TAPS(C_LFO, "LFO", "Tap LFO until LFO"),
    TURN_TO(C_K4, U_VALUE, 70, 0, "Turn KNOB 4 (FADE) up: the vibrato comes in late"),
    SAY(C_NONE, "Each long note starts plain, then sings", 10), END};
static const act_t L4_TIDE[] = {
    TURN_TO(C_ALGO, U_TRACK, 1, 0, "Turn ALGORITHM to 2: CRYPT PAD"),
    TAPS(C_SAVE, "PHRASES", "Tap SAVE until PHRASES"),
    TURN_TO(C_K1, U_PICK, DIRGE, 0, "Turn KNOB 1 to 14 DIRGE"),
    TAP(C_OCTUP, "Press OCT+ to load it"),
    TAPS(C_LFO, "LFO", "Tap LFO until LFO"),
    TURN_TO(C_K1, U_VALUE, 5, 0, "Turn KNOB 1 (RATE) almost all the way down"),
    TAPS(C_LFO, "LFO DEST", "Tap LFO until LFO DEST"),
    TURN_TO(C_K2, U_VALUE, 40, 0, "Turn KNOB 2 (FLT) up: the pad darkens and brightens"),
    SAY(C_NONE, "A slow tide, over many seconds", 14),
    TAP(C_PLAY, "Press PLAY to stop"), END};
static const lstep_t L4_STEPS[] = {
    {"Set up", SETUP}, {"The monks", L4_MONKS}, {"Tremolo", L4_TREMOLO}, {"Vibrato", L4_VIBRATO},
    {"A slow tide", L4_TIDE}, {0, 0}};

/* ---------------------------------------------------------------- LESSON 5: THE CASSETTE */
static const act_t L5_DUNGEON[] = {
    HOLD(C_HOME, "Hold HOME: the menu"),
    TURN_TO(C_ALGO, U_TAB, MTAB_SYSTEM, 0, "Turn ALGORITHM to the SYSTEM tab"),
    TURN_TO(C_PRESETS, U_ROW, MI_SCENE, 0, "PRESETS to the SCENE row"),
    TURN_TO(C_OCTUP, U_SCENE, 0, 0, "Press OCT- until SCENE says DUNGEON"),
    TAP(C_HOME, "Tap HOME to close the menu"),
    SAY(C_POWER, "Switch the FM-1 off, then on again", 5),
    TAP(C_PLAY, "Press PLAY"),
    SAY(C_NONE, "The whole DUNGEON scene: everything from Lessons 1 to 4", 10), END};
static const act_t L5_BALANCE[] = {
    TAPS(C_HOME, "MIXER", "Tap HOME until the MIXER"),
    TURN_TO(C_ALGO, U_TRACK, 3, 0, "Turn ALGORITHM to 4: the drone"),
    TURN_TO(C_K1, U_VALUE, 84, 0, "Turn KNOB 1 (LEVEL) down a little"),
    SAY(C_NONE, "The drone sits under everything", 5),
    TURN_TO(C_ALGO, U_TRACK, 0, 0, "Turn ALGORITHM to 1: the melody"),
    TURN_TO(C_K1, U_VALUE, 112, 0, "Turn KNOB 1 up a little: the melody in front"),
    SAY(C_NONE, "Balance is most of mixing", 6), END};
static const act_t L5_TAPE[] = {
    TAPS(C_FX, "TAPE", "Tap FX until TAPE"),
    TURN_TO(C_K1, U_VALUE, 100, 0, "Turn KNOB 1 (TAPE) up: a worn cassette"),
    SAY(C_NONE, "Wow, flutter and hiss: the tape stretches", 9),
    TURN_TO(C_K2, U_VALUE, 35, 0, "Turn KNOB 2 (CRSH) up a little: grit"),
    SAY(C_NONE, "Like a cheap sampler. A little goes a long way", 8), END};
static const act_t L5_MUTES[] = {
    SAY(C_NONE, "Hold GLO: the first four black keys mute tracks 1 to 4", 5.5f),
    MUTE(1, 0, "Hold GLO, tap the 1st black key: the melody drops out"),
    SAY(C_NONE, "Only the bed is left", 7),
    MUTE(5, 2, "Hold GLO, tap the 3rd black key: the monks go"),
    SAY(C_NONE, "Strip it down to pad and drone", 7),
    MUTE(5, 2, "Again: the monks return"),
    MUTE(1, 0, "Again on the 1st: the melody returns"),
    SAY(C_NONE, "Building up and breaking down: a live performance", 9), END};
static const act_t L5_RECORD[] = {
    SAY(C_NONE, "Connect the FM-1 to a computer with a USB data cable", 5),
    SAY(C_NONE, "It shows up as an audio input: record it in any app", 5),
    SAY(C_NONE, "Audacity is free. The tape and hiss are in the recording", 6),
    TAP(C_PLAY, "Press PLAY to stop"),
    SAY(C_NONE, "You have made a dungeon-synth piece", 6), END};
static const act_t L5_KEEP[] = {
    HOLD(C_HOME, "Hold HOME: the menu"),
    TURN_TO(C_ALGO, U_TAB, MTAB_SYSTEM, 0, "Turn ALGORITHM to the SYSTEM tab"),
    TURN_TO(C_PRESETS, U_ROW, MI_RESTORE, 0, "Turn PRESETS to RESTORE LAST"),
    TURN_TO(C_OCTUP, U_RESTORE, 0, 0, "Press OCT+ until it says ON"),
    TAP(C_HOME, "Tap HOME to close the menu"),
    SAY(C_NONE, "From now on the FM-1 wakes up with what you were playing", 6), END};
static const lstep_t L5_STEPS[] = {
    {"The whole dungeon", L5_DUNGEON}, {"Balance", L5_BALANCE}, {"The cassette", L5_TAPE},
    {"Play the mutes", L5_MUTES}, {"Record it", L5_RECORD}, {"Keep your music", L5_KEEP}, {0, 0}};

/* ---------------------------------------------------------------- LESSON 6: YOUR OWN TUNE */
static const act_t L6T_KEY[] = {
    TURN_TO(C_ALGO, U_TRACK, 0, 0, "Turn ALGORITHM to 1: HURDY GURDY"),
    TAP(C_SCL, "Tap SCL: as in Lesson 2"),
    TURN_TO(C_K1, U_VALUE, 2, 0, "KNOB 1 (ROOT) to D"),
    TURN_TO(C_K2, U_VALUE, 2, 0, "KNOB 2 (SCL) to MIN"),
    TURN_TO(C_K3, U_VALUE, 2, 0, "KNOB 3 (QNT) to WHITE: no wrong notes"),
    SAY(C_NONE, "The white keys from C play D minor", 4.5f), END};
static const act_t L6T_REC[] = {
    TAP(C_REC, "Tap REC: track 1 records what you play"),
    SAY(C_NONE, "It is already playing. Play along, slowly", 3),
    KEY(14, "G. Hold each note a moment", 1.7f),   /* (WHITE: G plays A, F G, E F, C D; one pass: 6.7 s) */
    KEY(12, "F", 1.45f),
    KEY(11, "E", 1.45f),
    KEY(7, "C: it plays D, home", 1.95f),
    SAY(C_NONE, "What you play is written into the steps", 5), END};
static const act_t L6T_LISTEN[] = {
    TAP(C_REC, "Tap REC again: recording stops"),
    SAY(C_NONE, "The tune plays itself, every time round", 9),
    SAY(C_NONE, "Not quite right? Play over it again: it adds", 5), END};
static const act_t L6T_FIX[] = {
    TAP(C_SEQ, "Tap SEQ: the STEP page, your notes"),
    TURN_TO(C_PRESETS, U_AFTERNOTE, 0, 0, "Turn PRESETS: the cursor to the step after a note"),
    TURN_TO(C_K3, U_STEPTIME, ST_TIE, 0, "Turn KNOB 3 (TIME) to TIE: the note holds longer"),
    SAY(C_NONE, "Listen: that note now rings through the next step", 8), END};
static const act_t L6T_CHANCE[] = {
    TAPS(C_SEQ, "AUTOMATION", "Tap SEQ again: AUTOMATION"),
    TURN_TO(C_K1, U_EVADD, 0, 0, "Turn KNOB 1 to the last row: + ADD"),
    TURN_TO(C_K2, U_EVNOTE, 0, 0, "Turn KNOB 2 to a step with a note"),
    TURN_TO(C_K3, U_EVCHANCE, 0, 0, "KNOB 3 shows CHANCE"),
    TAP(C_OCTUP, "Press OCT+: that note plays half the time"),
    SAY(C_NONE, "Now each pass is a little different", 12), END};
static const act_t L6T_KEEP[] = {
    TAP(C_PLAY, "Press PLAY to stop: saving needs it stopped"),
    TAPS(C_SAVE, "PROJECT", "Tap SAVE until PROJECT"),
    SAY(C_K1, "KNOB 1 is the slot: 1 of 4", 4),
    TURN_TO(C_K4, U_ACT, 4, 0, "Turn KNOB 4 (SAVE) right to pick it"),
    TAP(C_OCTUP, "Press OCT+"),
    SAY(C_NONE, "Name it with the keys, as on a phone. Or skip it", 6),
    TAP(C_OCTUP, "Press OCT+ to save"),
    SAY(C_NONE, "Your tune is kept. To load it: KNOB 3 (LOAD) here", 6), END};
static const lstep_t L6T_STEPS[] = {
    {"Set up", SETUP}, {"The key", L6T_KEY}, {"Record it", L6T_REC}, {"Listen back", L6T_LISTEN},
    {"Hold a note longer", L6T_FIX}, {"Let it breathe", L6T_CHANCE}, {"Keep it", L6T_KEEP}, {0, 0}};

/* ---------------------------------------------------------------- LESSON 7: THE HARP ARPEGGIO */
#define DMIN 7, 11, 14                                 /* (WHITE in D minor: C E G play D F A) */
#define BFLAT 4, 7, 11                                 /* A C E: B-flat D F */
#define CMAJ 6, 9, 12                                  /* B D F: C E G */
static const act_t L7_HARP[] = {
    TURN_TO(C_ALGO, U_TRACK, 0, 0, "Turn ALGORITHM to 1"),
    TAP(C_SCL, "Tap SCL: as in Lesson 2"),
    TURN_TO(C_PRESETS, U_SOUND, 0, "FOLK HARP", "Turn PRESETS to FOLK HARP"),
    TURN_TO(C_K1, U_VALUE, 2, 0, "KNOB 1 (ROOT) to D"),
    TURN_TO(C_K2, U_VALUE, 2, 0, "KNOB 2 (SCL) to MIN"),
    TURN_TO(C_K3, U_VALUE, 2, 0, "KNOB 3 (QNT) to WHITE: no wrong notes"),
    CHORD(DMIN, "Hold C, E and G together: D minor", 3.5f),
    SAY(C_NONE, "A chord, all at once. A harpist rolls it", 4), END};
static const act_t L7_ARP[] = {
    TAP(C_ARP, "Tap ARP"),
    TURN_TO(C_K1, U_VALUE, 3, 0, "Turn KNOB 1 (MODE) to UPDN: up, then down"),
    TURN_TO(C_K2, U_VALUE, 1, 0, "Turn KNOB 2 (RATE) to 1/8"),
    TURN_TO(C_K3, U_VALUE, 2, 0, "Turn KNOB 3 (OCT) to 2: two octaves"),
    CHORD(DMIN, "Hold C, E and G again: the harp rolls", 9),
    SAY(C_NONE, "One note at a time, in time with the drone", 4), END};
static const act_t L7_CHORDS[] = {
    CHORD(DMIN, "Hold C, E, G: D minor", 5.5f),
    CHORD(BFLAT, "The same shape on A, C, E: B-flat", 5.5f),
    CHORD(CMAJ, "On B, D, F: C major", 5.5f),
    CHORD(DMIN, "Back to C, E, G: home", 6),
    SAY(C_NONE, "Three shapes, one hand: a whole dungeon ballad", 4.5f), END};
static const act_t L7_HOLD[] = {
    TAPS(C_ARP, "ARP 2", "Tap ARP again: ARP 2"),
    TURN_TO(C_K3, U_VALUE, 1, 0, "Turn KNOB 3 (HOLD) to ON"),
    CHORD(DMIN, "Play C, E, G, then let go", 1.5f),
    SAY(C_NONE, "It keeps playing: hands free", 6),
    CHORD(BFLAT, "Play A, C, E: the new chord takes over", 1.5f),
    SAY(C_NONE, "Change the chord whenever you like", 6),
    CHORD(DMIN, "C, E, G: back home", 1.5f), END};
static const act_t L7_SHAPE[] = {
    TURN_TO(C_K2, U_VALUE, 70, 0, "Turn KNOB 2 (PROB) down: some notes rest"),
    SAY(C_NONE, "Like a player, not a machine", 7),
    TAPS(C_ARP, "ARP", "Tap ARP: back to the first page"),
    TURN_TO(C_K4, U_VALUE, 20, 0, "Turn KNOB 4 (GATE) down: short, plucked"),
    SAY(C_NONE, "Each string just touched", 6),
    TURN_TO(C_K1, U_VALUE, 4, 0, "Turn KNOB 1 (MODE) to RND: random order"),
    SAY(C_NONE, "Wind through the strings", 8), END};
static const act_t L7_END[] = {
    TURN_TO(C_K1, U_VALUE, 3, 0, "KNOB 1 (MODE) back to UPDN"),
    TURN_TO(C_K4, U_VALUE, 64, 0, "KNOB 4 (GATE) back to the middle"),
    SAY(C_NONE, "The arpeggiator works on any sound: try the LUTE", 5),
    SAY(C_NONE, "Next: drums for a ritual", 5), END};
static const lstep_t L7_STEPS[] = {
    {"Set up", SETUP}, {"A harp", L7_HARP}, {"Roll the chord", L7_ARP}, {"Change chords", L7_CHORDS},
    {"Hands free", L7_HOLD}, {"Shape it", L7_SHAPE}, {"Leave it playing", L7_END}, {0, 0}};

/* ---------------------------------------------------------------- LESSON 8: RITUAL DRUMS */
/* the DRUM grid (seq.c key_place): white keys from F are steps 1..16; black keys 1..8 the lanes, 9 ACC */
#define LN_FRAME 1
#define LN_TIMP 13
#define LN_SHAKR 8
static const act_t L8_KIT[] = {
    TURN_TO(C_ALGO, U_TRACK, 2, 0, "Turn ALGORITHM to 3"),
    TAPS(C_HOME, "MIXER", "Tap HOME until the MIXER"),
    TURN_TO(C_PRESETS, U_SOUND, 0, "TOMB DRUMS", "Turn PRESETS to TOMB DRUMS"),
    SAY(C_NONE, "Low, slow drums, far back in the hall", 4), END};
static const act_t L8_GRID[] = {
    TAP(C_SEQ, "Tap SEQ: on a drum track, a drum grid"),
    SAY(C_NONE, "The white keys are the steps: 16 of them, left to right", 5),
    SAY(C_NONE, "The black keys pick the drum", 4),
    KEY(LN_FRAME, "1st black key, FRAME, the frame drum", 1.6f),
    KEY(0, "1st white key, step 1: a hit", 1.6f),
    KEY(6, "4th white key, step 4", 1.6f),
    KEY(14, "9th white key, step 9", 1.6f),
    SAY(C_NONE, "BOOM . . boom . . . . BOOM: a slow heartbeat", 8), END};
static const act_t L8_MORE[] = {
    KEY(LN_TIMP, "6th black key, TIMP, the timpani", 1.6f),
    KEY(21, "13th white key, step 13", 1.6f),
    KEY(24, "15th white key, step 15", 1.6f),
    SAY(C_NONE, "The timpani rolls into the next round", 7),
    KEY(LN_SHAKR, "4th black key, SHAKR, a shaker", 1.6f),
    KEY(7, "5th white key, step 5", 1.6f),
    KEY(16, "10th white key, step 10", 1.6f),
    SAY(C_NONE, "Few hits, far apart: that is a ritual", 8), END};
static const act_t L8_ACCENT[] = {
    KEY(LN_FRAME, "1st black key, back to the frame drum", 1.6f),
    TURN(C_K1, -1, "Turn KNOB 1 (STEP) left: the cursor back to step 9"),
    TURN(C_K4, 1, "Turn KNOB 4 (ACC) right: step 9 hits harder"),
    SAY(C_NONE, "The big beat of the procession", 7), END};
static const act_t L8_HALL[] = {
    TAPS(C_FX, "FX", "Tap FX: the track's sends"),
    TURN_TO(C_K4, U_VALUE, 120, 0, "Turn KNOB 4 (REV) up: deep in the hall"),
    SAY(C_NONE, "Drums from somewhere under the castle", 8), END};
static const act_t L8_PLAY[] = {
    MUTE(5, 2, "Hold GLO, tap the 3rd black key, the drums stop"),
    SAY(C_NONE, "Just the drone again", 6),
    MUTE(5, 2, "Again: the procession returns"),
    SAY(C_NONE, "As in Lesson 5: bring them in when the piece needs them", 7),
    SAY(C_NONE, "Next: shape a sound of your own", 5), END};
static const lstep_t L8_STEPS[] = {
    {"Set up", SETUP}, {"A drum kit", L8_KIT}, {"The drum grid", L8_GRID}, {"More drums", L8_MORE},
    {"One beat harder", L8_ACCENT}, {"Into the hall", L8_HALL}, {"Bring them in", L8_PLAY}, {0, 0}};

/* ---------------------------------------------------------------- LESSON 9: SOUND DESIGN */
static const act_t L9_PAD[] = {
    TURN_TO(C_ALGO, U_TRACK, 1, 0, "Turn ALGORITHM to 2: CRYPT PAD"),
    TURN_TO(C_K1, U_PICK, DIRGE, 0, "Still on PHRASES: turn KNOB 1 to 14 DIRGE"),
    TAP(C_OCTUP, "Press OCT+ to load it: the chords from Lesson 3"),
    TAP(C_HOME, "Tap HOME: the sound's main controls"),
    SAY(C_NONE, "Now we change the sound itself, not the notes", 5), END};
static const act_t L9_FILTER[] = {
    TURN_TO(C_K1, U_VALUE, 10, 0, "Turn KNOB 1 (CUT) down: dark, behind a wall"),
    SAY(C_NONE, "The filter takes the bright part away", 5),
    TURN_TO(C_K1, U_VALUE, 95, 0, "Turn KNOB 1 up: bright and close"),
    TURN_TO(C_K2, U_VALUE, 100, 0, "Turn KNOB 2 (RES) up: the filter rings"),
    SAY(C_NONE, "A whistle at the edge of the sound", 5),
    TURN_TO(C_K1, U_VALUE, 50, 0, "KNOB 1 (CUT) to the middle"),
    TURN_TO(C_K2, U_VALUE, 45, 0, "KNOB 2 (RES) down a little"), END};
static const act_t L9_OSC[] = {
    TAP(C_EDIT, "Tap EDIT: the engine's own controls"),
    SAY(C_NONE, "OSC: where the sound starts", 3.5f),
    TURN_TO(C_K1, U_VALUE, 1, 0, "Turn KNOB 1 (WAVE) to SQR: hollow, reedy"),
    SAY(C_NONE, "Like a clarinet in a crypt", 5),
    TURN_TO(C_K1, U_VALUE, 4, 0, "Turn KNOB 1 to PWM: it keeps moving"),
    SAY(C_NONE, "Rich and restless", 5),
    TURN_TO(C_K2, U_VALUE, 40, 0, "Turn KNOB 2 (DTN) up: two oscillators apart"),
    SAY(C_NONE, "Wider, and a little uneasy", 5),
    TURN_TO(C_K4, U_VALUE, 45, 0, "Turn KNOB 4 (NOIS) up: breath in the pipes"),
    SAY(C_NONE, "A sound no preset had", 6), END};
static const act_t L9_KEEP[] = {
    TAP(C_PLAY, "Press PLAY to stop: saving needs it stopped"),
    TAPS(C_SAVE, "USER", "Tap SAVE until USER"),
    SAY(C_K1, "KNOB 1 is the slot: 32 of them for your sounds", 4),
    SAY(C_K4, "An empty slot: KNOB 4 (SAVE) is already picked", 4),
    TAP(C_OCTUP, "Press OCT+"),
    SAY(C_NONE, "Name it with the keys, as on a phone. Or skip it", 6),
    TAP(C_OCTUP, "Press OCT+ to save"),
    SAY(C_NONE, "U01 is yours: KNOB 2 (LOAD) puts it on any track", 6),
    TAP(C_PLAY, "Press PLAY: play on"),
    SAY(C_NONE, "Next: make it change over time, by itself", 6), END};
static const lstep_t L9_STEPS[] = {
    {"Set up", SETUP}, {"A pad to shape", L9_PAD}, {"Bright and dark", L9_FILTER}, {"Where sound starts", L9_OSC},
    {"Keep your sound", L9_KEEP}, {0, 0}};

/* ---------------------------------------------------------------- LESSON 10: THE LIVING PATTERN */
static const act_t L10_TUNE[] = {
    TURN_TO(C_ALGO, U_TRACK, 0, 0, "Turn ALGORITHM to 1: HURDY GURDY"),
    TURN_TO(C_K1, U_PICK, BALLAD, 0, "Still on PHRASES: turn KNOB 1 to 15 BALLAD"),
    TAP(C_OCTUP, "Press OCT+ to load it: the tune from Lesson 2"),
    TAP(C_HOME, "Tap HOME: the gurdy's main controls"),
    SAY(C_NONE, "The same every time round. Let's make it move", 5), END};
static const act_t L10_REC[] = {
    TAP(C_REC, "Tap REC: track 1 records, knobs too"),
    TURN_TO(C_K1, U_VALUE, 0, 0, "Turn KNOB 1 (BUZZ) down slowly"),
    SAY(C_NONE, "Wait a moment", 2.5f),
    TURN_TO(C_K1, U_VALUE, 127, 0, "Then all the way up"),
    TAP(C_REC, "Tap REC again: recording stops"),
    SAY(C_NONE, "Hands off. The buzz comes and goes by itself", 10),
    SAY(C_NONE, "Your knob move is part of the pattern now", 6), END};
static const act_t L10_SEE[] = {
    TAPS(C_SEQ, "AUTOMATION", "Tap SEQ until AUTOMATION"),
    SAY(C_NONE, "Each row: a step, and BUZZ's value there", 6),
    SAY(C_NONE, "Change one with the knobs, or EDIT to remove it", 5), END};
static const act_t L10_RATCH[] = {
    TURN_TO(C_K1, U_EVADD, 0, 0, "Turn KNOB 1 to the last row: + ADD"),
    TURN_TO(C_K2, U_EVNOTE, 0, 0, "KNOB 2 picks the step: step 1 has a note"),
    TURN_TO(C_K3, U_EVCHANCE, 1, 0, "Turn KNOB 3 left to RATCH"),
    TAP(C_OCTUP, "Press OCT+: that note plays twice"),
    TURN(C_K4, 1, "Turn KNOB 4 right: three times"),
    SAY(C_NONE, "A quick flourish, like a player's ornament", 10),
    SAY(C_NONE, "With Lesson 6's CHANCE: a tune that never quite repeats", 6), END};
static const act_t L10_END[] = {
    SAY(C_NONE, "Record any knob this way: the filter, the reverb send", 6),
    SAY(C_NONE, "Next: two sounds at once, on one keyboard", 6), END};
static const lstep_t L10_STEPS[] = {
    {"Set up", SETUP}, {"A tune", L10_TUNE}, {"Record a knob", L10_REC}, {"See it", L10_SEE},
    {"An ornament", L10_RATCH}, {"Make it move", L10_END}, {0, 0}};

/* ---------------------------------------------------------------- LESSON 11: TWO HANDS */
#define LH_DMIN 0, 4, 9                                /* (below SPLT E4) F3 A3 D4: D minor */
#define LH_BFLAT 0, 5, 9                               /* F3 A#3 D4: B-flat */
#define LH_GMIN 2, 5, 9                                /* G3 A#3 D4: G minor */
static const act_t L11_KEYS[] = {
    TURN_TO(C_ALGO, U_TRACK, 0, 0, "Turn ALGORITHM to 1: HURDY GURDY"),
    TAPS(C_HOME, "KEYS", "Tap HOME until KEYS: after MIXER and CLOCK"),
    SAY(C_NONE, "KEYS: one keyboard, two tracks", 4), END};
static const act_t L11_LAYER[] = {
    TURN_TO(C_K1, U_VALUE, 1, 0, "Turn KNOB 1 (MODE) to LAYER"),
    TURN_TO(C_K2, U_VALUE, 1, 0, "Turn KNOB 2 (TRK) to T2: CRYPT PAD"),
    KEY(9, "D: the gurdy and the pad together", 2.2f), KEY(12, "F", 1.4f), KEY(16, "A", 1.4f),
    KEY(14, "G", 1.4f), KEY(12, "F", 1.4f), KEY(11, "E", 1.4f), KEY(9, "D", 2.4f),
    TURN_TO(C_K4, U_VALUE, 1, 0, "Turn KNOB 4 (OCT) to -1: the pad an octave down"),
    KEY(16, "A", 1.6f), KEY(14, "G", 1.4f), KEY(12, "F", 1.4f), KEY(9, "D", 2.6f),
    SAY(C_NONE, "A tune doubled, low and soft underneath", 4), END};
static const act_t L11_SPLIT[] = {
    TURN_TO(C_K1, U_VALUE, 2, 0, "Turn KNOB 1 (MODE) to SPLIT"),
    TURN_TO(C_K4, U_VALUE, 2, 0, "KNOB 4 (OCT) back to 0"),
    TURN_TO(C_K3, U_VALUE, 8, 0, "Turn KNOB 3 (SPLT) to E4: the split point"),
    SAY(C_NONE, "Below E: the pad, left hand. From E up: the gurdy", 5),
    PRESS(LH_DMIN, "Left hand: F, A, D, and keep holding", 2.2f),
    KEY(16, "Right hand: A", 1.4f), KEY(14, "G", 1.4f), KEY(12, "F", 1.4f), KEY(14, "G", 1.4f), KEY(16, "A", 2.2f),
    LETGO("Let go"),
    PRESS(LH_BFLAT, "Left hand: F, A#, D: B-flat", 2.2f),
    KEY(21, "Right hand: high D", 1.4f), KEY(19, "C", 1.4f), KEY(17, "A#", 1.4f), KEY(16, "A", 2.4f),
    LETGO("Let go"),
    SAY(C_NONE, "Chords in one hand, a tune in the other", 4), END};
static const act_t L11_LATCH[] = {
    LATCH(15, 1, "Hold GLO, tap the 7th black key: track 2 latches"),
    CHORD(LH_DMIN, "Left hand: F, A, D, then let go", 1.6f),
    SAY(C_NONE, "The chord rings on by itself", 4),
    KEY(16, "Both hands free for the tune: A", 1.6f), KEY(17, "A#", 1.4f), KEY(16, "A", 1.4f), KEY(14, "G", 1.4f),
    KEY(12, "F", 1.4f), KEY(14, "G", 2.4f),
    CHORD(LH_GMIN, "A new chord replaces it: G, A#, D", 1.6f),
    KEY(17, "A#", 1.6f), KEY(16, "A", 1.4f), KEY(14, "G", 2.6f),
    LATCH(15, 1, "Hold GLO, the 7th black key again: unlatched"),
    SAY(C_NONE, "Latch a chord, then play over it as long as you like", 5), END};
static const act_t L11_END[] = {
    SAY(C_NONE, "KEYS is saved with the project", 4),
    SAY(C_NONE, "Next: the old ways, the gurdy and old tunings", 5), END};
static const lstep_t L11_STEPS[] = {
    {"Set up", SETUP}, {"One keyboard, two tracks", L11_KEYS}, {"Layer", L11_LAYER}, {"Split", L11_SPLIT},
    {"Hands free", L11_LATCH}, {"Keep it", L11_END}, {0, 0}};

/* ---------------------------------------------------------------- LESSON 12: THE OLD WAYS */
#define L12_REV 90                                     /* (the set-up's track 1 REV and track 4 LEVEL, put back) */
#define L12_LEVEL 104
#define MARCH 17                                       /* PATTERNS[17]: a stepping line, a rest after every note */
#define L12_PHRASE(first) KEY(14, first, 1.0f), KEY(12, "F", 0.95f), KEY(11, "E", 0.95f), KEY(9, "D", 0.95f), \
    KEY(7, "C", 0.95f), KEY(6, "B", 0.95f), KEY(7, "C: home", 1.9f)
static const act_t L12_SCALES[] = {
    TURN_TO(C_ALGO, U_TRACK, 0, 0, "Turn ALGORITHM to 1: HURDY GURDY"),
    TAP(C_SCL, "Tap SCL"),
    TURN_TO(C_K1, U_VALUE, 2, 0, "KNOB 1 (ROOT) to D"),
    TURN_TO(C_K2, U_VALUE, 2, 0, "KNOB 2 (SCL) to MIN"),
    TURN_TO(C_K3, U_VALUE, 2, 0, "KNOB 3 (QNT) to WHITE"),
    L12_PHRASE("Play from G down: plain minor"),
    TURN_TO(C_K2, U_VALUE, 7, 0, "Turn KNOB 2 (SCL) to HARM"),
    L12_PHRASE("The same keys: a castle"),
    TURN_TO(C_K2, U_VALUE, 17, 0, "Turn KNOB 2 to HUNG: Hungarian minor"),
    L12_PHRASE("Gothic, haunted"),
    TURN_TO(C_K2, U_VALUE, 2, 0, "KNOB 2 back to MIN"), END};
static const act_t L12_TEMP[] = {
    TAPS(C_SCL, "CHORD", "Tap SCL again: CHORD"),
    TURN_TO(C_K3, U_VALUE, 1, 0, "Turn KNOB 3 (TEMP) to PYTH"),
    SAY(C_NONE, "Pythagorean: tuned in pure fifths, as in old chant", 5),
    L12_PHRASE("Listen to the drones: still, not beating"),
    SAY(C_NONE, "MEAN and JUST: sweeter thirds, for slow chords", 4), END};
static const act_t L12_RING[] = {
    TAPS(C_SAVE, "PHRASES", "Tap SAVE until PHRASES"),
    TURN_TO(C_K1, U_PICK, MARCH, 0, "Turn KNOB 1 to 18 MARCH"),
    TAP(C_OCTUP, "Press OCT+: a step, a rest, a step"),
    TAPS(C_HOME, "MIXER", "Tap HOME until the MIXER"),
    TURN_TO(C_ALGO, U_TRACK, 3, 0, "Turn ALGORITHM to 4: the drone"),
    TURN_TO(C_K1, U_VALUE, 0, 0, "Turn KNOB 1 (LEVEL) down: the gurdy alone"),
    TURN_TO(C_ALGO, U_TRACK, 0, 0, "ALGORITHM back to 1"),
    TURN_TO(C_K3, U_VALUE, 8, 0, "Turn KNOB 3 (REV) down: dry, to hear the gaps"),
    TAPS(C_EDIT, "RING", "Tap EDIT until RING"),
    TURN_TO(C_K1, U_VALUE, 0, 0, "Turn KNOB 1 (RING) OFF"),
    SAY(C_NONE, "In every rest, the whole instrument stops", 6),
    TURN_TO(C_K1, U_VALUE, 1, 0, "Turn KNOB 1 back ON"),
    SAY(C_NONE, "The drones ring through: the wheel never stops", 6.5f), END};
static const act_t L12_CRANK[] = {
    TAPS(C_EDIT, "EDIT 2", "Tap EDIT until EDIT 2"),
    TURN_TO(C_K2, U_VALUE, 5, 0, "Turn KNOB 2 (COUP) to CRANK"),
    SAY(C_NONE, "SELECT is the crank now. Without it: silence", 5),
    CRANK(24, "Turn SELECT steadily: the wheel turns", 6),
    SAY(C_NONE, "Stop turning: the strings stop", 3),
    CRANK(13, "Slowly: soft and dark", 4.5f),
    CRANK(36, "Faster: louder and rougher", 4),
    CRANK(24, "Steady again", 2.5f),
    CRANK(55, "A sudden flick: the buzz kicks", 1.4f),
    CRANK(24, "And steady", 3),
    TURN_TO(C_K2, U_VALUE, 2, 0, "KNOB 2 (COUP) back to 1/8"),
    TAPS(C_HOME, "MIXER", "Tap HOME until the MIXER"),
    TURN_TO(C_K3, U_VALUE, L12_REV, 0, "KNOB 3 (REV) back up: into the hall"),
    TURN_TO(C_ALGO, U_TRACK, 3, 0, "ALGORITHM to 4"),
    TURN_TO(C_K1, U_VALUE, L12_LEVEL, 0, "KNOB 1 (LEVEL) back up: the drone returns"),
    SAY(C_NONE, "Next: a MIDI keyboard, or a computer", 5), END};
static const lstep_t L12_STEPS[] = {
    {"Set up", SETUP}, {"Old scales", L12_SCALES}, {"Old tunings", L12_TEMP}, {"Drones that ring", L12_RING},
    {"Turn the crank", L12_CRANK}, {0, 0}};

/* ---------------------------------------------------------------- 6: DREAM (GHOULBOX 2.3: what's new, not a lesson) */
static const act_t L6_FIND[] = {
    SAY(C_NONE, "GHOULBOX 2.3 can compose. This is DREAM", 4.5f),
    TAPS(C_HOME, "DREAM", "Tap HOME until DREAM: after MIXER, CLOCK, KEYS"),
    SAY(C_NONE, "Four knobs: EVOL, DENS, DRFT and MOOD", 4.5f), END};
static const act_t L6_FIRST[] = {
    SAY(C_K4, "KNOB 4 is the MOOD: MIN, a minor key", 4),
    KEY(9, "D. The piece will be in D", 2.5f),
    SAY(C_NONE, "It asks first: it replaces all four tracks", 4),
    TAP(C_OCTUP, "Press OCT+ for yes"),
    SAY(C_NONE, "The FM-1 writes a piece for its four sounds", 8),
    SAY(C_NONE, "A drone holds D, the pad and the monks the chords", 10),
    SAY(C_NONE, "The hurdy-gurdy sings a phrase, then answers it", 10),
    SAY(C_NONE, "Each note has a chance: no two passes are the same", 10), END};
static const act_t L6_EVOLVE[] = {
    TURN_TO(C_K3, U_VALUE, 8, 0, "Turn KNOB 3 (DRFT) to 8: a change every pass"),
    TURN_TO(C_K1, U_VALUE, 1, 0, "Turn KNOB 1 (EVOL) to ON"),
    SAY(C_NONE, "Now it changes by itself as it plays", 10),
    SAY(C_NONE, "A new chord, a new phrase, a part resting", 14),
    SAY(C_NONE, "The drone and the first phrase stay: the same piece", 14),
    SAY(C_NONE, "Leave it running for as long as you like", 10), END};
static const act_t L6_MOOD[] = {
    TURN_TO(C_K2, U_VALUE, 6, 0, "Turn KNOB 2 (DENS) to 6: busier"),
    TURN_TO(C_K4, U_VALUE, 2, 0, "Turn KNOB 4 (MOOD) to PHRY: darker"),
    KEY(4, "A. A new piece in A Phrygian", 2.5f),
    SAY(C_NONE, "At once this time: no question after the first", 8),
    SAY(C_NONE, "Same sounds, a new key and mood: a new piece", 12),
    SAY(C_NONE, "EVOL is still on: it keeps changing", 12), END};
static const act_t L6_YOURS[] = {
    SAY(C_NONE, "DREAM writes ordinary patterns: edit them, save them", 6),
    SAY(C_NONE, "Choose other sounds first: they dream other parts", 6),
    SAY(C_NONE, "Also new: RING keeps a gurdy's drones through rests", 6),
    SAY(C_NONE, "Free at ghoulbox.messy.quest", 6), END};
static const lstep_t L6_STEPS[] = {
    {"Find DREAM", L6_FIND}, {"A piece in D", L6_FIRST}, {"Let it change", L6_EVOLVE}, {"Another mood", L6_MOOD},
    {"Make it yours", L6_YOURS}, {0, 0}};

static const lesson_t LESSONS[] = {{L1_STEPS, 9, 0} /* (a new FM-1: DUNGEON) */, {L2_STEPS, 8, 4}, {L3_STEPS, 8, 4}, {L4_STEPS, 8, 4}, {L5_STEPS, 8, 4},
                                   {L6_STEPS, 8, 0} /* (index 6: 2.3's DREAM video, on DUNGEON) */,
                                   {L6T_STEPS, 8, 4} /* (index 7: LESSON 6, Your Own Tune) */,
                                   {L7_STEPS, 8, 4} /* (index 8: LESSON 7, The Harp Arpeggio) */,
                                   {L8_STEPS, 8, 4} /* (index 9: LESSON 8, Ritual Drums) */,
                                   {L9_STEPS, 8, 4} /* (index 10: LESSON 9, Sound Design) */,
                                   {L10_STEPS, 8, 4} /* (index 11: LESSON 10, The Living Pattern) */,
                                   {L11_STEPS, 8, 4} /* (index 12: LESSON 11, Two Hands) */,
                                   {L12_STEPS, 8, 4} /* (index 13: LESSON 12, The Old Ways) */};

/* ---------------------------------------------------------------- the player */
static FILE *wav, *vid;
static uint32_t now, next_frame;                       /* samples */
static int32_t lit_slot = -1;                          /* the card of the knob being turned */
static int dark;                                       /* the FM-1 switched off: a black screen */
static uint32_t left;                                  /* (lesson 11) the keys K_PRESS holds down */

static void frame_out(void)
{
    static uint8_t rgb[240u * 240u * 3u];
    uint32_t i;
    ui.bpm_t = 0;
    if (lit_slot >= 0) {
        ui.hot_col = (uint8_t)lit_slot;
        ui.hot_t = 20;
    }
    ui_draw();
    if (dark)
        memset(host_screen, 0, sizeof host_screen);
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        rgb[3 * i] = (uint8_t)((c >> 11) * 255u / 31u);
        rgb[3 * i + 1] = (uint8_t)(((c >> 5) & 63u) * 255u / 63u);
        rgb[3 * i + 2] = (uint8_t)((c & 31u) * 255u / 31u);
    }
    if (vid)
        fwrite(rgb, 1, sizeof rgb, vid);
}

static void run(float sec)                             /* the device runs: audio, the input loop, the screen */
{
    uint32_t end = now + (uint32_t)(sec * FS), i;
    int32_t o[2 * CTL];
    while (now < end) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            wav_put(wav, o[2 * i], o[2 * i + 1]);
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)clamp(o[2 * i], -32768, 32767);   /* as audio.c */
        }
        now += CTL;
        if (now >= next_frame) {                       /* 30 fps; the input loop twice a frame, as on the device */
            fm1_ms = (uint32_t)((uint64_t)now * 1000u / FS);
            fm1_in.frames = (uint32_t)((uint64_t)now * CRANK_FPS / FS);   /* (lesson 12: the input scan's frames, the crank's clock) */
            host_ticks += 16000u;
            ui_input();
            host_ticks += 16000u;
            ui_input();
            frame_out();
            next_frame += FS / 30u;
        }
    }
}

static void btn(uint32_t b, int down)
{
    if (down) {
        fm1_in.buttons |= 1u << panel.btn[b];
        host_pressed |= 1u << panel.btn[b];
    } else
        fm1_in.buttons &= ~(1u << panel.btn[b]);
}

static int16_t *knob_value(uint32_t slot)
{
    int16_t *v = 0;
    const page_t *pg = &PAGES[ui.page];
    if (ui.home && !ui.menu) {                         /* (lesson 9) HOME: the engine's four main controls */
        home_param(slot, &v);
        return v;
    }
    if (ui.home || ui.menu)
        return 0;
    if (pg->scope == SC_TRK) {                         /* the MIXER: LEVEL PAN REV MUTE of the track */
        static const uint8_t ID[4] = {P_LEVEL, P_PAN, P_REV, P_MUTE};
        return &TSEL->p[ID[slot]];
    }
    page_desc(pg, slot, &v);
    return v;
}

static int reached(const act_t *a)
{
    int16_t *v;
    switch (a->until) {
    case U_TRACK: return song.sel == a->arg;
    case U_SOUND: { char b[24] = {0}; sound_name(TSEL, b); return !strcmp(b, a->name); }
    case U_PICK: return ui.ppick == a->arg;
    case U_PAGE: return !ui.home && !ui.menu && !strcmp(PAGES[ui.page].title, a->name);
    case U_ROW: return ui.menu && ui.menu_sel == a->arg;
    case U_TAB: return menu_tab() == (uint32_t)a->arg;
    case U_SCENE: return ui_scene == a->arg;
    case U_RESTORE: return ((ui_prefs & PREF_RESTORE_OFF) != 0u) == (a->arg != 0);
    case U_VALUE: return (v = knob_value((uint32_t)(a->ctl - C_K1))) && *v == a->arg;
    case U_AFTERNOTE: return ui.cursor > 0 && TSEL->step[ui.cursor].time == ST_REST && TSEL->step[ui.cursor].n == 0 &&
                             (TSEL->step[ui.cursor - 1u].time == ST_TIE ||   /* (a held note's end) */
                              (TSEL->step[ui.cursor - 1u].time == ST_NOTE && TSEL->step[ui.cursor - 1u].n));
    case U_EVADD: { uint16_t rw[EV_ROWS]; uint32_t n; return (ev_cur(rw, &n) >> 8) == EVK_ADD; }
    case U_EVNOTE: return TSEL->step[ui.ev_step].time == ST_NOTE && TSEL->step[ui.ev_step].n;
    case U_EVCHANCE: return ui.ev_id == (a->arg ? EV_RATCH : EV_CHANCE);   /* (lesson 10: arg 1, RATCH) */
    case U_ACT: return ui.act == a->arg;
    case U_STEPTIME: return TSEL->step[ui.cursor].time == a->arg;
    }
    return 0;
}

static int direction(const act_t *a)                   /* which way to turn to get there */
{
    int16_t *v;
    if (a->until == U_VALUE && (v = knob_value((uint32_t)(a->ctl - C_K1))))
        return *v < a->arg ? 1 : -1;
    if (a->until == U_TRACK)
        return song.sel < a->arg ? 1 : -1;
    if (a->until == U_EVCHANCE)                        /* (+ ADD: CHANCE RATCH NUDGE come before the parameters) */
        return -1;
    if (a->until == U_STEPTIME)                        /* (NOTE TIE REST) */
        return TSEL->step[ui.cursor].time < a->arg ? 1 : -1;
    if (a->until == U_PICK)
        return ui.ppick < a->arg ? 1 : -1;
    if (a->until == U_ROW)                             /* (the menu keeps a tab's last row: either way) */
        return ui.menu_sel < a->arg ? 1 : -1;
    return 1;
}

static void stuck(uint32_t step, const act_t *a)
{
    fprintf(stderr, "step %u: \"%s\" never got there (page %s, track %u, cursor %u, LEN %d:", step, a->say,
            ui.home ? "HOME" : PAGES[ui.page].title, song.sel + 1u, ui.cursor, TSEL->p[P_SLEN]);
    for (uint32_t i = 0; i < 16u; i++)
        fprintf(stderr, " %c%u", "NTR"[TSEL->step[i].time % 3u], TSEL->step[i].n ? TSEL->step[i].note[0] : 0u);
    fprintf(stderr, ")\n");
    exit(1);
}

static void act(FILE *log, uint32_t step, const char *title, const act_t *a)
{
    float t0 = now / (float)FS, dur = a->dur > 0 ? a->dur : 3.5f, spent;
    uint32_t n = 0;
    int way = 0;                                       /* the turn's first click: 1 clockwise (up), -1 counter */
    uint32_t used = a->ctl;                            /* the control the hand used */
    char cname[16];
    run(0.6f);                                         /* the sword arrives, then the hand moves */
    spent = 0.6f;
    if ((a->kind == K_TAP && a->until == U_CLICKS) || a->kind == K_HOLD) {
        float held = a->kind == K_HOLD ? 1.0f : 0.15f;
        btn(a->ctl, 1); run(held); btn(a->ctl, 0); spent += held;
    } else if (a->kind == K_TAP || (a->kind == K_TURN && (a->ctl == C_OCTUP || a->ctl == C_OCTDN))) {   /* tap until it shows */
        while (!reached(a)) {
            uint32_t b = a->until == U_SCENE && ui_scene > a->arg ? C_OCTDN : a->ctl;   /* (the menu: either way) */
            if (n++ > 12u)
                stuck(step, a);
            used = b;
            btn(b, 1); run(0.15f); btn(b, 0); run(0.7f); spent += 0.85f;
        }
    } else if (a->kind == K_KEY) {                     /* a key held, as a player would */
        float held = dur - 0.6f - 0.05f;
        fm1_in.notes |= 1u << a->arg;
        host_notes |= 1u << a->arg;                    /* (its press, as the UI sees one: DREAM's keys) */
        run(held);
        fm1_in.notes &= ~(1u << a->arg);
        spent += held;
    } else if (a->kind == K_CHORD) {                   /* (lesson 7) three keys held together */
        uint32_t m = 1u << (a->arg & 31) | 1u << (a->arg >> 5 & 31) | 1u << (a->arg >> 10 & 31);
        float held = dur - 0.6f - 0.05f;
        fm1_in.notes |= m;
        host_notes |= m;
        run(held);
        fm1_in.notes &= ~m;
        spent += held;
    } else if (a->kind == K_CRANK) {                   /* (lesson 12) the crank: SELECT's clicks, steady */
        float gap = 1.0f / a->arg, left_s = dur - 0.6f - 0.05f;
        way = 1;
        for (; left_s >= gap; left_s -= gap) {
            host_enc[panel.enc[EN_SELECT]] += panel.dir[EN_SELECT];
            fm1_in.enc_dfr[panel.enc[EN_SELECT]] = fm1_in.frames;
            run(gap);
            spent += gap;
        }
    } else if (a->kind == K_PRESS || a->kind == K_LETGO) {   /* (lesson 11) the left hand: down and kept, then up */
        if (a->kind == K_PRESS) {
            left = 1u << (a->arg & 31) | 1u << (a->arg >> 5 & 31) | 1u << (a->arg >> 10 & 31);
            fm1_in.notes |= left;
            host_notes |= left;
        } else {
            fm1_in.notes &= ~left;
            left = 0;
        }
    } else if (a->kind == K_GLOKEY) {                  /* GLO held for its layer, a black key: a track's mute (or latch) */
        uint32_t tr = (a->until & 15u) - 1u, latch = a->until & 0x10u;
        int16_t was = latch ? lt[tr].on : trk[tr].p[P_MUTE];
        btn(C_GLO, 1); run(0.9f);
        fm1_in.notes |= 1u << a->arg; run(0.2f); fm1_in.notes &= ~(1u << a->arg); run(0.4f);
        btn(C_GLO, 0);
        spent += 1.5f;
        if ((latch ? lt[tr].on : trk[tr].p[P_MUTE]) == was)
            stuck(step, a);
    } else if (a->kind == K_TURN && a->until == U_CLICKS) {   /* (lesson 6) a few clicks, as many as arg */
        uint32_t e = (uint32_t)(a->ctl - C_SELECT + EN_SELECT), m = (uint32_t)(a->arg > 0 ? a->arg : -a->arg);
        if (a->ctl >= C_K1)
            lit_slot = a->ctl - C_K1;
        way = a->arg > 0 ? 1 : -1;
        for (; m; m--) {
            host_enc[panel.enc[e]] += way * panel.dir[e];
            run(0.4f);
            spent += 0.4f;
        }
    } else if (a->kind == K_TURN) {
        uint32_t e = (uint32_t)(a->ctl - C_SELECT + EN_SELECT);
        float click;                                   /* a value sweeps; a list steps, the first steps seen */
        if (a->ctl >= C_K1)
            lit_slot = a->ctl - C_K1;
        while (!reached(a)) {
            if (n++ > 400u)
                stuck(step, a);
            click = a->until == U_VALUE ? 0.025f : n <= 3u ? 0.4f : 0.08f;
            int d = direction(a);                      /* (a fast sweep may overshoot by a click and come back) */
            if (!way)
                way = d;
            host_enc[panel.enc[e]] += d * panel.dir[e];
            run(click);
            spent += click;
        }
    }
    if (a->ctl == C_POWER) {                           /* off, a breath, on: as at power-on (RESTORE LAST OFF: the scene
                                                        * chosen; ON: the last session, as it was) */
        run(0.6f);
        dark = 1;
        run(1.4f);
        if (ui_prefs & PREF_RESTORE_OFF)
            gb_scene_load(ui_scene);
        rev_clear();
        hall_clear();
        dark = 0;
        spent += 2.0f;
    }
    if (spent < dur)
        run(dur - spent);
    lit_slot = -1;
    if (a->kind == K_CHORD || a->kind == K_PRESS)
        snprintf(cname, sizeof cname, "KEY%d+%d+%d", a->arg & 31, a->arg >> 5 & 31, a->arg >> 10 & 31);
    else if (a->ctl == C_KEY)
        snprintf(cname, sizeof cname, "%sKEY%d", a->kind == K_GLOKEY ? "GLO+" : "", a->arg);
    else
        snprintf(cname, sizeof cname, "%s", CNAME[used]);
    fprintf(log, "%.3f\t%.3f\t%s\t%s\t%u\t%s\t%s\n", t0, now / (float)FS, cname,
            a->kind == K_TAP ? "tap" : a->kind == K_HOLD ? "hold" : a->kind == K_KEY || a->kind == K_CHORD ? "key" : a->kind == K_PRESS ? "press" : a->kind == K_LETGO ? "letgo" :
            a->kind == K_GLOKEY ? "glokey" : a->kind == K_CRANK ? "turn+" : a->kind != K_TURN ? "say" :
            a->ctl == C_OCTUP || a->ctl == C_OCTDN ? "tap" : way > 0 ? "turn+" : way < 0 ? "turn-" : "turn0", step, title,
            a->say);
}

int main(int argc, char **argv)
{
    uint32_t n = argc > 1 ? (uint32_t)atoi(argv[1]) : 1u, s, i, total;
    const char *path = argc > 2 ? argv[2] : "lesson.wav";
    const lesson_t *L;
    FILE *log;
    char lpath[600];
    if (n < 1u || n > sizeof LESSONS / sizeof LESSONS[0])
        return 2;
    L = &LESSONS[n - 1u];
    vid = argc > 3 ? fopen(argv[3], "wb") : 0;
    rep = fopen("/dev/null", "w");                     /* (ui_render's lint hooks report here) */
    ui_power_on();
    pal(UI_CRYPT_INDEX);
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    song.batt_raw = 600;
    usb.config = 0;
    ui_scene = L->scene;                               /* the FM-1 as the lesson before left it, stopped */
    if (n > 1u)                                        /* (lesson 1 set RESTORE LAST OFF; a 2.1 FM-1 starts ON) */
        ui_prefs |= PREF_RESTORE_OFF;
    gb_scene_load(ui_scene);
    rev_clear();
    hall_clear();
    snprintf(lpath, sizeof lpath, "%s.acts", path);
    if (!(wav = fopen(path, "wb")) || !(log = fopen(lpath, "w")))
        return 1;
    wav_hdr(wav, 0);                                   /* (the length is written at the end) */
    run(1.5f);
    for (s = 0; L->steps[s].title; s++)
        for (i = 0; L->steps[s].acts[i].say; i++) {
            const act_t *a = &L->steps[s].acts[i];
            act(log, s + 1u, L->steps[s].title, a);
        }
    run(L->tail);
    fclose(log);
    total = now;
    fseek(wav, 0, SEEK_SET);
    wav_hdr(wav, total);
    fclose(wav);
    if (vid)
        fclose(vid);
    printf("%s: %.1f s\n", path, total / (double)FS);
    return 0;
}
