# fastwad

[![CI](https://github.com/urgorri/fastwad/actions/workflows/ci.yml/badge.svg)](https://github.com/urgorri/fastwad/actions/workflows/ci.yml)
[![Pages Showcase](https://github.com/urgorri/fastwad/actions/workflows/pages.yml/badge.svg)](https://urgorri.github.io/fastwad/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-emerald.svg)](https://github.com/urgorri/fastwad)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17-purple.svg)](CMakeLists.txt)

**fastwad** is a modern, high-performance, deterministic C++ engine and CLI utility for compiling, inspecting, and extracting GoldSrc (Half-Life 1, Counter-Strike 1.6) and Quake (WAD2/WAD3) texture packages.

It offers byte-level deterministic builds, spatial mipmap downsampling (4 mip levels), per-texture 256-color K-Means++ quantization, Floyd-Steinberg dithering, full transparency alpha mask mapping, and machine-readable JSON output for seamless integration into mapping tools (TrenchBroom, J.A.C.K., Hammer) and CI/CD pipelines.

---

## Live Visual Showcase

Explore side-by-side visual comparisons, palette swatches, and spatial mipmaps automatically generated on every commit to `main`:

&rarr; **[https://urgorri.github.io/fastwad/](https://urgorri.github.io/fastwad/)**

---

## Features

- **100% Cross-Platform:** Native support for Windows (MSVC) and Linux (GCC / Clang) with portable UTF-8 path handling.
- **Bit-for-Bit Determinism:** Identical input files and options always produce the exact same binary WAD output across all platforms.
- **High-Fidelity Color Quantization:** Optimized K-Means++ 256-color clustering with configurable Floyd-Steinberg spatial error diffusion dithering.
- **Intelligent Dimension Snapping:** Aspect-ratio containment with automatic 16-pixel alignment and smart 2-pixel micro-stretch snapping to minimize padding artifacts.
- **GoldSrc & Quake Naming Engine:** UTF-8 deaccenting (`fútbol` &rarr; `futbol`), GoldSrc special prefix preservation (`{`, `!`, `+`, `~`), length clamping (15 chars), and deterministic FNV-1a collision hashing.
- **Machine-Readable CLI:** Clean UNIX pipe separation with structured `--json` mode for automated workflows.
- **Embeddable C++17 Header SDK:** Single include (`fastwad/fastwad.hpp`) for in-memory packing, unpacking, inspection, and directory conversion without CLI overhead.
- **Self-Contained & Lightweight:** Offline zero-external-dependency builds with vendored single-file STB image decoders/encoders (~310 KB binary).

---

## Command Line Interface (CLI)

```bash
fastwad <command> <input> <output> [options]
```

### Commands

| Command | Arguments | Description |
| :--- | :--- | :--- |
| `build` | `<input_dir> <output.wad>` | Compiles images into a GoldSrc (WAD3) or Quake (WAD2) archive. |
| `list` | `<input.wad>` | Lists metadata and dimensions of all lumps inside the archive. |
| `extract` | `<input.wad> <output_dir>` | Extracts textures from the WAD into individual PNG or BMP images. |
| `save-config` | `[options]` | Saves current CLI arguments to `fastwad.conf` as default presets. |
| `reset-config` | | Deletes `fastwad.conf` to restore factory defaults. |

### Options

| Flag | Format | Default | Description |
| :--- | :--- | :--- | :--- |
| `--json` | Flag | `false` | Emits structured JSON to `stdout` for programmatic integration. |
| `wad2=true` | `--wad2` | `false` | Generates Quake 1 WAD2 format instead of GoldSrc WAD3. |
| `allow_overwrite=true` | `--allow_overwrite` | `false` | Overwrites existing output files without prompting. |
| `disable_dither=true` | `--disable_dither` | `false` | Disables Floyd-Steinberg error diffusion dithering. |
| `max_size=512` | `--max_size 512` | `256` | Maximum texture bounding box (256, 512, 1024). |
| `align=center` | `--align center` | `center` | Padding alignment (`center`, `top`, `bottom`, `left`, `right`, `top-left`, etc.). |
| `stretch=true` | `--stretch` | `false` | Forces image stretching to `max_size` instead of aspect containment. |
| `pad_r`, `pad_g`, `pad_b` | `--pad_r 0` | `0 0 255` | RGB values for padding and transparency key color (index 255). |
| `format=bmp` | `--format bmp` | `png` | Image extraction format (`png` or `bmp`). |
| `config=<path>` | `--config <path>` | - | Loads custom settings profile from specified configuration file. |
| `--verbose`, `-v` | Flag | `false` | Enables diagnostic log output to `stderr`. |
| `--quiet`, `-q` | Flag | `false` | Suppresses standard informational logs. |

---

## Machine-Readable JSON Mode

When passing `--json`, `fastwad` guarantees structured JSON on `stdout` and diagnostic logs on `stderr`.

### Example: Inspecting an Archive
```bash
fastwad list ./halflife.wad --json | jq .
```
```json
{
  "format": "WAD3",
  "archive": "./halflife.wad",
  "num_lumps": 2,
  "info_table_offset": 65576,
  "lumps": [
    {
      "name": "c2a5_flr1a",
      "type": 67,
      "type_name": "MipTex",
      "offset": 12,
      "disk_size": 32800,
      "size": 32800,
      "width": 128,
      "height": 128,
      "transparent": false,
      "mip_offsets": [40, 16424, 20520, 21544],
      "palette_offset": 21802
    }
  ]
}
```

### Example: Building an Archive with Build Metrics
```bash
fastwad build ./my_textures ./out.wad --json
```
```json
{
  "status": "success",
  "exit_code": 0,
  "archive": "./out.wad",
  "format": "WAD3",
  "total_processed": 14,
  "skipped": 0,
  "failed": 0,
  "textures": [
    {
      "input": "./my_textures/wall_brick.png",
      "lump_name": "wall_brick",
      "width": 128,
      "height": 128,
      "transparent": false,
      "disk_size": 22708
    }
  ]
}
```

---

## Embeddable C++17 Header SDK

You can link `fastwad_core` into your C++ projects or include `fastwad/fastwad.hpp` for in-memory buffer processing without invoking CLI subprocesses.

```cpp
#include <fastwad/fastwad.hpp>
#include <iostream>
#include <fstream>
#include <vector>

int main() {
    // 1. Inspect an archive in memory
    std::ifstream file("halflife.wad", std::ios::binary);
    std::vector<uint8_t> wad_bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    auto wad_info = fastwad::InspectWadFromMemory(wad_bytes.data(), wad_bytes.size());
    if (wad_info) {
        std::cout << "Lumps in WAD: " << wad_info->num_lumps << "\n";
    }

    // 2. Unpack textures into in-memory structs
    auto textures = fastwad::UnpackWadFromMemory(wad_bytes.data(), wad_bytes.size());
    if (textures) {
        for (const auto& tex : *textures) {
            std::cout << "Texture: " << tex.name << " (" << tex.width << "x" << tex.height << ")\n";
        }

        // 3. Repack back into bit-for-bit WAD3 binary stream
        std::vector<uint8_t> repacked = fastwad::PackWadToMemory(*textures, fastwad::WadFormat::WAD3);
    }

    // 4. One-call directory conversion
    fastwad::TextureOptions opts;
    opts.disable_dither = false;
    opts.max_size = 512;
    fastwad::BuildResult res = fastwad::BuildWadFromDirectory("./assets", "./custom.wad", fastwad::WadFormat::WAD3, opts);

    return 0;
}
```

---

## Exit Codes

| Code | Status | Meaning |
| :--- | :--- | :--- |
| `0` | **Success** | Task completed perfectly with zero errors. |
| `1` | **Partial Success / CLI Error** | WAD built with skipped files or invalid arguments provided. |
| `2` | **Fatal Error** | I/O failure, corrupt archive signature, or unreadable source file. |

---

## Building from Source

### Prerequisites
- CMake 3.14+
- C++17 compatible compiler:
  - **Windows:** MSVC (Visual Studio 2019/2022)
  - **Linux:** GCC 10+ or Clang 11+

### Build Steps
```bash
# 1. Clone the repository
git clone https://github.com/urgorri/fastwad.git
cd fastwad

# 2. Configure CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Compile binaries
cmake --build build --config Release -j

# 4. Run automated test suite
ctest --test-dir build -C Release --output-on-failure
```

---

## License

`fastwad` is released under the **MIT License**. Third-party libraries (`stb_image`, `stb_image_resize2`, `stb_image_write`) are in the Public Domain / MIT License.
