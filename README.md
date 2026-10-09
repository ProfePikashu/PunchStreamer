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

Punch Streamer v0.1.0 currently targets:

- Windows x64
- OBS Studio 32+

Copy the packaged plugin files into your OBS Studio installation directory, preserving the included folder structure.

Restart OBS Studio after installation.

Then open:

`Tools -> Punch Streamer`

and select your camera source.

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
