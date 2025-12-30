#include "asset_converter.h"
#include "sqz_unpacker.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <ctime>
#include <sys/stat.h>

namespace assets {

// ============================================================================
// Constants
// ============================================================================

static const int TILE_SIDE = 16;
static const int LEVEL_TILES_PER_ROW = 256;
static const int NUM_UNION_TILES = 544;
static const int NUM_FRONT_TILES = 163;
static const int NUM_SPRITES = 460;

// Font constants
static const int FONT_CREDITS_W = 8;
static const int FONT_CREDITS_H = 12;
static const int NUM_FONT_CREDITS_CHARS = 41;
static const char FONT_CREDITS_CHARS[] = "0123456789!?.$_ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static const char LEVEL_SUFFIXES[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 'G'};
static const int LEVEL_NUM_ROWS[] = {49, 104, 49, 45, 128, 128, 128, 86, 110, 12, 24, 51, 51, 38, 173, 84};
static const int LEVEL_PALS[] = {8, 10, 7, 6, 3, 5, 1, 4, 2, 2, 11, 11, 11, 12, 2, 1};
static const char BACK_SUFFIXES[] = {'0', '0', '0', '1', '1', '1', '2', '3', '3', '0', '4', '4', '4', '5', '0', '2'};

// Track indices for each level
enum class Track { Boula, Bravo, Carte, Code, Final, Glace, Kool, Mines, Monster, Mystery, Pres, Presenta };
static const Track LEVEL_TRACKS[] = {
    Track::Mines, Track::Mines, Track::Pres, Track::Pres, Track::Pres, Track::Monster,
    Track::Glace, Track::Glace, Track::Mystery, Track::Monster, Track::Kool, Track::Kool,
    Track::Kool, Track::Mines, Track::Final, Track::Glace
};

static const char* TRACK_NAMES[] = {
    "BOULA", "BRAVO", "CARTE", "CODE", "FINAL", "GLACE", 
    "KOOL", "MINES", "MONSTER", "MYSTERY", "PRES", "PRESENTA"
};

static std::string g_sqz_path = "sqz";
static std::string g_res_path = "res";
static std::vector<Palette> g_level_palettes;
static Tileset g_union_tiles;
static Tileset g_front_tiles;
static Spriteset g_sprites;
static std::vector<std::vector<uint8_t>> g_font_credits;
static bool g_initialized = false;
static bool g_fonts_loaded = false;

// ============================================================================
// Utility Functions
// ============================================================================

static uint8_t vga_to_rgb(uint8_t six_bit) {
    six_bit &= 0x3F;
    return (six_bit << 2) | (six_bit >> 4);
}

static std::vector<uint8_t> convert_planar_to_linear(const std::vector<uint8_t>& data) {
    if (data.size() % 4 != 0) {
        throw std::runtime_error("Data size must be multiple of 4");
    }
    
    std::vector<uint8_t> result(data.size());
    size_t plane_length = data.size() / 4;
    
    for (size_t i = 0; i < plane_length; i++) {
        uint8_t b0 = data[plane_length * 0 + i];
        uint8_t b1 = data[plane_length * 1 + i];
        uint8_t b2 = data[plane_length * 2 + i];
        uint8_t b3 = data[plane_length * 3 + i];
        
        for (int j = 0; j < 4; j++) {
            int hi = ((b3 & 0x80) >> 0) | ((b2 & 0x80) >> 1) | 
                     ((b1 & 0x80) >> 2) | ((b0 & 0x80) >> 3);
            int lo = ((b3 & 0x40) >> 3) | ((b2 & 0x40) >> 4) | 
                     ((b1 & 0x40) >> 5) | ((b0 & 0x40) >> 6);
            
            result[i * 4 + j] = static_cast<uint8_t>(hi | lo);
            
            b0 <<= 2; b1 <<= 2; b2 <<= 2; b3 <<= 2;
        }
    }
    
    return result;
}

static std::vector<uint8_t> convert_4bpp_to_8bpp(const std::vector<uint8_t>& packed) {
    std::vector<uint8_t> result(packed.size() * 2);
    
    for (size_t i = 0; i < packed.size(); i++) {
        result[i * 2] = (packed[i] >> 4) & 0x0F;
        result[i * 2 + 1] = packed[i] & 0x0F;
    }
    
    return result;
}

static Palette load_vga_palette(const uint8_t* data, int num_colors) {
    Palette pal;
    pal.colors.fill(0);
    
    for (int i = 0; i < num_colors && i < 256; i++) {
        pal.colors[i * 3 + 0] = vga_to_rgb(data[i * 3 + 0]);
        pal.colors[i * 3 + 1] = vga_to_rgb(data[i * 3 + 1]);
        pal.colors[i * 3 + 2] = vga_to_rgb(data[i * 3 + 2]);
    }
    
    return pal;
}

static Palette load_palette_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        Palette pal;
        pal.colors.fill(0);
        return pal;
    }
    
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    
    return load_vga_palette(data.data(), static_cast<int>(data.size() / 3));
}

