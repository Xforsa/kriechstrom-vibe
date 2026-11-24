# KRIECHSTROM for VIBE FM

by Grigorii Bezborodov

Dark industrial and EBM groovebox for VIBE FM: a native Vibe, built with the VIBE FM SDK. It is the port of
[KRIECHSTROM](https://github.com/Xforsa/kriechstrom) for the M-VAVE FM-1, which is built on
[Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton Instruments), with parts of
[Glitchbox](https://github.com/erikdevos/glitchbox) by erikdevos. The sound engine is the FM-1 firmware's own code;
the screen, the controls, the pedalboards, the sampler, the audio input, MIDI OUT and the stage are new.

![KRIECHSTROM on the VIBE FM display](docs/sound.png)

Site: https://xforsa.github.io/kriechstrom-vibe/ · manual: https://xforsa.github.io/kriechstrom-vibe/manual/en.html ·
for the FM-1: https://xforsa.github.io/kriechstrom/

Kriechstrom is German for creepage current: the leakage across worn insulation that leaves a branching mark. Here that
mark grows behind every page as the master wears the mix out.

## What it does

- **Four tracks, KRIECHSTROM's sound:** the Felucca engines and GLCH, FM6, PHYS, DRUM with KRIECHSTROM's kits, 171
  sounds, a 64-step sequencer per track (locks, conditions, ratchets, chords, the arpeggiator), DICE in eight moods,
  OOPS, the per-track pedals (INFECT with VOX among them) and the master chain: DAMAGE, WALL, RESONATE, SPLICE, HAUNT,
  HISS. At 44.1 kHz its output is the FM-1 core's, bit for bit (tests/run.sh checks it); at other rates it is
  resampled.
- **The 480 x 320 display:** pages for SOUND, ENV, LFO, ARP, FX and SEQ, the four values on encoders 1-4, the four
  tracks with their levels, a piano roll or a drum grid of the 16 steps on the pads, the whole pattern above it;
  FM6's algorithm as operators and wires, needle meters on the mixer, tape reels on the master, glyphs on the drum
  lanes. HOME shows what every control does on the page you are on.
- **Pedalboards:** three pedals a track (the first is the project's INSERT, as on the FM-1, two more after it), three
  on the input, two on the whole mix; any of KRIECHSTROM's thirteen, run by the FM-1's own code. Each pedal shows on
  its window what it does to a wave, live; DATA, F5 or a click picks the one the knobs turn. The extra pedals are kept
  in `pedals.kxs`.
- **The sampler:** the input or the mix, recorded for 1, 2 or 4 bars from the next bar into the user slots USR1-3
  (the FM-1's format, IMA ADPCM); a track plays it as SLICE, cut where its hits are, or as SAMPLE across the keys.
  Kept in `usr1.kxu` .. `usr3.kxu`.
- **The start:** where you left it, the demo song, an empty project, or one of your projects.
- **The display wears with the sound:** the creep figure grows with DAMAGE and WALL; on every drum hit a discharge
  grows behind the page as the site's pictures are grown (the KICK from the floor, SNARE and CLAP from the side, the
  hats from the top; DAMAGE, WALL and HISS shape it; MENU > DISPLAY > LIGHTNING switches it off); DAMAGE
  tears the picture on the drum hits, CHAR towards DIGITAL splits its colours, SPLICE cuts bands of it, HAUNT's DRAG
  sags it over the last beat of a bar, HISS adds grain. Each effect has a picture of its own, moving with the
  sound: DAMAGE a tape bitten full of holes (square ones as CHAR goes DIGITAL), WALL bricks laid, RESONATE four
  strings in the chord, SPLICE the sixteenths glued back wrong, HAUNT the swell or the brake, HISS grain; the delay its
  echoes, the reverb a room, a spring or a hall, the slicer its pattern, the clock its beats. MENU > DISPLAY sets how much the screen wears: OFF, LOW (the default), FULL.
  The pages slide, a value turned lights up, and the pages move with the sound: the spectrum, the envelope and the
  LFO where they are, the playhead's trail.
- **SPINE (SEQ > SPINE):** names the key (by the notes, or as set) and walks a progression of up to eight chords; the
  tracks that follow it move to each chord by their TRN, a step each 1, 2 or 4 bars; DICE writes a progression in the
  MOOD, the keys move the followers live.
- **The pads:** mute and DICE per track; pads 9-16 hold effects, at first eight of this firmware's own (PRESS, SHRED,
  BLACKOUT, SHORTWAVE, CORRODE, HAMMER, VOID, STROBE, each with its picture on the display and the stage), with SHIFT
  eight more, each of the sixteen chosen
  from eighteen on MENU > PAD FX; on the SEQ page the pads are the 16 steps shown, lit as they play. A controller with an 8 x 8 grid (a
  Launchpad) has all 64 steps. The steps can be clicked on the display.
- **A foot pedal (MENU > FOOT PEDAL):** the expression pedal turns DAMAGE, WALL or the track's level (the page shows
  the pedal and a gauge of what it turns); the footswitch plays, DICEs, stops the tape or freezes. Without a pedal both
  work with the mouse on that page.
- **The audio input (IN 1, IN 2):** a guitar, a voice or a drum machine into the mix and through the whole master,
  with DRIVE, sends to the delay, reverb and chorus, a GATE opened by a track's notes, and as a track's stand-in for
  INFECT (a voice through the VOX vocoder on a pad, a guitar ducking the bass).
- **MIDI OUT:** every track's notes on its own channel (the sequencer, the arpeggiator, DICE's parts, the keys),
  24 PPQ clock, START and STOP.
