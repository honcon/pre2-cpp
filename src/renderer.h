#pragma once

#include "asset_converter.h"
#include <SDL2/SDL.h>
#include <memory>

namespace renderer {

class Renderer {
public:
    static const int SCREEN_WIDTH = 320;
    static const int SCREEN_HEIGHT = 200;
    static const int SCALE = 3;
    
    Renderer();
    ~Renderer();
    
    bool init(const char* title = "Prehistorik 2");
    void shutdown();
    
    void set_background(const assets::Image& image);
    void set_tilemap(const assets::LevelData& level);
    void clear_tilemap();
    void set_scroll(int x, int y);
    
    void render();
    bool process_events();
    bool is_key_down(SDL_Scancode key) const;
    
    int get_scroll_x() const { return scroll_x; }
    int get_scroll_y() const { return scroll_y; }
    
private:
    SDL_Window* window = nullptr;
    SDL_Renderer* sdl_renderer = nullptr;
    SDL_Texture* background_texture = nullptr;
    SDL_Texture* tilemap_texture = nullptr;
    
    const assets::LevelData* current_level = nullptr;
    
    int scroll_x = 0;
    int scroll_y = 0;
    int max_scroll_x = 0;
    int max_scroll_y = 0;
    
    const uint8_t* key_state = nullptr;
    bool running = true;
    
    void render_background();
    void render_tilemap();
    SDL_Texture* create_texture_from_image(const assets::Image& image);
};

} // namespace renderer
