# Beatscope — Open-Source Mechanical Watch Timegrapher

Beatscope is a desktop application for timing and analyzing mechanical watches from live audio input (microphone or contact pickup). It is designed for watchmakers and enthusiasts who need practical rate, beat error, amplitude, and position-comparison tools in one place.

## Repository status

- Current repository: `https://github.com/Harsh223/tg`
- Planned rename: `https://github.com/Harsh223/beatscope`

If you cloned before the rename, update your remote with:

    git remote set-url origin https://github.com/Harsh223/beatscope.git

## Releases (fully automated)

This project uses a tag-based GitHub Actions release pipeline with **no manual asset uploads**.

When a version tag like `v0.6.0` is pushed, CI automatically builds and publishes all release assets to GitHub Releases.

### Automatically generated assets

For each release tag `vX.Y.Z`, the workflow publishes:

- `beatscope-vX.Y.Z-source.tar.gz`
- `beatscope-vX.Y.Z-linux-x86_64.tar.gz`
- `beatscope-vX.Y.Z-macos-x86_64.tar.gz`
- `beatscope-vX.Y.Z-macos-x86_64.zip`
- `beatscope-vX.Y.Z-windows-x86_64.zip`
- `beatscope-vX.Y.Z-sha256.txt`

Release pages:

- Current: `https://github.com/Harsh223/tg/releases/latest`
- After rename: `https://github.com/Harsh223/beatscope/releases/latest`


## Install

Download assets from GitHub Releases and choose the package for your platform.

### Windows

Use:

- `beatscope-vX.Y.Z-windows-x86_64.zip`

Install:

1. Download the zip.
2. Extract it.
3. Run `beatscope.exe`.

### macOS

Use one of:

- `beatscope-vX.Y.Z-macos-x86_64.zip`
- `beatscope-vX.Y.Z-macos-x86_64.tar.gz`

Install:

1. Download the archive.
2. Extract it.
3. Move `beatscope` to your preferred location (for example `Applications`).
4. If blocked on first run, right-click -> **Open**.

### Linux

Use:

- `beatscope-vX.Y.Z-linux-x86_64.tar.gz`

Install:

1. Download and extract.
2. Make executable if needed:

       chmod +x beatscope

3. Run:

       ./beatscope

### Verify downloads (recommended)

Use the checksum file from the same release:

- `beatscope-vX.Y.Z-sha256.txt`

Example:

    sha256sum -c beatscope-vX.Y.Z-sha256.txt

## Key features

- Real-time rate / beat error / amplitude analysis
- Movement presets (quick BPH + lift-angle setup)
- Position-tagged snapshots (`DU`, `DD`, `CU`, `CD`, `CL`, `CR`)
- Position Summary panel with:
  - per-position rates
  - horizontal average
  - vertical average
  - `ΔH-V`
  - pair deltas (`DU-DD`, `CU-CD`)
- Guided mode and focus mode
- Save/load sessions (`.tgj`), with backward compatibility for tg session data

## Quick workflow

1. Start `beatscope`.
2. Select a movement preset or manually set BPH/lift angle.
3. Wait for a stable signal state.
4. Capture snapshots in multiple positions.
5. Review Position Summary for regulation deltas.
6. Save session files for records and comparison.

## Build from source

### Dependencies

- GTK+ 3
- PortAudio
- FFTW3 (single precision, `fftw3f`)
- Autotools (`autoconf`, `automake`, `libtool`, `pkg-config`)
- C compiler (`gcc` or `clang`)

### Build commands

Current repository path:

    git clone https://github.com/Harsh223/tg.git
    cd tg
    ./autogen.sh
    ./configure
    make

After rename:

    git clone https://github.com/Harsh223/beatscope.git
    cd beatscope
    ./autogen.sh
    ./configure
    make

Run:

    ./beatscope

Debug build:

    make beatscope-dbg

## Platform notes

### Debian / Ubuntu

    sudo apt-get install libgtk-3-dev portaudio19-dev libfftw3-dev git autoconf automake libtool pkg-config
    git clone https://github.com/Harsh223/tg.git
    cd tg
    ./autogen.sh
    ./configure
    make

### Fedora

    sudo dnf install fftw-devel portaudio-devel gtk3-devel autoconf automake libtool pkg-config git
    git clone https://github.com/Harsh223/tg.git
    cd tg
    ./autogen.sh
    ./configure
    make

### Windows (MSYS2)

    pacman -S mingw-w64-x86_64-gcc make pkg-config mingw-w64-x86_64-gtk3 mingw-w64-x86_64-portaudio mingw-w64-x86_64-fftw git autoconf automake libtool
    git clone https://github.com/Harsh223/tg.git
    cd tg
    ./autogen.sh
    ./configure
    make

## Upstream lineage and credits

Beatscope is a standalone continuation derived from a fork of the original **tg** project.

### Original project

- Project: `tg`
- Original author: **Marcello Mamino**
- Original upstream repository: `https://github.com/vacaboja/tg`
- License: GNU GPL v2

### Beatscope continuation

- Maintainer: **Harsh223**
- Current repository: `https://github.com/Harsh223/tg`
- Planned renamed repository: `https://github.com/Harsh223/beatscope`

This project preserves attribution to original authors and contributors while continuing active development under the Beatscope name.

## License

Beatscope (including inherited tg code) is distributed under **GNU GPL v2**.

See `LICENSE` for full terms and contributor notices.
