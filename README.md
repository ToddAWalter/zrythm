<!---
SPDX-FileCopyrightText: © 2018-2025 Alexandros Theodotou <alex@zrythm.org>
SPDX-License-Identifier: FSFAP
-->

Zrythm
======

> [!WARNING]
> Zrythm is undergoing major refactoring in this branch.
> Use the `v1` branch if you are looking for a usable version.

[![translated](https://hosted.weblate.org/widgets/zrythm/-/svg-badge.svg "Translation Status")](https://hosted.weblate.org/engage/zrythm/?utm_source=widget)

*a highly automated and intuitive digital audio workstation*

![screenshot](https://www.zrythm.org/static/images/screenshots/screenshot-20240208.png)

Zrythm is a digital audio workstation tailored for both professionals and beginners, offering an intuitive interface and robust functionality.

Key features include:
* Streamlined editing workflows
* Flexible tools for creative expression
* Limitless automation capabilities
* Powerful mixing features
* Chord assistance for musical composition
* Support for various plugin and file formats

Zrythm is [free software](https://www.gnu.org/philosophy/free-sw.html) written
in C++23 using the Qt6/QML and JUCE8 frameworks.

## Features

- Object looping, cloning, linking and stretching
- Adaptive snapping
- Multiple lanes per track
- Bounce anything to audio or MIDI
- Piano roll (MIDI editor) with chord integration, drum mode and a lollipop velocity editor
- Audio editor with part editing (including in external app) and adjustable gain/fades
- Event viewers (list editors) with editable object parameters
- Per-context object functions
- Audio/MIDI/automation recording with options to punch in/out, record on MIDI input and create takes
- Device-bindable parameters for external control
- Wide variety of track types for every purpose
- Signal manipulation with signal groups, aux sends and direct anywhere-to-anywhere connections
- In-context listening by dimming other tracks
- Automate anything using automation events or CV signal from modulator plugins and macro knobs
- Detachable views for multi-monitor setups
- Searchable preferences
- Support for LV2/CLAP/VST2/VST3/AU/LADSPA/DSSI plugins, SFZ/SF2 SoundFonts, Type 0 and 1 MIDI files, and almost every audio file format
- Flexible built-in file and plugin browsers
- Optional plugin sandboxing (bridging)
- Stem export
- Chord pad with built-in and user presets, including the ability to generate chords from scales
- Automatic project backups
- Undoable user actions with serializable undo history
- Hardware-accelerated UI
- SIMD-optimized DSP
- Cross-platform, cross-audio/MIDI backend and cross-architecture
- Available in multiple languages including Chinese, Japanese, Russian, Portuguese, French and German

For a full list of features, see the
[Features page](https://www.zrythm.org/en/features.html)
on our website.

## Building and Installation

Prebuilt installers are available at <https://www.zrythm.org/en/download.html>.
This is the recommended way to install Zrythm.

See the following instructions if you would like to build Zrythm from source instead.

### Building From Source

> [!NOTE]
> We make heavy use of CMake's [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html) module to fetch dependencies so you don't need to install any dependencies manually other than CMake and Qt.
> This guide assumes you are fine with fetching dependencies automatically.

> [!IMPORTANT]
> If you distribute your builds to others, you must comply with the license terms of Zrythm and all dependencies we use, in addition to our [Trademark Policy](TRADEMARKS.md).

You can change `Release` to `Debug` below if you want to build in debug mode.

#### GNU/Linux

1. Install [CMake](https://cmake.org/), [Ninja](https://ninja-build.org/) and [Qt](https://www.qt.io/) 6.9.0 or later.
2. Open a terminal and go to the root of Zrythm's source code.
3. Run `cmake -B builddir -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<path to your Qt prefix>`.
4. Run `cmake --build build --config Release`.
5. Zrythm is now built at `builddir/src/gui/zrythm`.

#### macOS

1. Install Xcode, CMake and Qt 6.9.0 or later.
2. Open a terminal and go to the root of Zrythm's source code.
3. Run `cmake -B builddir -G Xcode -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<path to your Qt prefix>`.
4. Open the Xcode project inside `builddir` and build it.

#### Windows

1. Install Visual Studio 2022 (CMake ships with Visual Studio) and Qt 6.9.0 or later.
2. Open Developer Powershell for Visual Studio 2022 and go to the root of Zrythm's source code.
3. Run `cmake -B builddir -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<path to your Qt prefix>`.
4. Open the Visual Studio solution inside `builddir` and build it.

> [!NOTE]
> On Windows, you must build Qt with the same compiler and same configuration (Debug/Release) you use to build Zrythm.

## Using Zryhm
See the [user manual](http://manual.zrythm.org/).

## Contributing
See [CONTRIBUTING.md](CONTRIBUTING.md).

## Hacking
See [HACKING.md](HACKING.md) and the [developer docs](https://docs.zrythm.org/).

## Forum
See [our forum](https://forum.zrythm.org).

## Chat
* [Zrythm server on Discord](https://discord.gg/ScHUMcNtPb)
* [#zrythmdaw:matrix.org on Matrix](https://matrix.to/#/#zrythmdaw:matrix.org).
* [#zrythm on Libera.Chat IRC](https://web.libera.chat/#zrythm).

## Issue tracker
See [Issues on GitLab](https://gitlab.zrythm.org/zrythm/zrythm/issues).

## Releases
<https://www.zrythm.org/releases>

## Copying Zrythm
[![agpl-3.0](https://www.gnu.org/graphics/agplv3-with-text-162x68.png)](https://www.gnu.org/licenses/agpl-3.0)

See [COPYING](COPYING) for general copying conditions and
[TRADEMARKS.md](TRADEMARKS.md) for our trademark policy.

## Support
If you would like to support this project please donate below or purchase a
binary installer from
<https://www.zrythm.org/en/download.html> - creating
a DAW takes years of work and contributions enable
us to spend more time working on the project.

- [liberapay.com/Zrythm](https://liberapay.com/Zrythm/donate)
- [paypal.me/zrythmdaw](https://paypal.me/zrythmdaw)
- [opencollective.com/zrythm](https://opencollective.com/zrythm/donate)
- Bitcoin (BTC): `bc1qjfyu2ruyfwv3r6u4hf2nvdh900djep2dlk746j`
- Litecoin (LTC): `ltc1qpva5up8vu8k03r8vncrfhu5apkd7p4cgy4355a`
- Monero (XMR): `87YVi6nqwDhAQwfAuB8a7UeD6wrr81PJG4yBxkkGT3Ri5ng9D1E91hdbCCQsi3ZzRuXiX3aRWesS95S8ij49YMBKG3oEfnr`