static Tileset read_tiles(const std::vector<uint8_t>& data, int num_tiles, int tile_w, int tile_h) {
    Tileset tileset;
    tileset.tile_width = tile_w;
    tileset.tile_height = tile_h;
    tileset.num_tiles = num_tiles;
    
    int bytes_per_tile_planar = tile_w * tile_h / 2;
    
    for (int i = 0; i < num_tiles; i++) {
        int offset = i * bytes_per_tile_planar;
        if (offset + bytes_per_tile_planar > static_cast<int>(data.size())) break;
        
        std::vector<uint8_t> tile_data(data.begin() + offset, 
                                        data.begin() + offset + bytes_per_tile_planar);
        
        auto linear = convert_planar_to_linear(tile_data);
        auto pixels = convert_4bpp_to_8bpp(linear);
        
        tileset.tiles.push_back(pixels);
    }
    
    return tileset;
}

static std::vector<std::vector<uint8_t>> read_tiles_from_stream(
    const std::vector<uint8_t>& data, size_t& offset, int num_tiles, int tile_w, int tile_h) {
    
    std::vector<std::vector<uint8_t>> tiles;
    int bytes_per_tile_planar = tile_w * tile_h / 2;
    
    for (int i = 0; i < num_tiles; i++) {
        if (offset + bytes_per_tile_planar > data.size()) break;
        
        std::vector<uint8_t> tile_data(data.begin() + offset, 
                                        data.begin() + offset + bytes_per_tile_planar);
        offset += bytes_per_tile_planar;
        
        auto linear = convert_planar_to_linear(tile_data);
        auto pixels = convert_4bpp_to_8bpp(linear);
        
        tiles.push_back(pixels);
    }
    
    return tiles;
}

static void load_fonts() {
    if (g_fonts_loaded) return;
    
    try {
        std::string filename = g_sqz_path + "/ALLFONTS.SQZ";
        auto data = sqz::unpack(filename);
        
        size_t offset = 0;
        g_font_credits = read_tiles_from_stream(data, offset, NUM_FONT_CREDITS_CHARS, 
                                                 FONT_CREDITS_W, FONT_CREDITS_H);
        
        g_fonts_loaded = true;
    } catch (...) {}
}

static void draw_font_char(std::vector<uint8_t>& image, int img_width, 
                           int x, int y, const std::vector<uint8_t>& char_pixels,
                           int char_w, int char_h) {
    for (int py = 0; py < char_h; py++) {
        for (int px = 0; px < char_w; px++) {
            int src_idx = py * char_w + px;
            if (src_idx >= static_cast<int>(char_pixels.size())) continue;
            
            uint8_t pixel = char_pixels[src_idx];
            if (pixel == 0) continue;
            
            int dst_x = x + px;
            int dst_y = y + py;
            if (dst_x < 0 || dst_x >= img_width) continue;
            if (dst_y < 0 || dst_y >= 200) continue;
            
            int dst_idx = dst_y * img_width + dst_x;
            if (dst_idx < static_cast<int>(image.size())) {
                image[dst_idx] = pixel;
            }
        }
    }
}

static void draw_credits_line(std::vector<uint8_t>& image, int x, int y, const std::string& text) {
    load_fonts();
    if (g_font_credits.empty()) return;
    
    int col = 0;
    for (char c : text) {
        const char* pos = strchr(FONT_CREDITS_CHARS, c);
        if (pos) {
            int idx = static_cast<int>(pos - FONT_CREDITS_CHARS);
            if (idx >= 0 && idx < static_cast<int>(g_font_credits.size())) {
                int dst_x = x + col * FONT_CREDITS_W;
                draw_font_char(image, 320, dst_x, y, g_font_credits[idx], 
                               FONT_CREDITS_W, FONT_CREDITS_H);
            }
        }
        col++;
    }
}

