# Prehistorik 2 - C++ SDL2 Port

A complete C++ port of the Prehistorik 2 level viewer, using SDL2 for graphics.

## Features

- **Full SQZ Decompression**: LZW, Huffman RLE, and DIET algorithms
- **Level Viewer**: View all 16 levels with smooth scrolling
- **All Game Screens**: TITUS, MENU, CASTLE, THEEND, CREDITS, GAMEOVER
- **Easter Eggs**: Year display, Developer photo
- **Asset Export Tools**: BMP, TSX (Tiled), sprite sheets, raw data
- **Audio Support**: Optional SDL2_mixer for music playback

## Requirements

- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.10+
- SDL2

### macOS
```bash
brew install cmake sdl2
```

### Ubuntu/Debian
```bash
sudo apt install cmake libsdl2-dev
```

### Windows
Download SDL2 development libraries from https://libsdl.org

## Building

```bash
# Clone and enter directory
cd pre2-cpp

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
make
```

## Game Assets

Copy the original game files to the `sqz/` directory in the build folder:

```
build/
├── pre2          # executable
├── sqz/          # game assets (you provide these)
│   ├── TITUS.SQZ
│   ├── MENU.SQZ
│   ├── LEVEL1.SQZ
│   ├── LEVEL2.SQZ
│   ├── ... (all .SQZ files)
│   ├── BOULA.TRK
│   └── ... (all .TRK files)
└── res/          # palette files (copied from Pre2/res/)
    ├── levels.pals
    ├── credits.pal
    ├── sprites.txt
    └── ...
```

## Running

```bash
cd build
./pre2
```

## Controls

| Key | Action |
|-----|--------|
| **←→↑↓** | Scroll (with inertia) |
| **+/=** | Volume Up |
| **-** | Volume Down |
| **Page Up** | Previous level |
| **Page Down** | Next level |
| **1-9, A-G** | Jump to specific level |
| **Space/Enter** | Continue from screens |
| **M** | Show Menu |
| **C** | Show Credits |
| **E** | Show TheEnd |
| **G** | Show GameOver |
| **ESC** | Quit |

## API Reference

### Loading Assets

```cpp
#include "asset_converter.h"

// Initialize paths
assets::set_sqz_path("sqz");
assets::load_level_palettes("res");

// Load a level
auto level = assets::get_level_data(0);  // Level 1

// Load screens
auto titus = assets::get_titus_bitmap();
auto menu = assets::get_menu_bitmap();
auto credits = assets::get_credits_bitmap();

// Load tiles
auto union_tiles = assets::get_union_tiles();
auto front_tiles = assets::get_front_tiles();

// Load sprites
auto sprites = assets::get_sprites();

// Load music track data
auto track = assets::get_level_track(0);
```

### Export Tools

```cpp
#include "asset_converter.h"

// Export everything to a cache directory
assets::prepare_all_assets("cache");

// Or export individually:
auto image = assets::get_titus_bitmap();
assets::write_bmp("titus.bmp", image);

auto tiles = assets::get_front_tiles();
auto palette = assets::get_level_palette(0);
assets::generate_tileset(tiles, palette, 16, "output", "FRONT");
// Creates: output/FRONT.bmp and output/FRONT.tsx

assets::export_raw_sqz("SAMPLE", "output");  // Creates: output/SAMPLE.BIN
```

### Data Structures

```cpp
struct Palette {
    std::array<uint8_t, 256 * 3> colors;
    uint8_t r(int i), g(int i), b(int i);
};

struct Image {
    int width, height;
    std::vector<uint8_t> pixels;  // 8bpp indexed
    Palette palette;
};

struct Tileset {
    int tile_width, tile_height, num_tiles;
    std::vector<std::vector<uint8_t>> tiles;
};

struct LevelData {
    Tilemap tilemap;
    Tileset local_tiles;
    Palette palette;
    std::vector<uint8_t> descriptors;
};
```

## Project Structure

```
pre2-cpp/
├── CMakeLists.txt
├── README.md
└── src/
    ├── main.cpp            # Entry point, game loop
    ├── sqz_unpacker.h/cpp  # SQZ decompression (LZW/Huffman/DIET)
    ├── asset_converter.h/cpp # Asset loading & export
    ├── renderer.h/cpp      # SDL2 rendering
    └── audio.h/cpp         # SDL2_mixer audio (optional)
```

## License

This is a fan project for educational purposes. Prehistorik 2 is © Titus Interactive.

## Credits

- Original game by Titus Interactive (1993)
- C++ port using SDL2
