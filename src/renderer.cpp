#include "renderer.h"
#include <stdexcept>

namespace renderer {

Renderer::Renderer() {}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init(const char* title) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL init failed: %s", SDL_GetError());
        return false;
    }
    
    window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * SCALE,
        SCREEN_HEIGHT * SCALE,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        return false;
    }
    
    sdl_renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    
    if (!sdl_renderer) {
        SDL_Log("Renderer creation failed: %s", SDL_GetError());
        return false;
    }
    
    SDL_RenderSetLogicalSize(sdl_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    key_state = SDL_GetKeyboardState(nullptr);
    running = true;
    
    return true;
}

void Renderer::shutdown() {
    if (tilemap_texture) {
        SDL_DestroyTexture(tilemap_texture);
        tilemap_texture = nullptr;
    }
    if (background_texture) {
        SDL_DestroyTexture(background_texture);
        background_texture = nullptr;
    }
    if (sdl_renderer) {
        SDL_DestroyRenderer(sdl_renderer);
        sdl_renderer = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
}

SDL_Texture* Renderer::create_texture_from_image(const assets::Image& image) {
    SDL_Surface* surface = SDL_CreateRGBSurface(
        0, image.width, image.height, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000
    );
    
    if (!surface) {
        return nullptr;
    }
    
    uint32_t* pixels = static_cast<uint32_t*>(surface->pixels);
    
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            int idx = y * image.width + x;
            uint8_t color_idx = (idx < static_cast<int>(image.pixels.size())) ? image.pixels[idx] : 0;
            
            uint8_t r = image.palette.r(color_idx);
            uint8_t g = image.palette.g(color_idx);
            uint8_t b = image.palette.b(color_idx);
            
            pixels[idx] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(sdl_renderer, surface);
    SDL_FreeSurface(surface);
    
    return texture;
}

void Renderer::set_background(const assets::Image& image) {
    if (background_texture) {
        SDL_DestroyTexture(background_texture);
    }
    background_texture = create_texture_from_image(image);
}

void Renderer::clear_tilemap() {
    if (tilemap_texture) {
        SDL_DestroyTexture(tilemap_texture);
        tilemap_texture = nullptr;
    }
    current_level = nullptr;
    scroll_x = scroll_y = 0;
    max_scroll_x = max_scroll_y = 0;
}

void Renderer::set_tilemap(const assets::LevelData& level) {
    current_level = &level;
    
    int map_width = level.tilemap.width * 16;
    int map_height = level.tilemap.height * 16;
    
    max_scroll_x = map_width - SCREEN_WIDTH;
    max_scroll_y = map_height - SCREEN_HEIGHT;
    if (max_scroll_x < 0) max_scroll_x = 0;
    if (max_scroll_y < 0) max_scroll_y = 0;
    
    SDL_Surface* surface = SDL_CreateRGBSurface(
        0, map_width, map_height, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000
    );
    
    if (!surface) {
        return;
    }
    
    // Clear with transparent
    SDL_FillRect(surface, nullptr, 0x00000000);
    
    uint32_t* pixels = static_cast<uint32_t*>(surface->pixels);
    
    auto union_tileset = assets::get_union_tiles();
    
    for (int ty = 0; ty < level.tilemap.height; ty++) {
        for (int tx = 0; tx < level.tilemap.width; tx++) {
            int map_idx = ty * level.tilemap.width + tx;
            uint8_t tile_byte = level.tilemap.map[map_idx];
            uint16_t lut_value = level.tilemap.lut[tile_byte];
            
            const std::vector<uint8_t>* tile_pixels = nullptr;
            
            if (lut_value < 256) {
                if (lut_value < level.local_tiles.tiles.size()) {
                    tile_pixels = &level.local_tiles.tiles[lut_value];
                }
            } else if (lut_value < 256 + union_tileset.num_tiles) {
                int union_idx = lut_value - 256;
                if (union_idx < static_cast<int>(union_tileset.tiles.size())) {
                    tile_pixels = &union_tileset.tiles[union_idx];
                }
            }
            
            // Skip first union tile (empty)
            if (lut_value == 256) {
                continue;
            }
            
            if (tile_pixels && !tile_pixels->empty()) {
                for (int py = 0; py < 16; py++) {
                    for (int px = 0; px < 16; px++) {
                        int src_idx = py * 16 + px;
                        if (src_idx >= static_cast<int>(tile_pixels->size())) continue;
                        
                        int dst_x = tx * 16 + px;
                        int dst_y = ty * 16 + py;
                        int dst_idx = dst_y * map_width + dst_x;
                        
                        uint8_t color_idx = (*tile_pixels)[src_idx];
                        
                        // Skip transparent (index 0)
                        if (color_idx == 0) continue;
                        
                        uint8_t r = level.palette.r(color_idx);
                        uint8_t g = level.palette.g(color_idx);
                        uint8_t b = level.palette.b(color_idx);
                        
                        pixels[dst_idx] = (0xFF << 24) | (r << 16) | (g << 8) | b;
                    }
                }
            }
        }
    }
    
    if (tilemap_texture) {
        SDL_DestroyTexture(tilemap_texture);
    }
    
    tilemap_texture = SDL_CreateTextureFromSurface(sdl_renderer, surface);
    SDL_SetTextureBlendMode(tilemap_texture, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(surface);
}

void Renderer::set_scroll(int x, int y) {
    scroll_x = x;
    scroll_y = y;
    
    if (scroll_x < 0) scroll_x = 0;
    if (scroll_y < 0) scroll_y = 0;
    if (scroll_x > max_scroll_x) scroll_x = max_scroll_x;
    if (scroll_y > max_scroll_y) scroll_y = max_scroll_y;
}

void Renderer::render() {
    SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, 255);
    SDL_RenderClear(sdl_renderer);
    
    render_background();
    render_tilemap();
    
    SDL_RenderPresent(sdl_renderer);
}

void Renderer::render_background() {
    if (background_texture) {
        SDL_RenderCopy(sdl_renderer, background_texture, nullptr, nullptr);
    }
}

void Renderer::render_tilemap() {
    if (tilemap_texture) {
        SDL_Rect src = {scroll_x, scroll_y, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_Rect dst = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderCopy(sdl_renderer, tilemap_texture, &src, &dst);
    }
}

bool Renderer::process_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }
    }
    return running;
}

bool Renderer::is_key_down(SDL_Scancode key) const {
    return key_state && key_state[key];
}

} // namespace renderer
