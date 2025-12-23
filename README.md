# Viz-A-Synth

An interactive synthesizer with real-time signal visualization. Built with JUCE and C++.

## Dependencies

### Required
- **CMake** 3.22 or higher

### Automatically Fetched
- **JUCE 8.0.4** (fetched via CMake FetchContent)

### Platform-Specific
- **Linux**: ALSA and JACK development libraries
  ```bash
  # Ubuntu/Debian
  sudo apt-get install libasound2-dev libjack-jackd2-dev

  # Fedora
  sudo dnf install alsa-lib-devel jack-audio-connection-kit-devel
  ```

## Building

### Quick Start

```bash
# Clone the repository
git clone https://github.com/yourusername/viz-a-synth.git
cd viz-a-synth

# Create build directory and configure
mkdir build && cd build
cmake ..

# Build (Debug)
cmake --build . --config Debug

# Or build Release
cmake --build . --config Release
```

**Tip: For faster builds, use all CPU cores by adding `-j` to the build command:**
```bash
cmake --build . -- -j
```
You can also specify the number of cores, e.g. `cmake --build . -- -j8` for 8 cores.

### Build Targets

The build creates two targets:
- **Standalone App**: `build/VizASynth_artefacts/Standalone/Viz-A-Synth.app` (macOS) or `Viz-A-Synth` (Linux)
- **VST3 Plugin**: Automatically installed to:
  - macOS: `~/Library/Audio/Plug-Ins/VST3/Viz-A-Synth.vst3`
  - Linux: `~/.vst3/Viz-A-Synth.vst3`

### Build Options

```bash
# Specify build type
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build only specific target
cmake --build . --target VizASynth_Standalone
cmake --build . --target VizASynth_VST3

# Clean build
rm -rf build && mkdir build && cd build && cmake ..
```

## Running

### Standalone App

**macOS:**
```bash
open build/VizASynth_artefacts/Standalone/Viz-A-Synth.app
```

**Linux:**
```bash
./build/VizASynth_artefacts/Standalone/Viz-A-Synth
```

### VST3 Plugin

Load the plugin in your DAW (Ableton Live, Logic Pro, Reaper, etc.). The plugin is automatically installed to your system's VST3 directory during build.

### Rebuilding After Changes

```bash
cd build
cmake --build . --config Debug
```

For faster iteration, use the Standalone target during development rather than loading the VST in a DAW.

## MIDI Testing (No Keyboard Required)

You can test the standalone app using Python scripts that send MIDI notes via a virtual port.

### Setup Python Environment

```bash
cd scripts
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

### Run MIDI Test Scripts

Start the standalone app, select `VizASynthTestPort` as MIDI input, then run:
```bash
python send_single_note.py
python send_scale.py
python keyboard_to_midi.py
```