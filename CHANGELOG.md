# Changelog

## Milestone 1 — Project foundation

- Added a CMake + JUCE 9 / C++20 project targeting VST3 on Windows and VST3 + AU on macOS.
- Added the complete DarkGrain APVTS parameter tree with stable IDs, ranges, defaults, units, and skew.
- Added a stereo passthrough processor with smoothed input gain, output gain, and click-free host bypass.
- Added APVTS session state save/restore.
- Added a lightweight dark native editor for the Milestone 1 controls and stereo level metering.
- Added unit tests for parameter definitions, state serialization, passthrough, gain, bypass, sample rates,
  block sizes, silence, DC offset, full-scale input, and repeated block processing.
- Added CMake presets for macOS and Windows Debug/Release builds, warning-as-error support, and a
  standalone development target.

### Verification

- Release and Debug builds complete with warnings treated as errors on macOS (Apple Clang, Ninja).
- Unit tests pass in both configurations (`100% tests passed`).
- AU validated with `auval -v aufx Dgrn Pfil` (`AU VALIDATION SUCCEEDED.`).
- VST3 validated with `pluginval` at strictness 6 and 10 (`SUCCESS`).
- A clean configure/build using JUCE 9.0.5 fetched through `FetchContent` (no local JUCE path) also
  builds and passes the unit tests.

## Milestone 2 — Dark reverb

- Added an 8-line feedback delay network reverb with mutually prime delay lengths.
- Added pre-delay (0–500 ms), reverb size (0.1–10 s), damping, diffusion, modulation rate/depth,
  stereo width, and wet/dry mixing.
- Added input diffusion all-pass filters, low-cut, freeze, and a stable feedback ceiling.
- Extended the processor to run the reverb chain with smoothed mix and width controls.
- Added reverb stability, tail, and automation tests.

## Milestone 3 — Grain engine

- Added a 4-second stereo circular input buffer with fractional reads and safe wraparound.
- Added an overlap-add granular engine with Hann-windowed grains, 10–500 ms grain sizes,
  1–64 grains/s density, position spread, ±24 st pitch, pitch scatter, reverse grains,
  stereo scatter, and freeze.
- Added click-free freeze behaviour that stops the write head and recycles the held material.
- Added grain buffer, wraparound, freeze, and extreme-setting stability tests.

## Milestone 4 — Particles and final tone

- Added a particle engine with density, size, brightness, pitch, pan width, randomness,
  feedback, and mix controls.
- Particles combine short band-passed grains, filtered noise, randomized pitch/pan/envelopes,
  and a bounded feedback delay.
- Added tilt-style dark tone shaping, musical saturation, mid/side stereo width, and an
  output safety limiter.
- Added the five particle controls required by the brief (size, pitch, width, feedback, mix),
  bringing the parameter tree to 31 parameters.
- Added full-chain automation tests at multiple sample rates and block sizes.

## Milestone 5 — UI and polish

- Added the complete dark native interface with INPUT, GRAINS, PARTICLES, SPACE, and OUTPUT panels.
- Added custom rotary knobs, panel styling, freeze indicator, host bypass, and an IN/OUT level meter.
- Added a preset bar with six factory presets (Init, Cinematic Dark, Frozen Choir, Granular Storm,
  Dust Particles, Deep Space) and user preset save/load as XML in
  `~/Library/Application Support/DarkGrain/Presets`.
- Added editor construction, painting, factory preset, rapid preset switching, and
  sample-rate-change tests.
- Added a signal flow that keeps the dry path clean and phase-coherent while the wet chain
  runs grains, particles, reverb, tone, saturation, width, and limiting.
- Final Release build validated with `pluginval` at strictness 6 and 10 (`SUCCESS`).

No AAX, cloud, license-server, or out-of-scope features have been added.

## Milestone 6 — Perform view and musical key awareness

- Redesigned the editor around a single perform view: one live ember field and two macro knobs,
  `GRAINS` and `SPACE`, instead of exposing the full control set by default.
  `GRAINS` drives grain density, size, spread, stereo width, reverse amount, particle density and
  mix, and dry/wet. `SPACE` drives reverb mix, size, diffusion, damping, pre-delay and stereo width.
- Moved the detailed INPUT, GRAINS, PARTICLES, SPACE, and OUTPUT controls behind an
  `ADVANCED` toggle that returns to the perform view on `PERFORM`.
- Added an animated dark background: warm orange particles drift upward and gain density, size,
  brightness and speed as the grain/particle macros rise, with a soft centre glow that pulses with
  the output level. Freezing the buffer turns the field cold blue.
- Added `Root Key` and `Scale` selectors to the perform view, defaulting to C minor. The subtitle
  shows the active key and scale.
- Added scale quantisation for generated grain and particle pitches (chromatic, natural minor,
  major, dorian, phrygian, lydian, mixolydian, harmonic minor, and both pentatonics), so pitch
  offsets snap onto the selected scale and textures stay in key.
- Changed the grain pitch-scatter default from 3 st to 0 st and the particle pitch default from
  -7 st to 0 st, so newly added instances are unpitched and in key until the producer moves a
  pitch control.
- Preset loading now keeps the project key and scale instead of resetting them, and the macro
  knobs resynchronise after preset loads.
- The parameter tree now holds 33 parameters, and the footer version reads from the build version.
- Added scale-quantiser unit tests and extended the parameter and editor tests.
