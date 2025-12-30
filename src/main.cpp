#include "asset_converter.h"
#include "renderer.h"
#include "audio.h"
#include <iostream>
#include <cmath>

// Game state
enum class GameState {
    Titus,
    Menu,
    Map,
    Playing,
    GameOver,
    Credits,
    TheEnd
};

class Game {
public:
    renderer::Renderer render;
    GameState state = GameState::Titus;
    int current_level = 0;
    
    // Scroll with inertia
    float scroll_x = 0, scroll_y = 0;
    float speed_x = 0, speed_y = 0;
    const float ACCEL = 0.02f;
    const float MAX_SPEED = 1.0f;
    const float MOVE_SPEED = 3.0f;
    
    bool running = true;
    
    bool init() {
        assets::set_sqz_path("sqz");
        assets::load_level_palettes("res");
        
        if (!render.init("Prehistorik 2 - C++ SDL2")) {
            return false;
        }
        
        // Initialize audio
        if (!audio::init()) {
            std::cout << "Audio disabled" << std::endl;
        }
        
        return true;
    }
    
    void shutdown() {
        audio::shutdown();
    }
    
    void play_level_music(int level_idx) {
        try {
            auto track = assets::get_level_track(level_idx);
            if (!audio::play_track_data(track)) {
                std::cout << "Failed to play level music" << std::endl;
            }
        } catch (...) {
            std::cout << "No music for level " << (level_idx + 1) << std::endl;
        }
    }
    
    void play_intro_music() {
        try {
            auto track = assets::get_intro_track();
            audio::play_track_data(track);
        } catch (...) {}
    }
    
    void play_menu_music() {
        try {
            auto track = assets::get_menu_track();
            audio::play_track_data(track);
        } catch (...) {}
    }
    
    float update_speed(float speed, int dir) {
        if (dir == 1) {
            speed += ACCEL;
            if (speed > MAX_SPEED) speed = MAX_SPEED;
        } else if (dir == -1) {
            speed -= ACCEL;
            if (speed < -MAX_SPEED) speed = -MAX_SPEED;
        } else {
            // Decelerate
            if (speed > 0) {
                speed -= ACCEL;
                if (speed < 0) speed = 0;
            } else if (speed < 0) {
                speed += ACCEL;
                if (speed > 0) speed = 0;
            }
        }
        return speed;
    }
    
    void load_level(int idx) {
        if (idx < 0) idx = 0;
        if (idx >= assets::NUM_LEVELS) idx = assets::NUM_LEVELS - 1;
        
        current_level = idx;
        scroll_x = scroll_y = 0;
        speed_x = speed_y = 0;
        
        std::cout << "Loading level " << (idx + 1) << "..." << std::endl;
        
        auto background = assets::get_level_background(idx);
        render.set_background(background);
        
        auto level_data = assets::get_level_data(idx);
        render.set_tilemap(level_data);
        
        play_level_music(idx);
    }
    
    void show_titus() {
        std::cout << "Showing Titus screen..." << std::endl;
        auto titus = assets::get_titus_bitmap();
        render.set_background(titus);
        render.clear_tilemap();
        play_intro_music();
        state = GameState::Titus;
    }
    
    void show_menu() {
        std::cout << "Showing Menu..." << std::endl;
        try {
            auto menu = assets::get_menu_bitmap();
            render.set_background(menu);
            render.clear_tilemap();
            play_menu_music();
        } catch (...) {}
        state = GameState::Menu;
    }
    
    void show_credits() {
        std::cout << "Showing Credits..." << std::endl;
        try {
            auto credits = assets::get_credits_bitmap();
            render.set_background(credits);
            render.clear_tilemap();
        } catch (...) {}
        state = GameState::Credits;
    }
    
    void show_theend() {
        std::cout << "Showing The End..." << std::endl;
        try {
            auto theend = assets::get_theend_bitmap();
            render.set_background(theend);
            render.clear_tilemap();
        } catch (...) {}
        state = GameState::TheEnd;
    }
    
    void show_gameover() {
        std::cout << "Game Over..." << std::endl;
        try {
            auto gameover = assets::get_gameover_bitmap();
            render.set_background(gameover);
            render.clear_tilemap();
            auto track = assets::get_gameover_track();
            audio::play_track_data(track);
        } catch (...) {}
        state = GameState::GameOver;
    }
    
