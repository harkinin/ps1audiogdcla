# PS1 Audio Effect for Godot 4.7

A live C++ GDExtension audio effect for Godot 4.7.x. Add it to the Master audio bus to process all audio routed through the bus.

Features:
- Adjustable signed PCM-style bit-depth quantisation (4–16 bit).
- Sample-rate reduction using a zero-order hold, without changing playback speed.
- One-pole low-pass filtering.
- Soft saturation/drive.
- Windows x64 debug and release builds through GitHub Actions.

This is a PS1-inspired lo-fi effect, not an exact recreation of the PlayStation's ADPCM hardware codec.

## Download the build

Every push to main runs the Windows x64 build. Once the workflow completes, download the ps1-audio-effect-windows-x64 artifact from the run's Artifacts section.

## Install

1. Extract the artifact ZIP.
2. Copy the included addons/ps1_audio_effect folder into your Godot project's res://addons/ folder.
3. Reopen the project so Godot loads the GDExtension.
4. In the Audio panel, select Master → Add Effect → PS1AudioEffect.

Alternatively, add addons/ps1_audio_effect/ps1_audio_autoload.gd as an Autoload named PS1Audio to install/configure the effect automatically. Do not use both installation methods at once.

## Default starting point

- Bit depth: 10
- Effective sample rate: 22050 Hz
- Low-pass cutoff: 10000 Hz
- Drive: 1.15

## Build locally

Requires Git, Python 3, SCons, and a C++ compiler toolchain. On Windows, use Developer PowerShell for Visual Studio with the C++ workload installed:

    py -3 -m pip install --user scons
    git clone --depth 1 --branch master https://github.com/godotengine/godot-cpp.git godot-cpp
    scons platform=windows target=template_debug arch=x86_64
    scons platform=windows target=template_release arch=x86_64

The included SConstruct targets Godot API 4.7.

## Tests

The DSP lives in `src/ps1_dsp.h` with no Godot dependency, so it can be tested on any machine with a C++17 compiler:

    g++ -std=c++17 -O1 -fsanitize=address,undefined -I src tests/dsp_test.cpp -o dsp_test
    ./dsp_test

## Troubleshooting

- **Restart the editor after replacing the DLL.** Hot-reload is disabled (`reloadable = false`) because the effect instance runs on the audio thread, and unloading the library under a live Master-bus effect crashes Godot.
- When the effect is added to a bus, the Output panel should print `PS1 Audio: created effect instance (mix rate ... Hz).` If Godot crashes before that line appears, the crash is happening before the effect is instantiated.
- To get a stack trace for a crash, run the editor from a terminal with `Godot_v4.7.2-stable_win64_console.exe --editor --path <project>` and copy the `handle_crash` output.