// Copy pixels from source to destination
static void copy_pixels(const std::vector<uint8_t>& src, int src_w, int src_h,
                        std::vector<uint8_t>& dst, int dst_w, int dst_x, int dst_y) {
    for (int y = 0; y < src_h; y++) {
        for (int x = 0; x < src_w; x++) {
            int src_idx = y * src_w + x;
            if (src_idx >= static_cast<int>(src.size())) continue;
            
            int dx = dst_x + x;
            int dy = dst_y + y;
            int dst_idx = dy * dst_w + dx;
            
            if (dst_idx >= 0 && dst_idx < static_cast<int>(dst.size())) {
                dst[dst_idx] = src[src_idx];
            }
        }
    }
}

static int divide_round_up(int a, int b) {
    return (a + b - 1) / b;
}

static void create_directory(const std::string& path) {
    mkdir(path.c_str(), 0755);
}

// ============================================================================
// Public Functions
// ============================================================================

void set_sqz_path(const std::string& path) {
    g_sqz_path = path;
}

void set_res_path(const std::string& path) {
    g_res_path = path;
}

void load_level_palettes(const std::string& res_path) {
    g_res_path = res_path;
    
    std::string filename = res_path + "/levels.pals";
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open: " + filename);
    }
    
    const int num_palettes = 13;
    const int bytes_per_palette = 3 * 16;
    
    g_level_palettes.resize(num_palettes);
    
    for (int i = 0; i < num_palettes; i++) {
        uint8_t pal_data[bytes_per_palette];
        file.read(reinterpret_cast<char*>(pal_data), bytes_per_palette);
        g_level_palettes[i] = load_vga_palette(pal_data, 16);
    }
    
    g_initialized = true;
}

Palette get_level_palette(int level_idx) {
    if (!g_initialized) {
        load_level_palettes(g_res_path);
    }
    
    int pal_idx = LEVEL_PALS[level_idx % NUM_LEVELS];
    if (pal_idx >= 0 && pal_idx < static_cast<int>(g_level_palettes.size())) {
        return g_level_palettes[pal_idx];
    }
    return g_level_palettes[0];
}

Tileset get_union_tiles() {
    if (g_union_tiles.num_tiles == 0) {
        std::string filename = g_sqz_path + "/UNION.SQZ";
        auto data = sqz::unpack(filename);
        g_union_tiles = read_tiles(data, NUM_UNION_TILES, TILE_SIDE, TILE_SIDE);
    }
    return g_union_tiles;
}

Tileset get_front_tiles() {
    if (g_front_tiles.num_tiles == 0) {
        std::string filename = g_sqz_path + "/FRONT.SQZ";
        auto data = sqz::unpack(filename);
        g_front_tiles = read_tiles(data, NUM_FRONT_TILES, TILE_SIDE, TILE_SIDE);
    }
    return g_front_tiles;
}

Image get_level_background(int level_idx) {
    char suffix = BACK_SUFFIXES[level_idx % NUM_LEVELS];
    std::string filename = g_sqz_path + "/BACK" + suffix + ".SQZ";
    
    auto data = sqz::unpack(filename);
    
    const int width = 320;
    const int height = 200;
    const int expected_size = width * height / 2;
    
    if (data.size() < static_cast<size_t>(expected_size)) {
        data.resize(expected_size, 0);
    }
    
    auto linear = convert_planar_to_linear(std::vector<uint8_t>(data.begin(), data.begin() + expected_size));
    auto pixels = convert_4bpp_to_8bpp(linear);
    
    Image img;
    img.width = width;
    img.height = height;
    img.pixels = pixels;
    img.palette = get_level_palette(level_idx);
    
    return img;
}

