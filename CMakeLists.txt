cmake_minimum_required(VERSION 3.10)
project(pre2 VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL2
find_package(SDL2 REQUIRED)

# Try to find SDL2_mixer
find_library(SDL2_MIXER_LIBRARY SDL2_mixer)

if(SDL2_MIXER_LIBRARY)
    add_definitions(-DHAVE_SDL_MIXER)
    message(STATUS "SDL2_mixer found: ${SDL2_MIXER_LIBRARY}")
else()
    message(STATUS "SDL2_mixer not found - audio disabled")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/sqz_unpacker.cpp
    src/asset_converter.cpp
    src/renderer.cpp
    src/audio.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${SDL2_INCLUDE_DIRS}
    /opt/homebrew/include
    src
)

# Link directories
target_link_directories(${PROJECT_NAME} PRIVATE
    /opt/homebrew/lib
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_LIBRARIES})

if(SDL2_MIXER_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_MIXER_LIBRARY})
endif()

# macOS specific
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework Cocoa")
endif()

# Copy sqz folder to build directory (if exists)
if(EXISTS "${CMAKE_SOURCE_DIR}/sqz")
    file(COPY ${CMAKE_SOURCE_DIR}/sqz DESTINATION ${CMAKE_BINARY_DIR})
endif()
