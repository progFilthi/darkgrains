# DarkGrain — Granular Texture & Dark Reverb Plugin

## Project brief

Build a professional, lightweight stereo audio effect plugin called **DarkGrain**.

DarkGrain transforms samples, loops, vocals, drums, and ambient material into dark,
textured, cinematic soundscapes. Its core sound combines:

- Granular “grain” processing
- Randomized dark particles
- A modulated feedback-delay-network (FDN) reverb
- Filtering, saturation, stereo widening, and dry/wet blending

The first release must target:

- **Windows:** VST3
- **macOS:** VST3 and AU (Audio Unit)

Do **not** implement AAX in the first milestone. Design the code so AAX can be added later.

## Technology decisions

Use the following stack exactly:

- Language: **C++20**
- Framework: **JUCE**
- Build system: **CMake**
- Plugin formats: **VST3** and **AU**
- Platforms: **Windows** and **macOS**
- UI: JUCE native GUI
- State: `juce::AudioProcessorValueTreeState` (APVTS)
- Testing: JUCE Unit Tests plus a small offline render test harness

Use CMake as the primary project system. JUCE’s CMake API supports plugin creation through
`juce_add_plugin`, which creates a shared-code target and separate targets for each selected
plugin format. [35][41]

## Product goal

Create a plugin that is:

- CPU-efficient enough for multiple instances in a project
- Stable at 44.1 kHz, 48 kHz, 88.2 kHz, and 96 kHz
- Safe with automation, preset changes, sample-rate changes, and bypass
- Usable as a stereo insert effect
- Intuitive for music producers without requiring deep technical knowledge

The plugin should sound dark, spacious, grainy, and cinematic, but must remain musical and
controllable.

## Non-goals

Do not build these in version 1.0:

- MIDI synthesis
- Sample library browsing
- Cloud accounts or license servers
- Preset marketplace
- Convolution reverb using large impulse-response files
- AAX support
- Mobile or web versions
- Machine-learning DSP

## Plugin type

Build a stereo audio effect plugin:

- Input channels: 2
- Output channels: 2
- Plugin category: audio effect
- Latency: 0 samples initially
- Tail: the reverb must continue naturally after input stops
- Bypass: smooth, click-free bypass
- Presets: save/load complete parameter state

## Signal flow

Implement this processing chain:

```text
Stereo input
   ↓
Input trim and high-pass filter
   ↓
Granular texture engine
   ↓
Particle generator
   ↓
Dark FDN reverb
   ↓
Tone shaping and saturation
   ↓
Stereo width
   ↓
Output limiter / safety clipper
   ↓
Dry/wet mix
   ↓
Stereo output
```

The dry signal must remain clean and phase-coherent. Process wet effects in parallel, then blend
with dry using equal-power or linear crossfade logic.

## Audio engine requirements

### Real-time safety

The real-time audio callback must never:

- Allocate memory with `new`, `malloc`, `std::vector` growth, or `std::string`
- Read or write files
- Lock a mutex
- Use logging
- Call GUI code
- Perform unbounded loops
- Trigger dynamic library loading

Preallocate all delay lines, grain buffers, noise buffers, work buffers, and UI-to-DSP
communication structures during `prepareToPlay()`.

Use lock-free communication between the GUI and audio thread. Parameter values may be read
atomically, but complex commands should use a lock-free FIFO.

### Parameter smoothing

Smooth all audible parameters to prevent zipper noise and clicks:

- Gain, mix, width, filter cutoff, reverb size, damping
- Grain density, size, pitch, position spread
- Any parameter that directly changes delay time or feedback

Use per-block or per-sample smoothing where appropriate. Delay-time changes must use fractional
delay interpolation, never abrupt buffer jumps.

## Granular texture engine

Build a custom granular engine using a circular input buffer.

### Input buffer

- Store at least 4 seconds of stereo input at the current sample rate
- Use a preallocated circular buffer
- Support fractional read positions
- Handle buffer wraparound safely
- Clear or fade buffers appropriately when sample rate or block size changes

### Grain engine

Each grain must have:

- Start position in the input buffer
- Duration / grain size
- Playback rate
- Pitch transpose
- Pan position
- Amplitude envelope
- Reverse-playback option
- Stereo spread
- Random position offset
- Random pitch offset
- Random amplitude variation

Grain size range:

- 10 ms to 500 ms

Density range:

- 1 to 64 grains per second