LevelData get_level_data(int level_idx) {
    char suffix = LEVEL_SUFFIXES[level_idx % NUM_LEVELS];
    std::string filename = g_sqz_path + "/LEVEL" + suffix + ".SQZ";
    
    auto data = sqz::unpack(filename);
    
    LevelData level;
    int num_rows = LEVEL_NUM_ROWS[level_idx % NUM_LEVELS];
    int tilemap_length = num_rows * LEVEL_TILES_PER_ROW;
    
    level.tilemap.width = LEVEL_TILES_PER_ROW;
    level.tilemap.height = num_rows;
    level.tilemap.map.assign(data.begin(), data.begin() + tilemap_length);
    
    size_t lut_offset = tilemap_length;
    level.tilemap.lut.resize(256);
    for (int i = 0; i < 256; i++) {
        level.tilemap.lut[i] = data[lut_offset + i * 2] | (data[lut_offset + i * 2 + 1] << 8);
    }
    
    int max_local_idx = -1;
    for (int i = 0; i < 256; i++) {
        uint16_t v = level.tilemap.lut[i];
        if (v < 256 && static_cast<int>(v) > max_local_idx) {
            max_local_idx = v;
        }
    }
    
    size_t tiles_offset = lut_offset + 512;
    int num_local_tiles = max_local_idx + 1;
    int bytes_per_tile = TILE_SIDE * TILE_SIDE / 2;
    
    std::vector<uint8_t> tiles_data(data.begin() + tiles_offset, 
                                     data.begin() + tiles_offset + num_local_tiles * bytes_per_tile);
    level.local_tiles = read_tiles(tiles_data, num_local_tiles, TILE_SIDE, TILE_SIDE);
    
    size_t desc_offset = tiles_offset + num_local_tiles * bytes_per_tile;
    if (desc_offset + 5029 <= data.size()) {
        level.descriptors.assign(data.begin() + desc_offset, data.begin() + desc_offset + 5029);
    }
    
    level.palette = get_level_palette(level_idx);
    
    return level;
}

static Image get_index8_with_palette(const std::string& name) {
    std::string filename = g_sqz_path + "/" + name + ".SQZ";
    auto data = sqz::unpack(filename);
    
    const int width = 320;
    const int height = 200;
    
    Image img;
    img.width = width;
    img.height = height;
    img.palette = load_vga_palette(data.data(), 256);
    img.pixels.assign(data.begin() + 768, data.begin() + 768 + width * height);
    
    return img;
}

static Image get_index4_with_palette(const std::string& name, const std::string& pal_file) {
    std::string filename = g_sqz_path + "/" + name + ".SQZ";
    auto data = sqz::unpack(filename);
    
    const int width = 320;
    const int height = 200;
    const int expected_size = width * height / 2;
    
    if (data.size() < static_cast<size_t>(expected_size)) {
        data.resize(expected_size, 0);
    }
    
    auto linear = convert_planar_to_linear(std::vector<uint8_t>(data.begin(), data.begin() + expected_size));
    auto pixels = convert_4bpp_to_8bpp(linear);
    
    Image img;
    img.width = width;
    img.height = height;
    img.pixels = pixels;
    img.palette = load_palette_file(g_res_path + "/" + pal_file);
    
    return img;
}

Image get_titus_bitmap() { return get_index8_with_palette("TITUS"); }
Image get_menu_bitmap() { return get_index8_with_palette("MENU"); }
Image get_castle_bitmap() { return get_index8_with_palette("CASTLE"); }
Image get_theend_bitmap() { return get_index8_with_palette("THEEND"); }
Image get_map_bitmap() { return get_index4_with_palette("MAP", "map.pal"); }
Image get_gameover_bitmap() { return get_index4_with_palette("GAMEOVER", "gameover.pal"); }