    void run() {
        show_titus();
        
        bool space_was_pressed = false;
        bool pgup_was_pressed = false;
        bool pgdn_was_pressed = false;
        
        std::cout << "\nControls:" << std::endl;
        std::cout << "  Arrow keys: Scroll" << std::endl;
        std::cout << "  Page Up/Down: Change level" << std::endl;
        std::cout << "  Space/Enter: Continue" << std::endl;
        std::cout << "  1-9, A-G: Jump to level" << std::endl;
        std::cout << "  M: Menu, C: Credits, E: TheEnd, G: GameOver" << std::endl;
        std::cout << "  +/-: Volume" << std::endl;
        std::cout << "  ESC: Quit" << std::endl;
        
        int volume = 100;
        
        while (running && render.process_events()) {
            bool space_pressed = render.is_key_down(SDL_SCANCODE_SPACE) || 
                                  render.is_key_down(SDL_SCANCODE_RETURN);
            bool pgup_pressed = render.is_key_down(SDL_SCANCODE_PAGEUP);
            bool pgdn_pressed = render.is_key_down(SDL_SCANCODE_PAGEDOWN);
            
            // Volume control
            if (render.is_key_down(SDL_SCANCODE_EQUALS) || render.is_key_down(SDL_SCANCODE_KP_PLUS)) {
                volume = std::min(128, volume + 2);
                audio::set_volume(volume);
            }
            if (render.is_key_down(SDL_SCANCODE_MINUS) || render.is_key_down(SDL_SCANCODE_KP_MINUS)) {
                volume = std::max(0, volume - 2);
                audio::set_volume(volume);
            }
            
            switch (state) {
                case GameState::Titus:
                case GameState::Menu:
                case GameState::Credits:
                case GameState::TheEnd:
                case GameState::GameOver:
                    if (space_pressed && !space_was_pressed) {
                        if (state == GameState::Titus) {
                            show_menu();
                        } else {
                            state = GameState::Playing;
                            load_level(current_level);
                        }
                    }
                    break;
                    
                case GameState::Playing: {
                    // Handle direction input with inertia
                    int dir_x = 0, dir_y = 0;
                    if (render.is_key_down(SDL_SCANCODE_RIGHT)) dir_x++;
                    if (render.is_key_down(SDL_SCANCODE_LEFT))  dir_x--;
                    if (render.is_key_down(SDL_SCANCODE_DOWN))  dir_y++;
                    if (render.is_key_down(SDL_SCANCODE_UP))    dir_y--;
                    
                    speed_x = update_speed(speed_x, dir_x);
                    speed_y = update_speed(speed_y, dir_y);
                    
                    scroll_x += MOVE_SPEED * speed_x;
                    scroll_y += MOVE_SPEED * speed_y;
                    
                    if (scroll_x < 0) scroll_x = 0;
                    if (scroll_y < 0) scroll_y = 0;
                    
                    render.set_scroll(static_cast<int>(scroll_x), static_cast<int>(scroll_y));
                    
                    // Level switching
                    if (pgup_pressed && !pgup_was_pressed) {
                        load_level(current_level - 1);
                    }
                    if (pgdn_pressed && !pgdn_was_pressed) {
                        load_level(current_level + 1);
                    }
                    
                    // Direct level jump
                    for (int i = 0; i < 9; i++) {
                        if (render.is_key_down(static_cast<SDL_Scancode>(SDL_SCANCODE_1 + i))) {
                            load_level(i);
                        }
                    }
                    if (render.is_key_down(SDL_SCANCODE_A)) load_level(9);
                    if (render.is_key_down(SDL_SCANCODE_B)) load_level(10);
                    if (render.is_key_down(SDL_SCANCODE_D)) load_level(11);
                    if (render.is_key_down(SDL_SCANCODE_F)) load_level(13);
                    
                    // Screen shortcuts
                    if (render.is_key_down(SDL_SCANCODE_M)) show_menu();
                    if (render.is_key_down(SDL_SCANCODE_C)) show_credits();
                    if (render.is_key_down(SDL_SCANCODE_E)) show_theend();
                    if (render.is_key_down(SDL_SCANCODE_G)) show_gameover();
                    break;
                }
                    
                case GameState::Map:
                    break;
            }
            
            space_was_pressed = space_pressed;
            pgup_was_pressed = pgup_pressed;
            pgdn_was_pressed = pgdn_pressed;
            
            render.render();
        }
    }
};

int main(int argc, char* argv[]) {
    try {
        Game game;
        if (!game.init()) {
            std::cerr << "Failed to initialize" << std::endl;
            return 1;
        }
        
        game.run();
        game.shutdown();
        
        std::cout << "Goodbye!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
