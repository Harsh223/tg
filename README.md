# Beatscope — Open-Source Mechanical Watch Timegrapher

Beatscope is a desktop application for timing and analyzing mechanical watches from live audio input (microphone or contact pickup). It provides practical tools for measuring rate, beat error, amplitude, and position comparisons in one workflow.

## Repository

- Source code: `https://github.com/Harsh223/beatscope`
- Latest release: `https://github.com/Harsh223/beatscope/releases/latest`
- All releases: `https://github.com/Harsh223/beatscope/releases`

If you cloned before the rename from `tg`, update your remote:

    git remote set-url origin https://github.com/Harsh223/beatscope.git

## Releases

Releases are built automatically from version tags (`v*`) using GitHub Actions.

When a tag such as `v0.1.1` is published, Beatscope generates release assets for supported platforms and uploads them to the GitHub release page.

Typical assets include:

- `beatscope-vX.Y.Z-source.tar.gz`
- `beatscope-vX.Y.Z-linux-x86_64.tar.gz`
- `beatscope-vX.Y.Z-windows-x86_64.zip`
- `beatscope-vX.Y.Z-macos-x86_64.tar.gz` / `beatscope-vX.Y.Z-macos-x86_64.zip` (when macOS runner is available)
- `beatscope-vX.Y.Z-sha256.txt`

## Install

Download the package for your OS from the latest release page.

### Windows

Use:

- `beatscope-vX.Y.Z-windows-x86_64.zip`

Install:

1. Download and extract the zip.
2. Run `beatscope.exe`.

### macOS

Use (if present in that release):

- `beatscope-vX.Y.Z-macos-x86_64.zip` or `.tar.gz`

Install:

1. Download and extract.
2. Run `beatscope`.

Gatekeeper note for unsigned builds:
- If macOS blocks first launch, right-click the binary and select **Open**.
- Then confirm in **System Settings → Privacy & Security** if prompted.

### Linux

Use:

- `beatscope-vX.Y.Z-linux-x86_64.tar.gz`

Install:

1. Download and extract.
2. Make executable if required:

       chmod +x beatscope

3. Run:

       ./beatscope

### Verify downloads (recommended)

Use the checksum file from the same release:

- `beatscope-vX.Y.Z-sha256.txt`

Example:

    sha256sum -c beatscope-vX.Y.Z-sha256.txt

## Key Features

- Real-time rate, beat error, and amplitude analysis
- Movement presets (quick BPH + lift-angle setup)
- Position-tagged snapshots (`DU`, `DD`, `CU`, `CD`, `CL`, `CR`)
- Position Summary panel with:
  - per-position rates
  - horizontal average
  - vertical average
  - `ΔH-V`
  - pair deltas (`DU-DD`, `CU-CD`)
- Guided mode and focus mode
- Save/load sessions (`.tgj`), with backward compatibility for original `tg` session data

## Quick Workflow

1. Start `beatscope`.
2. Select a movement preset or manually set BPH/lift angle.
3. Wait for stable signal detection.
4. Capture snapshots across positions.
5. Review Position Summary for regulation deltas.
6. Save the session for later comparison.

## Build From Source

### Dependencies

- GTK+ 3
- PortAudio
- FFTW3 (single precision, `fftw3f`)
- Autotools (`autoconf`, `automake`, `libtool`, `pkg-config`)
- C compiler (`gcc` or `clang`)

### Build

    git clone https://github.com/Harsh223/beatscope.git
    cd beatscope
    ./autogen.sh
    ./configure
    make

Run:

    ./beatscope

Debug build:

    make beatscope-dbg

## Platform Notes

### Debian / Ubuntu

    sudo apt-get install libgtk-3-dev portaudio19-dev libfftw3-dev git autoconf automake libtool pkg-config
    git clone https://github.com/Harsh223/beatscope.git
    cd beatscope
    ./autogen.sh
    ./configure
    make

### Fedora

    sudo dnf install fftw-devel portaudio-devel gtk3-devel autoconf automake libtool pkg-config git
    git clone https://github.com/Harsh223/beatscope.git
    cd beatscope
    ./autogen.sh
    ./configure
    make

### Windows (MSYS2)

    pacman -S mingw-w64-x86_64-gcc make pkg-config mingw-w64-x86_64-gtk3 mingw-w64-x86_64-portaudio mingw-w64-x86_64-fftw git autoconf automake libtool
    git clone https://github.com/Harsh223/beatscope.git
    cd beatscope
    ./autogen.sh
    ./configure
    make

## Upstream Lineage and Credits

Beatscope is a standalone continuation derived from a fork of the original **tg** project.

### Original project

- Project: `tg`
- Original author: **Marcello Mamino**
- Upstream repository: `https://github.com/vacaboja/tg`
- License: GNU GPL v2

### Beatscope continuation

- Maintainer: **Harsh223**
- Repository: `https://github.com/Harsh223/beatscope`

This project preserves attribution to original authors and contributors while continuing active development under the Beatscope name.

## License

Beatscope (including inherited `tg` code) is distributed under **GNU GPL v2**.

See `LICENSE` for full terms.