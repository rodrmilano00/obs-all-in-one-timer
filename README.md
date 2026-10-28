# All In One Timer - OBS Studio Plugin

OBS Studio port of the [All In One Timer](https://github.com/rodrmilano00/resolume-all-in-one-timer) FFGL plugin for Resolume Arena: a customizable stopwatch / countdown source with the same controls and behavior.

---

## Features

- **Count Up / Countdown modes** - Use as a stopwatch or set a countdown target
- **Time formats** - `MM:SS` or `HH:MM:SS`
- **Countdown setup** - Separate Hours, Minutes and Seconds sliders
- **System font selection** - Dropdown with all installed Windows fonts
- **Native OBS color picker** - Text and background colors with alpha
- **Adjustable size and position** - Same Size / X / Y controls as the Resolume version
- **Optional background** - Colored box behind the timer
- **Auto-pause** - The timer only runs while the source is on the program output, and resumes when it comes back
- **Hotkeys** - Start/Pause and Reset can be bound in *Settings → Hotkeys*
- **High-resolution text** - Rendered with GDI at 400px, like the original

---

## Installation

### Option A: Pre-built DLL

1. Copy the plugin folder so it looks like this:
   ```
   C:\ProgramData\obs-studio\plugins\all-in-one-timer\
   ├── bin\64bit\all-in-one-timer.dll
   └── data\locale\en-US.ini, es-ES.ini
   ```
2. Restart OBS Studio
3. In a scene, click **+** and add **All In One Timer**

### Option B: Build from source

#### Prerequisites

- **Visual Studio 2022** (or Build Tools) with the C++ workload (includes CMake)
- **OBS Studio 28+ x64** installed (tested with 32.0.1)
- **Git**

The build does not need the full OBS source tree or its dependencies: CMake sparse-clones only the `libobs` headers and generates `obs.lib` from your installed `obs.dll`.

#### Steps

From a *Developer PowerShell for VS 2022* (or with `cmake` on your PATH):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
cmake --install build --config Release
```

`cmake --install` copies the plugin to `C:\ProgramData\obs-studio\plugins\all-in-one-timer\`. Restart OBS afterwards.

| CMake option | Default | Description |
|--------------|---------|-------------|
| `OBS_INSTALL_DIR` | `C:/Program Files/obs-studio` | OBS install folder, used to generate `obs.lib` |
| `OBS_VERSION_TAG` | `32.0.1` | obs-studio tag the headers are fetched from (match your OBS) |
| `LIBOBS_SOURCE_DIR` | `deps/obs-studio` | Existing obs-studio checkout, if you already have one |

---

## Usage

Add an **All In One Timer** source to a scene. The source is the size of your canvas, so the Size / X / Y properties work exactly like in Resolume (you can still move or crop the source with the OBS transform tools).

### Timer Controls

| Property | Type | Description |
|----------|------|-------------|
| Start/Pause | Button / Hotkey | Starts or pauses the timer |
| Reset | Button / Hotkey | Stops the timer and resets it to zero |
| Format | Dropdown | `MM:SS` or `HH:MM:SS` |
| Mode | Dropdown | `Count Up` (stopwatch) or `Countdown` |
| CD Hours | Slider 0-23 | Countdown target hours |
| CD Minutes | Slider 0-59 | Countdown target minutes |
| CD Seconds | Slider 0-59 | Countdown target seconds |

### Text Color

| Property | Type | Description |
|----------|------|-------------|
| Color | Color picker (with alpha) | Text color and transparency |

### Display

| Property | Type | Description |
|----------|------|-------------|
| Size | Slider 0.05-2.0 | Text height relative to the canvas height |
| X Position | Slider 0-1 | Horizontal center (0 = left, 1 = right) |
| Y Position | Slider 0-1 | Vertical center (0 = bottom, 1 = top, as in Resolume) |
| Font | Dropdown | Installed system fonts |

### Background

| Property | Type | Description |
|----------|------|-------------|
| Show BG | Checkbox | Toggle the background box |
| BG Color | Color picker (with alpha) | Background color and transparency |

### Auto-Pause Behavior

The timer only advances while the source is **active**, i.e. shown on the program output. Switching to a scene that does not contain it pauses the timer, and switching back resumes it from where it left off. In Studio Mode, a scene that is only in *Preview* does not count as active.

### Remote control

- **Hotkeys**: *Settings → Hotkeys* lists *Start/Pause* and *Reset* under the source name.
- **obs-websocket**: send `PressInputPropertiesButton` with `propertyName` set to `start_pause` or `reset`.

---

## Differences from the Resolume version

| Resolume (FFGL) | OBS |
|-----------------|-----|
| Hue / Saturation / Brightness / Alpha parameters | One color picker with alpha (text and background) |
| Pauses when the layer leaves the composition output | Pauses when the source leaves the program output |
| Start/Pause and Reset as event parameters | Property buttons, plus hotkeys and obs-websocket |

Elapsed time is not saved between OBS sessions, same as the original.

---

## Project Structure

```
obs-all-in-one-timer/
├── src/
│   └── all-in-one-timer.c   # Plugin source
├── data/
│   └── locale/              # en-US / es-ES strings
├── deps/                    # libobs headers (fetched by CMake, gitignored)
├── build/                   # Build output (gitignored)
├── CMakeLists.txt
└── README.md
```

---

## Technical Details

- Text is drawn with **GDI** (`DrawTextW`, 400px font) into a DIB, converted to a coverage mask, then tinted with the text color and uploaded as a dynamic `GS_BGRA` texture. It is only re-rendered when the time string, font or color changes.
- The background is drawn with OBS's built-in solid effect under the text quad; the text uses the default effect with alpha blending.
- Timer state is updated in `video_tick` (graphics thread). Settings, buttons and hotkeys come from other threads, so shared state is guarded by an `SRWLOCK`.
- Windows x64 only, because of GDI text rendering.

---

## License

BSD 3-Clause, same as the original project. libobs headers are GPL-2.0 (OBS Studio).

---

## Author

**rodrmilano00**