Image get_credits_bitmap() {
    const int width = 320;
    const int height = 200;
    
    Image img;
    img.width = width;
    img.height = height;
    img.pixels.resize(width * height, 0);
    img.palette = load_palette_file(g_res_path + "/credits.pal");
    
    int w = FONT_CREDITS_W;
    int h = FONT_CREDITS_H;
    
    draw_credits_line(img.pixels,  1 * w,  8 +  0 * h, "CODER. DESIGNER AND ARTIST DIRECTOR.");
    draw_credits_line(img.pixels, 14 * w, 10 +  1 * h, "ERIC ZMIRO");
    draw_credits_line(img.pixels,  4 * w,  2 +  4 * h, ".MAIN GRAPHICS AND BACKGROUND.");
    draw_credits_line(img.pixels, 11 * w,  4 +  5 * h, "FRANCIS FOURNIER");
    draw_credits_line(img.pixels,  9 * w,  8 +  7 * h, ".MONSTERS AND HEROS.");
    draw_credits_line(img.pixels, 11 * w, 10 +  8 * h, "LYES  BELAIDOUNI");
    draw_credits_line(img.pixels, 15 * w,  6 + 12 * h, "THANKS TO");
    draw_credits_line(img.pixels,  2 * w,  0 + 14 * h, "CRISTELLE. GIL ESPECHE AND CORINNE.");
    draw_credits_line(img.pixels,  0 * w,  0 + 15 * h, "SEBASTIEN BECHET AND OLIVIER AKA DELTA.");
    
    return img;
}

Image get_year_bitmap() {
    const int width = 320;
    const int height = 200;
    
    Image img;
    img.width = width;
    img.height = height;
    img.pixels.resize(width * height, 0);
    img.palette = load_palette_file(g_res_path + "/credits.pal");
    
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    int year = 1900 + tm_info->tm_year;
    
    if (year >= 1996 && year <= 2067) {
        std::string year_str = std::to_string(year);
        int x = (width - static_cast<int>(year_str.length()) * FONT_CREDITS_W) / 2;
        int y = height / 2 - FONT_CREDITS_H / 2;
        draw_credits_line(img.pixels, x, y, year_str);
    }
    
    return img;
}

Image get_dev_photo() {
    std::string filename_h = g_sqz_path + "/LEVELH.SQZ";
    std::string filename_i = g_sqz_path + "/LEVELI.SQZ";
    
    auto planes01 = sqz::unpack(filename_h);
    auto planes02 = sqz::unpack(filename_i);
    
    std::vector<uint8_t> planes;
    planes.insert(planes.end(), planes01.begin(), planes01.end());
    planes.insert(planes.end(), planes02.begin(), planes02.end());
    
    auto linear = convert_planar_to_linear(planes);
    auto pixels = convert_4bpp_to_8bpp(linear);
    
    const int width = 640;
    const int height = 480;
    pixels.resize(width * height, 0);
    
    Image img;
    img.width = width;
    img.height = height;
    img.pixels = pixels;
    
    for (int i = 0; i < 16; i++) {
        uint8_t c = vga_to_rgb(static_cast<uint8_t>(i * 4));
        img.palette.colors[i * 3 + 0] = c;
        img.palette.colors[i * 3 + 1] = c;
        img.palette.colors[i * 3 + 2] = c;
    }
    
    return img;
}

Spriteset get_sprites() {
    if (g_sprites.sprites.empty()) {
        std::string txt_file = g_res_path + "/sprites.txt";
        std::ifstream file(txt_file);
        if (!file) {
            throw std::runtime_error("Cannot open: " + txt_file);
        }
        
        g_sprites.entries.resize(NUM_SPRITES);
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            std::istringstream iss(line);
            int idx;
            char eq;
            int x, y, w, h;
            if (iss >> idx >> eq >> x >> y >> w >> h) {
                if (idx >= 0 && idx < NUM_SPRITES) {
                    g_sprites.entries[idx] = {x, y, w, h};
                }
            }
        }
        
        std::string sqz_file = g_sqz_path + "/SPRITES.SQZ";
        auto data = sqz::unpack(sqz_file);
        
        size_t offset = 0;
        for (int i = 0; i < NUM_SPRITES; i++) {
            const auto& entry = g_sprites.entries[i];
            int bytes_planar = entry.w * entry.h / 2;
            
            if (offset + bytes_planar > data.size()) break;
            
            std::vector<uint8_t> sprite_data(data.begin() + offset,
                                              data.begin() + offset + bytes_planar);
            
            auto linear = convert_planar_to_linear(sprite_data);
            auto pixels = convert_4bpp_to_8bpp(linear);
            
            g_sprites.sprites.push_back(pixels);
            offset += bytes_planar;
        }
    }
    
    return g_sprites;
}

// ============================================================================
// Music functions
// ============================================================================

static std::vector<uint8_t> get_track_data(const std::string& name) {
    std::string filename = g_sqz_path + "/" + name + ".TRK";
    return sqz::unpack(filename);
}

