# OPAL VST3

OPAL is a compact input-reactive harmonic resonance processor for music production.

The selected frequency is **not** treated as a free-running meditation tone. OPAL extracts energy already present near the selected center and related harmonics, enhances that field, sends it into a tuned reverberant space, and widens only the processed field while leaving the dry source intact.

> The spiritual labels are creative/meditative associations, not medical claims.

## Current DSP path

```
INPUT
  -> selected-frequency band extraction
  -> boost
  -> related harmonic extraction/enhancement
  -> resonant reverb field
  -> wet-field stereo width
  -> additive MIX with untouched dry signal
```

The current engine uses causal IIR/state-variable filtering and algorithmic reverb. It uses no lookahead and reports **0 samples of plugin latency**.

## Controls

- **FREQ** — chooses the resonance center.
- **BOOST** — adds gain to source energy already present around the selected center.
- **HARMONICS** — increases related harmonic-band energy from the source.
- **SPACE** — blends the extracted field into OPAL's reverberant resonance space.
- **WIDTH** — scales stereo width of the processed field only.
- **MIX** — amount of the OPAL enhancement added to the untouched dry input.

## Frequency grid

Current selectable centers:

`111, 174, 222, 285, 333, 396, 417, 444, 528, 555, 639, 741, 777, 852, 888, 963, 999 Hz`

## UI direction

- Embedded animated opal gemstone as the visual centerpiece.
- Dark graphite hardware chassis.
- Liquid/prismatic fire inside the stone.
- Compact frequency grid with symbolic labels.
- UAD-style vertical FIELD meter.
- Deliberate rotary controls and restrained motion.

## Build

### Requirements

- CMake 3.22+
- C++20 compiler
- Git
- Windows x64 recommended for FL Studio testing

JUCE 9.0.3 is fetched automatically by CMake.

### Windows / Visual Studio

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Opal_VST3
```

Expected VST3 output:

```
build/Opal_artefacts/Release/VST3/OPAL.vst3
```

For local testing, copy the built `OPAL.vst3` bundle into a VST3 folder scanned by FL Studio and rescan plugins.

## Status

This repository currently contains the first functional architecture pass:

- VST3/Standalone JUCE project
- automatable parameters + state persistence
- input-reactive frequency/harmonic extraction
- resonant reverb field
- wet-only stereo widening
- resonance-energy metering
- first liquid OPAL UI
- Windows CI build pipeline

Next engineering passes should focus on listening tests, harmonic weighting, tuned-space voicing, mono compatibility, parameter edge cases, performance profiling, and final visual refinement.
