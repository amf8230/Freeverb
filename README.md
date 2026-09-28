# Echo Vault

A vintage chamber / plate / room / hall reverb plugin (VST3 + AU), built with JUCE.

See **[INSTALL.md](INSTALL.md)** for build, install, and usage instructions.

## Project layout

```
Source/ReverbEngine.h        Reverb DSP (framework-independent C++17)
Source/Presets.h             Factory presets
Source/PluginProcessor.*     Plugin wrapper: parameters, presets, state
Source/PluginEditor.*        User interface
Tests/RenderTest.cpp         Offline tests: RT60 accuracy, stability, stereo, demo renders
Installer/                   macOS install script
.github/workflows/build.yml  Cloud build: tests + universal macOS VST3/AU
```

## DSP overview

* Pre-delay, then per-room early reflection patterns (tapped delay lines)
* Input diffusion (2 to 6 Schroeder allpass stages depending on room)
* 16-line feedback delay network with a Hadamard mixing matrix and prime-length delays
* Jot absorption filters give frequency-dependent decay (separate LF and HF RT60)
* Slow, cubic-interpolated delay modulation for smooth, non-metallic tails
* Per-room voicing EQ, user low/high cut, M/S width, tube-style send saturation
* Smoothed parameters and a crossfaded room switch, so there are no clicks when automating

## Building locally (optional)

Requires CMake 3.22+ and a C++17 compiler (Xcode on macOS). JUCE is downloaded automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4
```

Offline engine test: `g++ -O2 -std=c++17 -I Source Tests/RenderTest.cpp -o render_test && ./render_test`

## License note

JUCE 8 is dual-licensed (AGPLv3 or a JUCE commercial license). Private, personal and
in-studio use is fine. Selling or distributing the plugin would require either releasing
the source under AGPLv3 or getting a JUCE license (free "Starter" tier for small revenue).