- **CV and GATE:** one track's notes on CV OUT (1 V per octave) and GATE OUT.
- **The stage (1280 x 720):** the four tracks as four rings of steps round the centre, a hand sweeping each; in the
  middle the figure the drums grow inside the master's sound as a ring. The tracks on the left, the master's chain on
  the right, the pads' effects below. Steps, tracks, mutes and knobs can be clicked, dragged or turned with the wheel,
  the effects held with the mouse or several fingers; each track has DICE, mute and its level as a fader, the tempo
  drags, the MOOD clicks on; each also has its control on the device.
- **The DAW:** each track's mixer (the SDK's `mix_<n>_*`), its sound, envelope and sends, the master's effects as
  parameters; its transport and tempo; projects and settings stored with the DAW project. Under VIBE FM Link the
  controller shows the page's name and its four values.
- **A demo song:** the first start holds a song in four slots (E minor, 100 BPM): PLAY plays its theme, SEQ > SONG
  and F5 the whole song, MENU > PARTS A-D and SHIFT + ENTER bring it back.
- **Storage:** the music as it is now is kept by itself (`session.krp`), four project slots that SONG plays in a chain,
  32 user sounds. The files are KRIECHSTROM's FM-1 formats.
- **Projects (MENU > PROJECTS, or SHIFT + MENU, or the start's fourth choice):** 32 whole projects, each in one `.kxp` file: the music, the four slots, every
  pedalboard and the sampler's recordings. Named with ENC 2 and 3, saved with F5, loaded with ENTER twice. SHIFT + F5
  exports one as a file into `VIBE FM/kriechstrom/` on the host's writing place; MENU > PROJECT FILES imports them,
  and a `.kxp` dropped on the plugin's window is imported too.
- **More outputs (MENU > OUTPUTS):** when the DAW gives the plugin more outputs, each track goes out on its own pair
  (T1 on OUT 3/4 .. T4 on OUT 9/10, after its pedals, LEVEL and PAN), the shared effects' return on OUT 11/12 and the
  audio input on OUT 13/14, while OUT 1/2 stays the master. They are taken before the master's chain (DAMAGE, WALL and
  the rest work on the sum), so they add up to the mix before it. Record OUT 1/2 as the finished sound and the parts
  to add to it; for a clean multitrack to mix from scratch, turn the master's effects off before recording.

![The stage](docs/stage.png)

## Controls

