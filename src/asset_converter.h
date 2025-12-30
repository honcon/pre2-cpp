#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <array>

namespace assets {

// Palette: 256 colors, each with R, G, B
struct Palette {
    std::array<uint8_t, 256 * 3> colors;
    
    uint8_t r(int i) const { return colors[i * 3]; }
    uint8_t g(int i) const { return colors[i * 3 + 1]; }
    uint8_t b(int i) const { return colors[i * 3 + 2]; }
};

// Image data with indexed pixels
struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;  // 8bpp indexed
    Palette palette;
};

// Sprite entry from sprites.txt
struct SpriteEntry {
    int x, y, w, h;
};

// Sprite data
struct Spriteset {
    std::vector<SpriteEntry> entries;
    std::vector<std::vector<uint8_t>> sprites;  // Each sprite pixels
};

// Tile data
struct Tileset {
    int tile_width = 16;
    int tile_height = 16;
    int num_tiles = 0;
    std::vector<std::vector<uint8_t>> tiles;
};

// Level tilemap
struct Tilemap {
    int width = 256;
    int height = 0;
    std::vector<uint8_t> map;
    std::vector<uint16_t> lut;
};

// Level data
struct LevelData {
    Tilemap tilemap;
    Tileset local_tiles;
    Palette palette;
    std::vector<uint8_t> descriptors;
};

// Number of levels
constexpr int NUM_LEVELS = 16;

// Initialize asset paths
void set_sqz_path(const std::string& path);
void set_res_path(const std::string& path);

// Load level palettes from res/levels.pals
void load_level_palettes(const std::string& res_path);

// Get background image for a level
Image get_level_background(int level_idx);

// Get level data
LevelData get_level_data(int level_idx);

// Get palette for a level
Palette get_level_palette(int level_idx);

// Screen images
Image get_titus_bitmap();
Image get_menu_bitmap();
Image get_castle_bitmap();
Image get_theend_bitmap();
Image get_map_bitmap();
Image get_gameover_bitmap();

// Easter egg screens
Image get_credits_bitmap();
Image get_year_bitmap();
Image get_dev_photo();

// Load union tiles
Tileset get_union_tiles();

// Load front tiles
Tileset get_front_tiles();

// Load sprites
Spriteset get_sprites();

// Music track data (raw TRK file)
std::vector<uint8_t> get_level_track(int level_idx);
std::vector<uint8_t> get_intro_track();
std::vector<uint8_t> get_menu_track();
std::vector<uint8_t> get_gameover_track();
std::vector<uint8_t> get_boss_track();
std::vector<uint8_t> get_bravo_track();
std::vector<uint8_t> get_motif_track();

// ============================================================================
// Export Tools
// ============================================================================

// Write image as BMP file (simpler than PNG, no external deps)
bool write_bmp(const std::string& filename, const Image& image);

// Write image as raw indexed pixels
bool write_raw(const std::string& filename, const std::vector<uint8_t>& data);

// Generate tileset image and TSX file for Tiled editor
bool generate_tileset(const Tileset& tiles, const Palette& palette, 
                      int tiles_per_row, const std::string& out_path, 
                      const std::string& base_name);

// Write TSX (Tiled tileset) file
bool write_tsx(const std::string& base_name, const std::string& out_path,
               int tile_w, int tile_h, int image_w, int image_h);

// Generate sprite sheet image
Image generate_spritesheet(const Spriteset& sprites, const Palette& palette,
                           int sheet_width, int sheet_height);

// Convert and export all fonts
bool export_fonts(const std::string& out_path);

// Convert title screen (PRESENT.SQZ) to background and foreground
bool convert_title(const std::string& resource, const std::string& out_path);

// Prepare all assets to cache directory
bool prepare_all_assets(const std::string& cache_dir);

// Export raw unpacked SQZ data
bool export_raw_sqz(const std::string& name, const std::string& out_path);

} // namespace assets