std::vector<uint8_t> get_level_track(int level_idx) {
    Track track = LEVEL_TRACKS[level_idx % NUM_LEVELS];
    return get_track_data(TRACK_NAMES[static_cast<int>(track)]);
}

std::vector<uint8_t> get_intro_track() { return get_track_data("PRESENTA"); }
std::vector<uint8_t> get_menu_track() { return get_track_data("CARTE"); }
std::vector<uint8_t> get_gameover_track() { return get_track_data("BOULA"); }
std::vector<uint8_t> get_boss_track() { return get_track_data("MONSTER"); }
std::vector<uint8_t> get_bravo_track() { return get_track_data("BRAVO"); }
std::vector<uint8_t> get_motif_track() { return get_track_data("CODE"); }

// ============================================================================
// Export Tools
// ============================================================================

bool write_bmp(const std::string& filename, const Image& image) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;
    
    int row_padding = (4 - (image.width % 4)) % 4;
    int row_size = image.width + row_padding;
    int pixel_data_size = row_size * image.height;
    int palette_size = 256 * 4;
    int header_size = 14 + 40;
    int file_size = header_size + palette_size + pixel_data_size;
    
    // BMP Header
    uint8_t header[54] = {0};
    header[0] = 'B'; header[1] = 'M';
    *reinterpret_cast<uint32_t*>(&header[2]) = file_size;
    *reinterpret_cast<uint32_t*>(&header[10]) = header_size + palette_size;
    *reinterpret_cast<uint32_t*>(&header[14]) = 40;  // DIB header size
    *reinterpret_cast<int32_t*>(&header[18]) = image.width;
    *reinterpret_cast<int32_t*>(&header[22]) = -image.height;  // Top-down
    *reinterpret_cast<uint16_t*>(&header[26]) = 1;   // Planes
    *reinterpret_cast<uint16_t*>(&header[28]) = 8;   // Bits per pixel
    *reinterpret_cast<uint32_t*>(&header[34]) = pixel_data_size;
    
    file.write(reinterpret_cast<char*>(header), 54);
    
    // Palette (BGRA format)
    for (int i = 0; i < 256; i++) {
        uint8_t bgra[4] = {
            image.palette.b(i),
            image.palette.g(i),
            image.palette.r(i),
            0
        };
        file.write(reinterpret_cast<char*>(bgra), 4);
    }
    
    // Pixel data
    std::vector<uint8_t> row(row_size, 0);
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            row[x] = image.pixels[y * image.width + x];
        }
        file.write(reinterpret_cast<char*>(row.data()), row_size);
    }
    
    return true;
}

bool write_raw(const std::string& filename, const std::vector<uint8_t>& data) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

bool write_tsx(const std::string& base_name, const std::string& out_path,
               int tile_w, int tile_h, int image_w, int image_h) {
    std::string filename = out_path + "/" + base_name + ".tsx";
    std::ofstream file(filename);
    if (!file) return false;
    
    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<tileset name=\"" << base_name << "\" tilewidth=\"" << tile_w 
         << "\" tileheight=\"" << tile_h << "\">\n";
    file << "  <image source=\"" << base_name << ".bmp\" width=\"" << image_w 
         << "\" height=\"" << image_h << "\"/>\n";
    file << "</tileset>\n";
    
    return true;
}

bool generate_tileset(const Tileset& tiles, const Palette& palette,
                      int tiles_per_row, const std::string& out_path,
                      const std::string& base_name) {
    if (tiles.tiles.empty()) return false;
    
    int num_tiles = static_cast<int>(tiles.tiles.size());
    int tiles_per_col = divide_round_up(num_tiles, tiles_per_row);
    
    int out_width = tiles.tile_width * tiles_per_row;
    int out_height = tiles.tile_height * tiles_per_col;
    
    Image img;
    img.width = out_width;
    img.height = out_height;
    img.pixels.resize(out_width * out_height, 0);
    img.palette = palette;
    
    for (int row = 0; row < tiles_per_col; row++) {
        for (int col = 0; col < tiles_per_row; col++) {
            int tile_idx = row * tiles_per_row + col;
            if (tile_idx >= num_tiles) break;
            
            copy_pixels(tiles.tiles[tile_idx], tiles.tile_width, tiles.tile_height,
                        img.pixels, out_width, col * tiles.tile_width, row * tiles.tile_height);
        }
    }
    
    std::string bmp_file = out_path + "/" + base_name + ".bmp";
    write_bmp(bmp_file, img);
    write_tsx(base_name, out_path, tiles.tile_width, tiles.tile_height, out_width, out_height);
    
    return true;
}

