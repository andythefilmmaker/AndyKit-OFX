# Linework

**Open source OpenFX plugin for DaVinci Resolve Studio** that turns the edges of your footage into points, shapes and lines — with temporal pixel memory, contour fields, dither print, glitch and a beat pulse.

![Linework looks](docs/preview.jpg)

*Andy Lines · Andy Lines Gray · Andy Circles · Andy Dither*

- Works on the **Edit** page (as a clip effect) and the **Color** page (as a node).
- **macOS** (Apple Silicon + Intel), **Windows** and **Linux** builds from every release.
- Every parameter is keyframable; every frame renders the same whether you play, scrub or render on a farm.
- MIT licensed.

## Install

Download the zip for your system from [Releases](../../releases), then follow [`docs/INSTALL.txt`](docs/INSTALL.txt):

| System  | Copy `Linework.ofx.bundle` to          |
|---------|----------------------------------------|
| macOS   | `/Library/OFX/Plugins/`                |
| Windows | `C:\Program Files\Common Files\OFX\Plugins\` |
| Linux   | `/usr/OFX/Plugins/`                    |

On macOS run once: `sudo xattr -dr com.apple.quarantine /Library/OFX/Plugins/Linework.ofx.bundle`

In Resolve: **Effects → OpenFX → Filters → ANDYTHEFILMMAKER → Linework**.

## What it does

| Group | Controls |
|---|---|
| **Presets** | Built-in looks + *Apply Preset*, frame title |
| **General** | Seed, colour mode (source / palette / mono / duotone / hybrid), 8 palettes, 12 blend modes, source & overlay opacity, artwork frame, labels |
| **Motion** | Glitch, drift, pulse, orbit — amount and speed |
| **Points** | Density, edge threshold, point radius, node shapes (bracket, square, circle, cross, diamond, mixed), size range, hierarchy, variation, halo circles |
| **Connections** | Contour-following, nearest, radial, orthogonal; strength, edge confidence, weight, hub network |
| **Subject** | Emphasize or isolate the area with the strongest detail |
| **Pixel Memory** | Temporal feedback in drifting windows — feedback, glyphs or archive style |
| **Contours** | Topographic iso-lines or flow streamlines |
| **Grid / Dither / Texture** | Modular grid, Atkinson / Floyd-Steinberg / Bayer dither print, scanlines, grain |
| **Rhythm** | BPM-locked pulse driving shapes, lines and glitch (OpenFX effects don't receive audio) |

## Build from source

Requirements: CMake ≥ 3.20 and a C++17 compiler. The OpenFX SDK (1.5.1) and stb are fetched automatically.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
# → build/bundle/Linework.ofx.bundle
```

macOS universal: add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.

### Testing without Resolve

```sh
# render the engine on a still (12 frames so motion and memory evolve)
./build/lw_render tests/assets/portrait.png out.png --preset "Andy Circles" --frames 12 density=80

# load the real plugin bundle in a minimal OpenFX host (needs expat): -DLW_BUILD_TESTS=ON
./build/smoke_host build/bundle tests/assets/portrait.png out --frames 4 --preset 2
```

## Project layout

```
src/core/     engine — no OpenFX dependency (analysis, drawing, layers, presets)
src/ofx/      OpenFX plugin wrapper
tools/        lw_render command-line renderer
tests/ofxhost minimal OpenFX host for CI smoke tests
```

Parameters are declared once in `src/core/Params.h` (`LW_PARAM_LIST`) and drive the engine, the plugin UI and the CLI.

## Roadmap

- GPU rendering (Metal / CUDA / OpenCL)
- Real-time preview optimisations for 4K timelines
- More looks; preset import/export as text

## Credits

Created by **Andrea Pio Molinari — ANDYTHEFILMMAKER**.

Uses the [OpenFX SDK](https://github.com/AcademySoftwareFoundation/openfx) (BSD-3-Clause) and [stb](https://github.com/nothings/stb) (public domain / MIT) — see [THIRD_PARTY.md](THIRD_PARTY.md).

---

### In italiano

Linework è un plugin OpenFX gratuito e open source per DaVinci Resolve Studio: trasforma i bordi del girato in punti, forme e linee, con memoria dei pixel, contour, dither, glitch e pulsazione a tempo. Si installa copiando `Linework.ofx.bundle` nella cartella dei plugin OFX (vedi tabella sopra) e si trova in **Effects → OpenFX → ANDYTHEFILMMAKER → Linework**.
