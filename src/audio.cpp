#include "audio.h"
#include "sqz_unpacker.h"

#ifdef HAVE_SDL_MIXER
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <fstream>
#include <cstring>

namespace audio {

static bool g_initialized = false;
static Mix_Music* g_music = nullptr;
static int g_volume = 100;
static std::string g_temp_file = "/tmp/pre2_music.mod";

bool init() {
    if (g_initialized) return true;
    
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        SDL_Log("SDL audio init failed: %s", SDL_GetError());
        return false;
    }
    
    // Initialize SDL_mixer with MOD support
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) < 0) {
        SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
        return false;
    }
    
    g_initialized = true;
    SDL_Log("Audio initialized");
    return true;
}

void shutdown() {
    stop();
    
    if (g_initialized) {
        Mix_CloseAudio();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        g_initialized = false;
    }
}

bool play_track(const std::string& filename) {
    if (!g_initialized && !init()) return false;
    
    stop();
    
    // Unpack DIET-compressed TRK file to raw MOD
    std::vector<uint8_t> data;
    try {
        data = sqz::unpack(filename);
    } catch (const std::exception& e) {
        SDL_Log("Failed to unpack TRK: %s - %s", filename.c_str(), e.what());
        return false;
    }
    
    if (data.empty()) {
        SDL_Log("Empty TRK data: %s", filename.c_str());
        return false;
    }
    
    // Write decompressed MOD to temp file
    std::ofstream temp(g_temp_file, std::ios::binary);
    if (!temp) {
        SDL_Log("Failed to create temp file");
        return false;
    }
    temp.write(reinterpret_cast<const char*>(data.data()), data.size());
    temp.close();
    
    SDL_Log("Decompressed TRK: %zu bytes", data.size());
    
    // Load as MOD music
    g_music = Mix_LoadMUS(g_temp_file.c_str());
    if (!g_music) {
        SDL_Log("Mix_LoadMUS failed: %s", Mix_GetError());
        return false;
    }
    
    // Play music looping
    if (Mix_PlayMusic(g_music, -1) < 0) {
        SDL_Log("Mix_PlayMusic failed: %s", Mix_GetError());
        Mix_FreeMusic(g_music);
        g_music = nullptr;
        return false;
    }
    
    Mix_VolumeMusic(g_volume);
    SDL_Log("Playing music from: %s", filename.c_str());
    
    return true;
}

bool play_track_data(const std::vector<uint8_t>& data) {
    if (!g_initialized && !init()) return false;
    
    stop();
    
    if (data.empty()) {
        return false;
    }
    
    // The data should already be unpacked (from get_level_track etc)
    // Write to temp file
    std::ofstream temp(g_temp_file, std::ios::binary);
    if (!temp) {
        return false;
    }
    temp.write(reinterpret_cast<const char*>(data.data()), data.size());
    temp.close();
    
    SDL_Log("MOD data size: %zu bytes", data.size());
    
    // Load as MOD music
    g_music = Mix_LoadMUS(g_temp_file.c_str());
    if (!g_music) {
        SDL_Log("Mix_LoadMUS failed: %s", Mix_GetError());
        return false;
    }
    
    if (Mix_PlayMusic(g_music, -1) < 0) {
        SDL_Log("Mix_PlayMusic failed: %s", Mix_GetError());
        Mix_FreeMusic(g_music);
        g_music = nullptr;
        return false;
    }
    
    Mix_VolumeMusic(g_volume);
    return true;
}

void stop() {
    if (g_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(g_music);
        g_music = nullptr;
    }
}

void pause() {
    Mix_PauseMusic();
}

void resume() {
    Mix_ResumeMusic();
}

bool is_playing() {
    return Mix_PlayingMusic() != 0;
}

void set_volume(int volume) {
    g_volume = (volume < 0) ? 0 : (volume > 128) ? 128 : volume;
    Mix_VolumeMusic(g_volume);
}

} // namespace audio

#else // No SDL_MIXER

namespace audio {

bool init() { return false; }
void shutdown() {}
bool play_track(const std::string&) { return false; }
bool play_track_data(const std::vector<uint8_t>&) { return false; }
void stop() {}
void pause() {}
void resume() {}
bool is_playing() { return false; }
void set_volume(int) {}

} // namespace audio

#endif