Pitch range:

- -24 to +24 semitones

Position spread:

- 0% to 100% of available buffer history

The engine must support:

- **Freeze:** repeatedly read from a held section of the buffer
- **Reverse grains:** randomly or deterministically reverse selected grains
- **Pitch scatter:** random pitch variation around a central transpose value
- **Stereo scatter:** independent left/right grain placement
- **Grain envelope:** smooth attack and release to avoid clicks

Use overlap-add with windowed grains. Start with Hann, triangular, or raised-cosine windows.

## Particle generator

“Particles” are short, randomized textural events derived from the input and/or filtered noise.

Each particle event may include:

- A short grain from the input buffer
- Filtered noise
- A downward pitch shift
- A random pan position
- A short amplitude envelope
- A band-passed resonant filter
- Optional feedback into the reverb

The particle engine must have controls for:

- Density
- Size
- Brightness
- Pitch range
- Pan width
- Randomness
- Feedback amount
- Mix level

Particles should feel organic, not like obvious random noise bursts. Use correlated randomness,
envelopes, filtering, and stereo placement.

## Dark FDN reverb

Implement a feedback delay network reverb rather than a convolution reverb.

### Requirements

- Pre-delay: 0–500 ms
- Reverb size: 0.1–10 seconds
- Damping: high-frequency roll-off
- Diffusion: early-reflection-like smearing
- Modulation depth and rate
- Stereo width
- Low-cut and high-cut controls
- Wet/dry mix
- Freeze mode: infinite or near-infinite sustain, with safety gain reduction

### Suggested architecture

Use at least 8 delay lines with mutually prime or carefully chosen delay lengths.

Each delay line should include:

- Fractional delay read/write
- Low-pass damping filter
- Feedback gain
- All-pass diffusion
- Stereo decorrelation

Apply slow LFO modulation to one or more delay lengths to reduce metallic resonance.

The reverb must remain stable at maximum feedback. Implement internal safety limiting or feedback
attenuation so extreme settings do not produce runaway gain.

## Tone and output section

Add:

- Input high-pass filter: 20 Hz–1 kHz
- Input low-pass filter: 1 kHz–20 kHz
- Dark tilt / low-shelf and high-shelf controls
- Optional saturation stage
- Stereo width control
- Output gain
- Soft-clip safety limiter
- Dry/wet mix

The saturation stage should be subtle and musical. Do not make distortion the primary character
unless the user explicitly enables it.

## Parameters

Create these parameters with sensible ranges, defaults, skew, and units.

| Parameter | Range | Default | Notes |
|---|---:|---:|---|
| Input Gain | -24 to +24 dB | 0 dB | Pre-effects input level |
| High-Pass | 20 Hz–1 kHz | 40 Hz | Removes rumble |
| Grain Size | 10–500 ms | 90 ms | Length of each grain |
| Grain Density | 1–64 grains/sec | 12 | Grain events per second |
| Grain Spread | 0–100% | 35% | Random position spread |
| Grain Pitch | -24 to +24 semitones | 0 st | Central pitch shift |
| Pitch Scatter | 0–24 semitones | 3 st | Random pitch variation |
| Reverse Amount | 0–100% | 20% | Probability of reversed grains |
| Stereo Spread | 0–100% | 45% | Grain/texture width |
| Freeze | Off/On | Off | Holds and recycles texture |
| Particle Density | 0–100% | 35% | Number of particle events |
| Particle Brightness | 0–100% | 40% | Filter/color character |
| Particle Randomness | 0–100% | 50% | Pitch, timing, pan variation |
| Reverb Size | 0.1–10 sec | 2.5 sec | FDN decay time |
| Reverb Damping | 0–100% | 55% | High-frequency absorption |
| Diffusion | 0–100% | 60% | Smearing / early texture |
| Modulation Rate | 0.01–2 Hz | 0.15 Hz | Reverb movement |
| Modulation Depth | 0–100% | 25% | Delay modulation amount |
| Pre-Delay | 0–500 ms | 35 ms | Space before reverb |
| Reverb Mix | 0–100% | 35% | Wet level into final blend |
| Tone Dark | 0–100% | 45% | Global darkness / filtering |
| Saturation | 0–100% | 15% | Optional harmonic drive |
| Width | 0–200% | 120% | Stereo image width |
| Output Gain | -24 to +12 dB | 0 dB | Final output level |
| Dry/Wet | 0–100% | 50% | Final effect blend |