| | |
|---|---|
| keys | play the selected track (its chord keys and arpeggiator) |
| pads 1-4 | mute a track (SHIFT: solo it; again: all back) |
| pads 5-8 | DICE: a new part for that track in the MOOD (SHIFT: OOPS on it); on MASTER: DAMAGE, WALL, SPLICE, HAUNT on and off; on a pedalboard: pedal 1-3 bypassed, 4 all |
| pads 9-16 | held: an effect each, chosen on MENU > PAD FX from 26 (at first PRESS, SHRED, BLACKOUT, SHORTWAVE, CORRODE, HAMMER, VOID, STROBE (SHIFT: 1/16, TAPE STOP, FREEZE, GATE, CRUSH SWEEP, OCT DOWN, FLANGER, RANDOM)) |
| SOUND ENV LFO ARP FX SEQ | the pages; the same key again: the next page of that family |
| ◀ ▶ | the family's pages |
| encoders 1-4 | the four values on the page |
| DATA | the page's own knob: on SOUND the sounds (SHIFT + DATA on any SOUND page: the engine); on a list the list; on SEQ the step; on PEDALS the pedal; on MIXER the selected track's level; elsewhere the card marked DATA (ENTER moves to the next) |
| A | the selected track's level |
| B | DAMAGE (SHIFT + B: WALL) |
| F1-F4 | select track 1-4 |
| F5 | the page's action: OOPS on SOUND, DICE on SEQ (SHIFT: MOOD), REC on SAMPLER, the next pedal on PEDALS, SAVE on PROJECTS (SHIFT: export) and PARTS A-D, the input's type on INPUT |
| F6 | UNDO |
| MENU | always its tiles: projects, project files, parts A-D, mixer, outputs, MIDI OUT and CV, foot pedal, display, pad FX, clock (SHIFT + MENU: straight to PROJECTS) |
| SHIFT | held or tapped (then it holds for the next control, a knob's whole turn); held still, the pads' second functions are shown |
| PLAY, STOP, REC | the transport; REC arms the selected track for live recording |
| ENTER | on SEQ: the cursor's step on or off; on PROJECTS: load (twice); the Playground's and the plugin's panels have no ENTER key: a click on the DATA knob is ENTER |
| HOME | what every control does on this page, and how its effect goes on and off (SHIFT + HOME: MENU, for the plugin's Rack view, which has no MENU key); RETURN (↵) goes back to the page before, step by step |
| the mouse (plugin, browser) | a click on what is shown: a tab, a track, a page's dot, a step, a pedal, a sound of the list, a master effect's tile, a slot, a project; a drag up or down in one of the four columns turns that column's knob (the host's layout) |

The full cheat sheet, page by page, is on the [site](https://xforsa.github.io/kriechstrom-vibe/#cheat).

## Play it

- In the browser: open the [VIBE FM Playground](https://play.thefirmwarehub.com/) and drop `kriechstrom.wasm` onto
  the device.
- In a DAW: open VIBE FM VST and drop `kriechstrom.zip` onto its window.
- Both files are on the [releases](https://github.com/Xforsa/kriechstrom-vibe/releases) page. PLAY starts the four
  tracks' patterns at once.

## Versions

KRIECHSTROM for VIBE FM follows KRIECHSTROM for the FM-1: its version X.Y is the FM-1 line its sound comes from (0.5
plays KRIECHSTROM 0.5.x), and a release that changes only the VIBE side counts up behind it (0.5.1, 0.5.2 ..). Each
release names the FM-1 version inside it; `core/kriechstrom` points at that commit. The VIBE is never ahead of the FM-1
in the sound: what is new in the sound comes to the FM-1 first, the VIBE takes it with the next X.Y.

## Build

Needs [Zig 0.13.0](https://ziglang.org/download/#release-0.13.0) (`zig` on the PATH or `ZIG=/path/to/zig`), Python 3
with Pillow and fontTools (the core's tables and fonts), a C compiler and Node.js 18+ for the tests.

```sh
git clone --recursive https://github.com/Xforsa/kriechstrom-vibe
cd kriechstrom-vibe
sh tools/build.sh          # build/kriechstrom.wasm and build/kriechstrom.zip
sh tests/run.sh            # the tests, vibe-check among them
```

## Layout

- `kriechstrom/` the firmware (its folder name is its id): `kriechstrom.c` builds everything as one unit, as the FM-1
  firmware does; `kx_core.h` the KRIECHSTROM sources, `kx_audio.h` the audio flow and the resampler, `kx_ctl.h` what
  the controls do in the core, `kx_input.h` the audio input, `kx_midi.h` MIDI OUT and CV, `kx_store.h` storage,
  `kx_daw.h` the DAW, `kx_pedals.h` the pedalboards, `kx_sampler.h` the sampler, `kx_proj.h` my projects and their files, `kx_outs.h`
  the outputs, `kx_ui.h` and `kx_draw.h` the
  pages, `kx_pics.h` and `kx_te.h` the pictures, `kx_live.h` the live parts of the pages, `kx_creep.h` and
  `kx_wear.h` the creep and the display's wear, `kx_stage.h` the stage. `icon.png` is drawn by `tools/make_icon.py`.
- `core/kriechstrom/` KRIECHSTROM for the FM-1, a git submodule. Its sound and edit code is used as it is; the one
  changes for VIBE FM are hooks in `fx.c` (the audio input, the extra pedals and INFECT's sources for them, the
  tracks' and the effects' outputs), empty in the FM-1 build.
- `sdk/`, `tools/vibe-build.sh`, `tools/vibe-check.mjs`, `tools/wasmtest.mjs`, `tools/media-host.mjs` from the
  [VIBE FM SDK](https://github.com/Vibe-FM-SDK/vibe-fm-sdk) (Apache-2.0, `sdk/VERSION` names its commit);
  `tools/vibe-build.sh` adds the generated headers' folders, `tools/kx-test.mjs` is `wasmtest.mjs` that also saves the
  stage.
- `site/` the site (GitHub Pages, `tools/make_site.sh`), in English and Russian; its pictures are drawn by
  `tools/make_hero.py` with the FM-1 site's own `krst_art.py`.
- `tests/` the host tests: the core at 44.1 kHz against the FM-1 build, vibe-check, other rates, the input, MIDI OUT,
  storage, my projects (save, export, load, import), the outputs.

## Licence

GPL-3.0-only, as KRIECHSTROM and Felucca; see [LICENSE](LICENSE), [LICENSING.md](LICENSING.md) and
[CREDITS.md](CREDITS.md). The VIBE FM SDK files keep their Apache-2.0 licence (`sdk/LICENSE`, `sdk/NOTICE`).
"VIBE FM" is a trademark of Sven Trogus; this firmware is built with the VIBE FM SDK.
