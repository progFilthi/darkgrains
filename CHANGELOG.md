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

No AAX, cloud, license-server, or out-of-scope features have been added.
