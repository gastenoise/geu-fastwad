# fastWAD

fastWAD is a professional, standalone CLI tool for building, listing, and extracting GoldSrc-compatible (Half-Life 1) WAD archives. It prioritizes deterministic output, high visual fidelity, and strict compliance with the WAD2/WAD3 format.

## Features
*   **Build:** Create valid WAD2 or WAD3 archives from a directory of images (PNG, JPEG, BMP, GIF, WEBP, TGA).
*   **List:** View detailed metadata (dimensions, format, names) of existing WAD archives.
*   **Extract:** Revert textures from a WAD into high-quality PNG or BMP files.
*   **Configuration Management:** Save and reset persistent settings via `fastwad.conf`.
*   **Visual Fidelity:** Per-texture 256-color K-Means++ quantization and optional Floyd-Steinberg dithering.
*   **Transparency:** Full support for transparent inputs, mapping alpha to the GoldSrc-standard index 255 (color 0,0,255).
*   **Deterministic:** Identical inputs and flags always produce the same bit-identical output.
*   **Portable:** Single static Windows executable with no external dependencies (targets Windows 7+).

## Usage
`fastwad <command> <input> <output> [options]`

Options can be provided as `--key value` flags or as `key=value` assignments. fastWAD automatically loads `fastwad.conf` from the same directory if it exists.

### Commands
*   `build <input_dir> <output.wad>`: Converts images in `input_dir` to a WAD archive.
*   `list <input.wad>`: Lists all textures and their metadata in the WAD.
*   `extract <input.wad> <output_dir>`: Extracts all textures to the specified directory.
*   `save-config [options]`: Saves the current CLI settings to `fastwad.conf`.
*   `reset-config`: Deletes the `fastwad.conf` file to revert to hardcoded defaults.

### Options
| Option | Default | Description |
| :--- | :--- | :--- |
| `wad2=true` | `false` | Generate WAD2 (Quake) instead of WAD3 (Half-Life). |
| `allow_overwrite=true` | `false` | Overwrite existing output files. |
| `disable_dither=true` | `false` | Disable Floyd-Steinberg dithering. |
| `max_size=512` | `256` | Set maximum texture dimension (256, 512, 1024). |
| `align=center` | `center` | Alignment for padding (center, top, bottom, left, right, top-left, etc.). |
| `stretch=true` | `false` | Force stretch to `max_size` (ignores aspect ratio). |
| `pad_r=0` | `0` | Red component of the padding/transparency key color. |
| `pad_g=0` | `0` | Green component of the padding/transparency key color. |
| `pad_b=255` | `255` | Blue component of the padding/transparency key color. |
| `format=bmp` | `png` | Extraction output format (`png` or `bmp`). |
| `config=<path>` | - | Load settings from a specific configuration file. |
| `--save-config=<path>` | - | Export current settings to a specified file path. |
| `verbose=true` | `false` | Enable detailed logging of the conversion process. |
| `quiet=true` | `false` | Suppress all standard output except errors. |

## Technical Rules
*   **Sizing & Fit:**
    *   Images are scaled to fit within the `max_size` while preserving aspect ratio (contain).
    *   Final dimensions are snapped to the nearest multiple-of-16.
    *   **Micro-stretch:** If the scaled image is within 2 pixels of the snapped dimension, it is stretched to fill; otherwise, it is padded with the key color.
*   **Naming:**
    *   All names are normalized to lowercase alphanumeric (plus `_` and `-`).
    *   Unicode characters are deaccented (e.g., `á -> a`).
    *   GoldSrc prefixes (`{`, `!`, `+`, `~`) are preserved if they are the first character.
    *   Names are truncated to 15 characters.
    *   Collisions are resolved deterministically using an FNV-1a 32-bit hex suffix.
*   **Transparency:**
    *   Full transparency (alpha 0) is mapped to index 255.
    *   Semi-transparency is blended against the padding color before quantization.
*   **Animation:** Animated GIFs or WebP files are automatically detected and skipped to maintain standard WAD compatibility.
*   **Output:** Generates full mipmaps (4 levels) and a 256-color palette for each texture.

## Build Instructions
fastWAD uses CMake and targets Windows with the MSVC compiler for static runtime linking.

1.  **Clone and Configure:**
    ```cmd
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    ```
2.  **Build:**
    ```cmd
    cmake --build . --config Release
    ```
3.  **Run Tests:**
    ```cmd
    ctest -C Release
    ```
    Alternatively, run the test executable directly:
    ```cmd
    ./Release/fastwad_tests
    ```

## Exit Codes
*   `0`: **Success** - Task completed perfectly.
*   `1`: **Partial Success** - Archive created, but some files were skipped or warnings were issued.
*   `2`: **Fatal Error** - IO failure, invalid format, or write error.
*   `3`: **CLI Error** - Invalid arguments or missing paths.
