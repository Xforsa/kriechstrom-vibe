# Changelog

All notable changes to KRIECHSTROM for VIBE FM. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [0.5.1] - 2026-11-20

On KRIECHSTROM 0.5.2 for the FM-1.

### Added
- every page has its number on a white plate before its title, and crop marks round its picture
- a page change cuts in like spliced tape for a moment (not with SCREEN WEAR OFF); the power-on picture too
- pictures for MOD (the four slots wired), ARP 2 (swing and chance) and CHORD (the chord on the keys)

### Fixed
- DETAIL showed the step's NOTE and TIME on its cards instead of CHANCE, RATCH, NUDGE and VEL; DATA now moves its step
- MOD's first card was empty: it is the slot ENC 1 picks
- saving a sound while playing said SAVE ERROR: it says STOP TO SAVE, as on the FM-1
- HOME on PROJECT and MY SOUNDS names ENC 1
- LANES 2 drew two of its four faders

## [0.5] - 2026-11-20

On KRIECHSTROM 0.5.2 for the FM-1.

### Added
- pedalboards: three pedals a track (the project's INSERT and two more), three on the input, two on the whole mix, any
  of KRIECHSTROM's thirteen; each shows on its window what it does to a wave; DATA, F5 or a click picks the pedal
- the sampler (SOUND > SAMPLER): the input or the mix, 1, 2 or 4 bars from the next bar, into USR1-3, played as SLICE
  cut at its hits or as SAMPLE; kept between sessions
- a foot pedal (MENU > FOOT PEDAL): the expression pedal turns DAMAGE, WALL or the level, the switch plays, DICEs,
  stops the tape or freezes; both work with the mouse on that page
- pictures of their own for the master's effects (a tape bitten full of holes, bricks, strings, cut sixteenths, the
  swell or the brake, grain), the delay, the reverb, the chorus, the slicer, the clock, the destinations, the drum
  lanes, the voices and the display's three wears; needle meters on the mixer, tape reels on the master, FM6's
  algorithm, glyphs on the drum lanes
- HOME shows what each control does on the page you are on
- clicks on the steps and the pedals; a Launchpad's 64 pads are all 64 steps; VIBE FM Link shows the page and the
  four knobs on the controller

### Changed
- brighter text throughout; the knobs' names on the pedals in full
- the core: KRIECHSTROM 0.5.2 (two more empty hooks in fx.c, for the pedals)

## [0.4] - 2026-11-19

### Added
- the start asks: where you left it, the demo song or an empty project (F1 F2 F3, or DATA and ENTER; the keys, pads
  and PLAY go on with what is there)
- SHIFT tapped holds for the next control (with a mouse in the browser or the plugin); while SHIFT is on, the pads'
  second functions are shown
- the pages slide from one to the next; a value turned lights its card; the SOUND page shows the master's spectrum,
  ENV and LFO where the newest note and the LFO are now, SEQ the playhead's trail, MASTER the signal along the chain
- MENU > DISPLAY: SCREEN WEAR OFF, LOW, FULL

### Changed
- the display's wear is LOW by default: a hint of the tears, the grain and the split colours (FULL as before)
- DICE says which track got the new part and in which MOOD

## [0.3] - 2026-11-19

### Added
- a demo song to start from: four project slots in E minor at 100 BPM (THEME, DRIVE, BREAK, WEIGHT, with ROOM, ROT,
  GATE and SLAB KIT) and a SONG over them; the first start plays it on PLAY, SEQ > SONG and F5 plays the whole song,
  MENU > PROJECT and SHIFT + ENTER brings it back (into the empty slots); made by tools/make_demo.c

## [0.2] - 2026-11-19

### Added
- SONG (the sections and their repeats, F5 plays the song), AUTOMATION (the locks and events as a list) and FM6's
  operator pages
- MY SOUNDS: 32 user sounds, saved from the selected track and loaded back (MENU's SAVE on the FM-1)
- the site, in English and Russian: https://xforsa.github.io/kriechstrom-vibe/

### Changed
- the creep is a discharge: jagged, forking into fine filaments, the strongest grown first
- the power-on picture shows the name only
- SAMPLE and SLICE sounds, which play recorded sound this firmware does not carry, are left out of the sound list
- the core is KRIECHSTROM's main, with the input's hook merged

## [0.1] - 2026-11-19

### Added
- KRIECHSTROM 0.5.1's sound and edit code as a native Vibe (one `.wasm`), bit for bit with the FM-1 at 44.1 kHz
- pages for SOUND, ENV, LFO, ARP, FX and SEQ, the mixer, the project slots, MIDI OUT and CV; FOCUS; HOME's overview
- the display's wear (the creep, tears, split colours, spliced bands, grain) and the power-on picture
- the pads, the audio input, MIDI OUT per track with clock, CV and GATE, the stage
- DAW parameters and transport, storage, tests and CI