Use meaningful parameter IDs, stable names, units, and automation-friendly ranges.

## State and presets

Use `juce::AudioProcessorValueTreeState` for:

- Parameter creation
- Host automation
- Undo/redo where practical
- Preset state serialization
- DAW session recall

The plugin must save and restore all parameters correctly across:

- DAW session reload
- Plugin instance duplication
- Preset switching
- Automation playback
- Sample-rate changes

JUCE’s APVTS tutorial is the reference implementation for state persistence and parameter-backed
processing. [34]

## UI requirements

Build a clean, dark, producer-focused interface.

### Layout

Use a single non-resizable or lightly resizable window initially.

Suggested layout:

```text
+------------------------------------------------------+
|  DARKGRAIN                          Preset: Init     |
+------------------------------------------------------+
|  INPUT        |  GRAINS       |  PARTICLES           |
|  Gain         |  Size         |  Density             |
|  HP Filter    |  Density      |  Brightness          |
|               |  Spread       |  Randomness          |
|               |  Pitch        |  Width               |
|               |  Reverse      |                      |
|               |  Freeze       |                      |
+------------------------------------------------------+
|  SPACE                          |  OUTPUT            |
|  Size                           |  Tone Dark         |
|  Damping                        |  Saturation        |
|  Diffusion                      |  Width             |
|  Mod Rate                       |  Output            |
|  Mod Depth                      |  Dry/Wet           |
|  Pre-Delay                      |                    |
+------------------------------------------------------+
```

### UI behavior

- All controls must update smoothly and be automatable
- Parameter changes must not cause clicks
- Freeze should have a clear visual indicator
- Show a simple stereo level meter if inexpensive to implement
- Avoid heavy images, animations, or web views
- Keep the initial UI lightweight and responsive

## File structure

Use this project structure:

```text
darkgrain/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── .gitignore
├── cmake/
│   └── CompilerWarnings.cmake
├── source/
│   ├── PluginProcessor.h
│   ├── PluginProcessor.cpp
│   ├── PluginEditor.h
│   ├── PluginEditor.cpp
│   ├── dsp/
│   │   ├── GrainEngine.h
│   │   ├── GrainEngine.cpp
│   │   ├── ParticleEngine.h
│   │   ├── ParticleEngine.cpp
│   │   ├── DarkReverb.h
│   │   ├── DarkReverb.cpp
│   │   ├── Filter.h
│   │   ├── Filter.cpp
│   │   ├── StereoWidth.h
│   │   ├── StereoWidth.cpp
│   │   ├── Saturation.h
│   │   ├── Saturation.cpp
│   │   ├── Limiter.h
│   │   └── Limiter.cpp
│   ├── parameters/
│   │   ├── ParameterIDs.h
│   │   ├── ParameterLayout.h
│   │   └── ParameterLayout.cpp
│   └── state/
│       ├── PresetManager.h
│       └── PresetManager.cpp
├── tests/
│   ├── TestRunner.cpp
│   ├── GrainEngineTests.cpp
│   ├── ReverbTests.cpp
│   └── ParameterTests.cpp
└── resources/
    └── fonts/
```

## CMake requirements

The agent must create a CMake-based project that:

- Uses JUCE through `add_subdirectory(JUCE)` or an equivalent supported dependency method
- Calls `juce_add_plugin()` correctly
- Builds VST3 on Windows
- Builds VST3 and AU on macOS
- Produces a standalone app for development/testing
- Supports separate Debug and Release configurations
- Avoids MinGW; use MSVC on Windows and Apple Clang/Xcode on macOS

JUCE documentation specifies that `juce_add_plugin` creates a shared-code target plus format-specific
targets. [35][41]

### Initial CMake target

Use a target name such as:

```cmake
juce_add_plugin(DarkGrain
    COMPANY_NAME "Prog Filthi"
    PLUGIN_MANUFACTURER_CODE Pfil
    PLUGIN_CODE Dgrn
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "DarkGrain"
)
```

Do not add `AAX` until an Avid developer setup is available.

## Development milestones

### Milestone 1 — Project foundation

Deliverables:

- CMake project builds on Windows and macOS
- VST3 and AU targets compile
- Plugin loads in Reaper, Ableton Live, and Logic Pro
- Stereo passthrough works
- Bypass works without clicks
- Basic input/output gain works
- APVTS parameter infrastructure exists

