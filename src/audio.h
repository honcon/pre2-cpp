#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace audio {

// Initialize audio system
bool init();

// Shutdown audio system
void shutdown();

// Load and play a TRK file (Adlib format)
bool play_track(const std::string& filename);

// Load and play from raw TRK data
bool play_track_data(const std::vector<uint8_t>& data);

// Stop current music
void stop();

// Pause/Resume
void pause();
void resume();

// Check if playing
bool is_playing();

// Set volume (0-128)
void set_volume(int volume);

} // namespace audio
