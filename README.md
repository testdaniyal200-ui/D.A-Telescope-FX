# D.A Production — Telescope FX

A complete **source project** for a custom stereo spatial delay/reverb plug-in, written with **JUCE 8 + C++17 + CMake**. Inspired visually by the user-supplied Telescope FX concept. The audio implementation is original, **not** a recreation of any proprietary DSP algorithm.

**Formats:** VST3 (Windows x64), AU + VST3 (macOS x86_64/arm64 universal). macOS AU requires compilation on macOS. **No prebuilt binaries are included.**

## Fastest way: build with GitHub Actions

1. Create a new, preferably private, GitHub repository.
2. Unzip this project and **upload the contents of the `DA_Production_Telescope_FX` folder to the repository root**, including the hidden `.github` directory. (Do not upload only the ZIP.)
3. Commit/push on branch `main`, or open **Actions → Build D.A Production Telescope FX → Run workflow**.
4. Wait for both build jobs to finish and get files under the workflow run's **Artifacts** section:
   - `DA-Telescope-Windows-x64-VST3` → Windows 64-bit VST3 ZIP.
   - `DA-Telescope-macOS-Universal-AU-VST3` → AU and VST3 ZIPs for macOS Intel + Apple Silicon.
5. Extract the artifacts, then install the plug-in bundle:
   - Windows: copy `Telescope FX.vst3` to `C:\Program Files\Common Files\VST3\`
   - macOS AU: copy `Telescope FX.component` to `~/Library/Audio/Plug-Ins/Components/`
   - macOS VST3: copy `Telescope FX.vst3` to `~/Library/Audio/Plug-Ins/VST3/`
   - Restart/rescan your DAW's plug-in list.

> GitHub CI outputs are **unsigned**. macOS Gatekeeper or some DAWs may require local authorization or properly signed/notarized builds. For redistribution, configure Apple Developer signing and notarization; production releases should also be tested in multiple DAWs.

## Local builds

Requirements: CMake >= 3.24, Git, C++17, Windows Visual Studio 2022 with C++ desktop workload, or macOS Xcode and command-line tools. First configuration downloads JUCE from GitHub.

**Windows (PowerShell):**

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target DA_Telescope_VST3 --parallel 4
```

**macOS (Terminal):**

```bash
cmake -S . -B build -G Xcode '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --target DA_Telescope_AU DA_Telescope_VST3 --parallel 4
```

**Test assets and presets:**

```bash
python -m unittest discover -s tests -v
```

## Sound engine

Telescope FX provides a real-time stereo signal processor with an interpolated fractional-delay ring buffer, gentle feedback filtering, stereo mid-side widening, LFO-modulated motion, and JUCE's reverb engine. There are five selectable sound-character modes:

- **Telescope:** spatial echo and immersive reverb.
- **Orbit:** rotating stereo motion.
- **Warp:** moving, cross-coupled echoes.
- **Dimension:** short modulated spatial taps.
- **Atmosphere:** longer, more reverberant tails.

Main knobs: `Zoom`, `Spread`, `Distance`, `Motion`. Secondary controls in the gear panel: `Mix`, `Feedback`, `Tone`. Four groups of three-position switches control zoom/spread/distance/motion profiles. A host-automatable `Engine` parameter chooses the five modes. Input and output amplitude meters are displayed at the bottom.

The plug-in supports stereo and mono audio buses; there is no MIDI input. DSP buffers are allocated in `prepareToPlay`, not in the audio callback. Factory preset recall uses host-notified parameter changes. The entire parameter state is persisted via APVTS state serialization.

## 100 factory presets

100 named, distinct presets across 10 banks of 10: Nebula, Orbit, Warp, Deep Space, Dimension, Atmosphere, Vocal, Rhythm, Guitar, and Cinema. They are:

- compiled into the plug-in from `Source/FactoryPresets.h` for reliable DAW hosting and cross-platform use;
- separately available as editable, inspection-friendly JSON in `Presets/` and `Presets/index.json`.

Select presets with the header arrows or click the preset name for a grouped submenu. DAW program APIs also enumerate all 100. These are **parameter factory presets**, not preset WAV/IR assets; the JSON files are documentation/import source, while the compiled presets are automatically included with the binaries.

## GUI files

| File | Role |
|---|---|
| `Assets/Generated/interface-preview.png` | Full D.A Production mockup |
| `Assets/Generated/knob-hires.png` | High-resolution transparent knob |
| `Assets/Generated/galaxy-hud-hires.png` | High-resolution circular nebula HUD |
| `Assets/Generated/button-telescope-active.png` | Active button |
| `Assets/Generated/button-orbit-inactive.png` | Inactive button |
| `Assets/Generated/preset-selector.png` | Preset selector |
| `Assets/Generated/waveform-meter.png` | Waveform design |
| `Assets/Generated/nebula-background.png` | Space background |
| `Assets/Generated/radio-option-group.png` | Radio selectors |

The three referenced runtime images in `Assets/Runtime/` are embedded in the executable via JUCE BinaryData; other UI elements are vector-drawn so their labels, states, knob highlights, meters, and presets remain interactive. The entire GUI scales when the plug-in window is resized.

**Visual fidelity note:** The user-provided image is a design reference, not source-layer graphics or an original DSP specification. This implementation closely follows its layout, palette, text hierarchy and texture assets, but is not a pixel-for-pixel clone. Most notably, the image-generated central HUD contains a static graphic, while the on-screen numeric zoom is dynamically overdrawn.

## File structure

```text
.github/workflows/build.yml   Windows/macOS CI
CMakeLists.txt               JUCE/CMake build project
Source/PluginProcessor.*     Real-time DSP, parameters, state + presets
Source/PluginEditor.*        Interactive GUI
Source/FactoryPresets.h      100 presets compiled into plugin
Presets/                    100 individual JSON factory presets + index
Assets/Runtime/             3 embedded runtime PNG files
Assets/Generated/           9 high-resolution original GUI art assets
tests/test_project.py        Integrity tests
```

## License and distribution

The project's custom source code and generated images are supplied for the user's project. JUCE is a separate dependency with its **own GPLv3/commercial licensing conditions**: check [juce.com/get-juce](https://juce.com/get-juce/) before distributing a closed-source commercial plugin. The GitHub workflow fetches JUCE; it is **not** included in this ZIP. Branding is **D.A Production**; no original company logo is used in the project GUI.

## Engineering status

Files and assets have been generated and project-level integrity tests run. **The JUCE plug-in has not been cross-compiled/host-tested in this environment** (JUCE cannot be downloaded from here). GitHub Actions is included to perform actual VST3 and AU compilation on supported runners. Verify successful builds, audio behavior, preset recall, automation, audio stability, and GUI scaling in your DAW before production deployment.