Acceptance criteria:

- No compiler warnings in CI or local release build
- Plugin loads and processes stereo audio
- Parameter changes do not crash the host
- State saves and reloads correctly

### Milestone 2 — Dark reverb

Deliverables:

- FDN reverb engine
- Pre-delay
- Damping
- Diffusion
- Modulation
- Stereo width
- Wet/dry mix
- Stable feedback behavior

Acceptance criteria:

- No denormals, clicks, or runaway feedback
- Smooth parameter automation
- Reverb tail continues after input stops
- CPU use remains acceptable with multiple instances

### Milestone 3 — Grain engine

Deliverables:

- Circular input buffer
- Grain scheduling
- Overlap-add grain rendering
- Pitch, position, size, density, reverse, and stereo controls
- Freeze mode
- Click-free parameter changes

Acceptance criteria:

- No buffer overruns or underruns
- No audible zipper noise during automation
- Freeze works predictably
- Grain engine sounds musical at default settings

### Milestone 4 — Particles and final tone

Deliverables:

- Particle generator
- Filtered noise/textural events
- Randomized pitch, pan, and envelope behavior
- Tone shaping
- Saturation
- Stereo width
- Output limiter
- Complete parameter set

Acceptance criteria:

- All controls affect audio audibly and predictably
- No parameter causes instability
- Plugin does not clip excessively at default settings
- Presets sound distinct and usable

### Milestone 5 — UI and polish

Deliverables:

- Complete dark-themed GUI
- Preset system
- Smooth automation
- Error-free host testing
- Installer or packaged plugin artifacts

Acceptance criteria:

- UI remains responsive during automation
- No GUI/audio-thread race conditions
- Plugin passes basic stress testing
- Ready for private beta testing

## Testing requirements

The agent must test:

- 44.1 kHz, 48 kHz, 88.2 kHz, and 96 kHz sample rates
- Block sizes including 32, 64, 128, 256, 512, and 1024 samples
- Mono-compatible stereo behavior
- Automation of every parameter
- Rapid preset switching
- Bypass on/off
- Sample-rate changes during playback
- Extreme feedback and freeze settings
- Silence, DC offset, full-scale input, and clipped input
- Long sessions for memory stability

Create automated tests for:

- Grain buffer bounds
- Circular-buffer wraparound
- Reverb stability
- Parameter smoothing
- Bypass behavior
- State serialization
- No allocation in `processBlock()`

## Coding standards

- Use modern C++20
- Prefer `std::array`, `std::span`, and fixed-size buffers in the audio path
- Avoid exceptions in the real-time thread
- Avoid `std::vector`, `std::string`, heap allocation, locks, and file I/O in `processBlock()`
- Use clear names: `grainEngine`, `reverbEngine`, `particleEngine`
- Keep DSP classes independent from JUCE GUI classes where possible
- Separate DSP from UI
- Use `juce::SmoothedValue` or equivalent smoothing where appropriate
- Add comments only where the DSP intent is not obvious
- Every public DSP class must have a concise header comment explaining its purpose

## Initial default sound

The default preset should immediately sound useful on a vocal loop, pad, or ambient texture:

- Moderate grain density
- Slightly dark filtering
- Medium reverb size
- Noticeable but not extreme modulation
- Controlled stereo width
- No clipping
- No excessive noise
- No obvious metallic resonance

The first impression should be: **“This turns a normal sample into a dark, evolving cinematic texture.”**

## Agent working rules

1. Start with the CMake/JUCE project skeleton.
2. Implement audio passthrough before adding effects.
3. Implement the reverb before the grain engine.
4. Keep every DSP module independently testable.
5. Do not add features outside this README without asking.
6. If a design decision is ambiguous, choose the simpler, more robust option.
7. Never sacrifice real-time safety for convenience.
8. Maintain compatibility with both Windows and macOS from the first commit.
9. Keep the plugin lightweight: no unnecessary dependencies, frameworks, or large assets.
10. Provide a short changelog after each meaningful milestone.

## Definition of done for version 1.0

DarkGrain is complete when:

- It builds successfully as VST3 on Windows and VST3 + AU on macOS
- It loads in major DAWs
- It processes stereo audio without clicks, crashes, or allocation in the audio callback
- All parameters automate smoothly
- State persists correctly
- The grain, particle, and reverb engines work together musically
- The UI is complete and responsive
- Automated tests pass
- The project is ready for beta testing