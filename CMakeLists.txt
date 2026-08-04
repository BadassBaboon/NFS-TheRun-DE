cmake_minimum_required(VERSION 3.10)
project(NFSTR_DefinitiveEdition C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/main.cpp
    src/logger.cpp
    src/config.cpp
    src/memory.cpp
    src/features/patch_util.cpp
    src/features/garage_render.cpp
    src/features/extra_ui.cpp
    src/features/fps_unlocker.cpp
    src/features/track_rules.cpp
    src/features/traffic.cpp
    src/features/vehicle.cpp
    src/features/engine_audio.cpp
    src/features/particle_fix.cpp
    src/features/render_settings.cpp
    src/features/settings_probe.cpp
)

add_library(NFSTR_DefinitiveEdition SHARED ${SOURCES})

# Target as .asi extension (Win32 DLL)
set_target_properties(NFSTR_DefinitiveEdition PROPERTIES
    PREFIX ""
    SUFFIX ".asi"
)

target_link_libraries(NFSTR_DefinitiveEdition PRIVATE
    psapi
    kernel32
    user32
)
