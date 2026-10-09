# Punch Streamer

**Punch Streamer** is an OBS Studio plugin that adds a configurable animated punch effect to your camera source.

It tracks the face, positions the impact automatically, applies a directional distortion effect and plays the JAPISH impact sound.

**Compiled and maintained by ProfePikashu.**

## Features

- Face tracking using YuNet
- Automatic punch positioning
- Animated punch overlay
- Directional face distortion
- JAPISH impact sound
- Live configuration from OBS
- Per-profile settings
- Configurable camera source
- Customizable punch size and position
- Configurable punch speed
- Adjustable detection confidence
- Adjustable distortion force and direction
- Adjustable impact volume
- Built-in presets

## Presets

### Recommended
The original Punch Streamer experience.

### Subtle
A softer punch with reduced distortion.

### Cartoon
A larger and more exaggerated impact.

### Custom
Create your own configuration.

## Configuration

Open:

`Tools -> Punch Streamer`

Changes are applied live.

Use **Test JAPISH** to preview the effect.

## Hotkey

Punch Streamer supports an OBS frontend hotkey.

The development/default configuration uses:

`Ctrl + Alt + J`

The hotkey can be changed from OBS Studio's Hotkeys settings.

## Installation

### Easy installation

Punch Streamer v0.1.0 currently supports:

- Windows x64
- OBS Studio 32+

1. Install OBS Studio normally.
2. Close OBS Studio if it is running.
3. Open the Punch Streamer GitHub Releases page.
4. Download `PunchStreamer-0.1.0-windows-x64.zip`.
5. Open the ZIP. Inside you will see two folders: `obs-plugins` and `data`.
6. Copy both folders into your OBS Studio installation folder.

For a standard OBS installation, that folder is usually:

`C:\Program Files\obs-studio\`

Allow Windows to merge the folders if prompted.

7. Start OBS Studio.
8. If you do not have a camera yet, add one with `Sources -> + -> Video Capture Device`.
9. Open `Tools -> Punch Streamer`.
10. Select your camera source.
11. Keep the **Recommended** preset for the original effect.
12. Configure the trigger in `Settings -> Hotkeys -> Punch Streamer`.

You are ready to JAPISH.

## Building

Punch Streamer is based on the official OBS Plugin Template and uses CMake.

The current Windows development environment uses:

- Visual Studio 2022
- CMake
- Qt 6
- OpenCV
- OBS Studio frontend API

## License

The Punch Streamer source code is distributed under the GNU General Public License v2.

Third-party components and assets may retain their respective licenses or ownership.

## Credits

Developed, compiled and maintained by **ProfePikashu**.

Powered by OBS Studio, OpenCV and YuNet.