Image generate_spritesheet(const Spriteset& sprites, const Palette& palette,
                           int sheet_width, int sheet_height) {
    Image img;
    img.width = sheet_width;
    img.height = sheet_height;
    img.pixels.resize(sheet_width * sheet_height, 0);
    img.palette = palette;
    
    for (size_t i = 0; i < sprites.sprites.size() && i < sprites.entries.size(); i++) {
        const auto& entry = sprites.entries[i];
        const auto& pixels = sprites.sprites[i];
        
        copy_pixels(pixels, entry.w, entry.h, img.pixels, sheet_width, entry.x, entry.y);
    }
    
    return img;
}

bool export_fonts(const std::string& out_path) {
    load_fonts();
    if (g_font_credits.empty()) return false;
    
    // Create font tileset
    Tileset font_tiles;
    font_tiles.tile_width = FONT_CREDITS_W;
    font_tiles.tile_height = FONT_CREDITS_H;
    font_tiles.num_tiles = static_cast<int>(g_font_credits.size());
    font_tiles.tiles = g_font_credits;
    
    Palette pal = load_palette_file(g_res_path + "/credits.pal");
    
    return generate_tileset(font_tiles, pal, NUM_FONT_CREDITS_CHARS, out_path, "FONTS");
}

bool convert_title(const std::string& resource, const std::string& out_path) {
    std::string filename = g_sqz_path + "/" + resource + ".SQZ";
    auto data = sqz::unpack(filename);
    
    const int width = 320;
    const int height = 200;
    const int image_size = width * height;
    
    Palette pal = load_vga_palette(data.data(), 256);
    
    // Background
    Image bg;
    bg.width = width;
    bg.height = height;
    bg.pixels.assign(data.begin() + 768, data.begin() + 768 + image_size);
    bg.palette = pal;
    
    // Foreground (offset 0x600 after image)
    Image fg;
    fg.width = width;
    fg.height = height;
    size_t fg_offset = 768 + image_size + 0x600;
    if (fg_offset + image_size <= data.size()) {
        fg.pixels.assign(data.begin() + fg_offset, data.begin() + fg_offset + image_size);
    } else {
        fg.pixels = bg.pixels;
    }
    fg.palette = pal;
    
    write_bmp(out_path + "/" + resource + "_B.bmp", bg);
    write_bmp(out_path + "/" + resource + "_F.bmp", fg);
    
    return true;
}

bool export_raw_sqz(const std::string& name, const std::string& out_path) {
    try {
        auto data = sqz::unpack(g_sqz_path + "/" + name + ".SQZ");
        return write_raw(out_path + "/" + name + ".BIN", data);
    } catch (...) {
        return false;
    }
}

bool prepare_all_assets(const std::string& cache_dir) {
    create_directory(cache_dir);
    
    // Export fonts
    export_fonts(cache_dir);
    
    // Convert 4bpp images with palettes
    auto gameover = get_gameover_bitmap();
    write_bmp(cache_dir + "/GAMEOVER.bmp", gameover);
    
    auto map = get_map_bitmap();
    write_bmp(cache_dir + "/MAP.bmp", map);
    
    // Convert title
    convert_title("PRESENT", cache_dir);
    
    // Generate sprite sheet
    auto sprites = get_sprites();
    auto pal = get_level_palette(0);
    auto sheet = generate_spritesheet(sprites, pal, 640, 480);
    write_bmp(cache_dir + "/SPRITES.bmp", sheet);
    
    // Generate front tileset
    auto front = get_front_tiles();
    generate_tileset(front, pal, NUM_FRONT_TILES, cache_dir, "FRONT");
    
    // Export raw files
    std::string raw_dir = cache_dir + "/RAW";
    create_directory(raw_dir);
    export_raw_sqz("SAMPLE", raw_dir);
    export_raw_sqz("KEYB", raw_dir);
    
    return true;
}

} // namespace assets
