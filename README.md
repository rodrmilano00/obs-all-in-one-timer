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

### Requirements

- **OBS Studio 32.0 or newer**, 64-bit (built and tested against 32.0.1)
- **Windows 10/11 x64**

### Option A: Download a release (recommended)

1. Go to the [Releases](../../releases) page
2. Download `all-in-one-timer-vX.X.X-windows-x64.zip`
3. **Close OBS Studio**
4. Extract the zip into the OBS plugins folder:
   ```
   C:\ProgramData\obs-studio\plugins\
   ```
   Tip: paste `%ProgramData%\obs-studio\plugins` in the Explorer address bar. If the `plugins` folder does not exist yet, create it.
5. Check that the files ended up exactly like this:
   ```
   C:\ProgramData\obs-studio\plugins\
   └── all-in-one-timer\
       ├── bin\
       │   └── 64bit\
       │       └── all-in-one-timer.dll
       └── data\
           └── locale\
               ├── en-US.ini
               └── es-ES.ini
   ```
6. Open OBS Studio
7. In a scene, click **+** under *Sources* and add **All In One Timer**

> **Important:** Keep the `data\locale` folder next to `bin`. Without it the plugin still loads, but the properties show internal keys (`Group.TimerControls`, `CdHours`...) instead of readable names.

#### Installing only the DLL

If you only have `all-in-one-timer.dll`, create the folders above by hand, put the DLL in `bin\64bit\`, and copy the two `.ini` files from this repo's `data\locale\` into `data\locale\`.

#### Updating

Close OBS, replace `all-in-one-timer.dll` (and the `.ini` files) with the new ones, and open OBS again. Your timer sources and their settings are kept.

#### Uninstalling

Close OBS and delete `C:\ProgramData\obs-studio\plugins\all-in-one-timer\`.

#### Troubleshooting

If **All In One Timer** is not in the source list:

1. Make sure OBS was fully closed (check the system tray) and reopened after copying the files
2. Check the folder structure above. The DLL must be in `bin\64bit\`, not directly in `all-in-one-timer\`
3. Open *Help → Log Files → View Current Log* and search for `all-in-one-timer`. A line like `Loading module: all-in-one-timer.dll` means it loaded. An error near it says why it did not
4. Check that your OBS version is 32.0 or newer (*Help → About*)

### Option B: Build from source

#### Prerequisites

- **Visual Studio 2022** (or Build Tools) with the C++ workload (includes CMake)
- **OBS Studio 32.0+ x64** installed (tested with 32.0.1)
- **Git**

The build does not need the full OBS source tree or its dependencies: CMake sparse-clones only the `libobs` headers and generates `obs.lib` from your installed `obs.dll`.

#### Steps

From a *Developer PowerShell for VS 2022* (or with `cmake` on your PATH):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
cmake --install build --config Release
```

`cmake --install` copies the plugin to `C:\ProgramData\obs-studio\plugins\all-in-one-timer\` (close OBS first, the DLL is locked while OBS runs). Open OBS afterwards.

The built DLL is at `build\Release\all-in-one-timer.dll`.

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
├── dist/                    # Release packages (gitignored)
├── CMakeLists.txt
└── README.md
```

---

## Releases

### For Users - How to Install from a Release

See [Option A: Download a release](#option-a-download-a-release-recommended). In short: extract the zip into `C:\ProgramData\obs-studio\plugins\` with OBS closed, then open OBS.

### For Maintainers - How to Create a Release

1. **Update the version** in `CMakeLists.txt` (`project(all-in-one-timer VERSION X.Y.Z ...)`)
2. **Build in Release mode** (see [Build from source](#option-b-build-from-source))
3. **Package the zip** from the repo root (PowerShell):
   ```powershell
   $version = "1.0.0"
   cmake --install build --config Release --prefix dist/stage
   tar -a -c -f "dist/all-in-one-timer-v$version-windows-x64.zip" -C dist/stage all-in-one-timer
   ```
   Use `tar` instead of `Compress-Archive`: Windows PowerShell 5 writes zip paths with backslashes, which some extractors mishandle.
4. **Test the zip**: close OBS, extract it into `C:\ProgramData\obs-studio\plugins\`, open OBS and add the source
5. **Create a GitHub release**:
   - Go to the repo's **Releases** page on GitHub
   - Click **Draft a new release**
   - **Tag version**: semantic versioning (e.g. `v1.0.0`)
   - **Release title**: e.g. `All In One Timer for OBS v1.0.0`
   - **Description**: list changes, new features and bug fixes
   - **Attach** `all-in-one-timer-vX.X.X-windows-x64.zip` from `dist\`
   - Click **Publish release**

#### What Each Release Should Include

| Asset | Description |
|-------|-------------|
| `all-in-one-timer-vX.X.X-windows-x64.zip` | Plugin folder ready to extract into `C:\ProgramData\obs-studio\plugins\` (DLL + locale files) |

#### Release Description Template

```markdown
## All In One Timer for OBS vX.X.X

### Features
- Feature 1
- Feature 2

### Bug Fixes
- Fix 1

### Installation
1. Close OBS Studio
2. Download `all-in-one-timer-vX.X.X-windows-x64.zip`
3. Extract it into `C:\ProgramData\obs-studio\plugins\`
   (you should end up with `plugins\all-in-one-timer\bin\64bit\all-in-one-timer.dll`)
4. Open OBS and add the **All In One Timer** source

### Requirements
- OBS Studio 32.0+ (64-bit)
- Windows 10/11 x64
```

### v1.0.0 Release Notes

```markdown
## All In One Timer for OBS v1.0.0

First release: OBS Studio port of the All In One Timer FFGL plugin for Resolume Arena.

### Features
- Count Up / Countdown modes with MM:SS or HH:MM:SS formats
- Countdown setup with separate Hours / Minutes / Seconds sliders
- Installed system font dropdown
- Text and background colors with alpha (native OBS color picker)
- Size, X and Y position controls
- Optional background box
- Auto-pause while the source is not on the program output
- Start/Pause and Reset hotkeys, also usable from obs-websocket
- English and Spanish interface

### Installation
1. Close OBS Studio
2. Download `all-in-one-timer-v1.0.0-windows-x64.zip`
3. Extract it into `C:\ProgramData\obs-studio\plugins\`
4. Open OBS and add the **All In One Timer** source

### Requirements
- OBS Studio 32.0+ (64-bit)
- Windows 10/11 x64
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
