# Building Prehistorik 2 C++ Port

## Quick Start (macOS)

```bash
# Install dependencies
brew install cmake sdl2

# Build
cd pre2-cpp
mkdir build && cd build
cmake ..
make

# Setup assets
cp -r ../../Pre2/bin/Release/net9.0/osx-arm64/sqz .
cp -r ../../Pre2/res .

# Run
./pre2
```

## Quick Start (Linux)

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install cmake libsdl2-dev

# Build
cd pre2-cpp
mkdir build && cd build
cmake ..
make

# Setup assets
cp -r /path/to/sqz .
cp -r /path/to/res .

# Run
./pre2
```

## Quick Start (Windows)

1. Install Visual Studio 2019+ with C++ Desktop Development
2. Install CMake: https://cmake.org/download/
3. Download SDL2 development libraries: https://github.com/libsdl-org/SDL/releases
4. Set `SDL2_DIR` environment variable to SDL2 cmake folder

```cmd
cd pre2-cpp
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## Required Files

### sqz/ folder
Copy all `.SQZ` and `.TRK` files from the original Prehistorik 2 game:
- `TITUS.SQZ`, `MENU.SQZ`, `CASTLE.SQZ`, `THEEND.SQZ`
- `LEVEL1.SQZ` through `LEVELG.SQZ`
- `BACK0.SQZ` through `BACK5.SQZ`
- `FRONT.SQZ`, `UNION.SQZ`, `SPRITES.SQZ`
- `ALLFONTS.SQZ`, `SAMPLE.SQZ`, `KEYB.SQZ`
- All `.TRK` music files

### res/ folder
Copy the palette and sprite info files from `Pre2/res/`:
- `levels.pals` - Level palettes
- `credits.pal`, `map.pal`, `gameover.pal`, `menu2.pal`, `motif.pal`
- `sprites.txt` - Sprite dimensions

## Build Options

### Enable SDL2_mixer for Audio (optional)
```bash
brew install sdl2_mixer  # macOS
sudo apt install libsdl2-mixer-dev  # Linux

cmake ..  # Automatically detects SDL2_mixer
```

### Debug Build
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

### Release Build
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## Troubleshooting

### "Cannot open: sqz/TITUS.SQZ"
Copy the game assets to the `sqz/` folder in the build directory.

### "Cannot open: res/levels.pals"
Copy the `res/` folder from the Pre2 project.

### SDL2 not found
```bash
# macOS
brew install sdl2

# Linux
sudo apt install libsdl2-dev
```

### CMake too old
```bash
# macOS
brew install cmake

# Linux
sudo apt install cmake
```
