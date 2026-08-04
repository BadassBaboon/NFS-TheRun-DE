@echo off
echo Building NFS The Run Definitive Edition (.asi) with MinGW...

set GXX=C:\MinGW\bin\g++.exe
set OUT=NFSTR_DefinitiveEdition.asi

%GXX% -shared -O2 -std=c++17 -m32 ^
    src/main.cpp ^
    src/logger.cpp ^
    src/config.cpp ^
    src/memory.cpp ^
    src/features/patch_util.cpp ^
    src/features/garage_render.cpp ^
    src/features/extra_ui.cpp ^
    src/features/fps_unlocker.cpp ^
    src/features/track_rules.cpp ^
    src/features/traffic.cpp ^
    src/features/vehicle.cpp ^
    src/features/engine_audio.cpp ^
    src/features/particle_fix.cpp ^
    src/features/render_settings.cpp ^
    src/features/settings_probe.cpp ^
    -o %OUT% ^
    -lpsapi -lkernel32 -luser32 ^
    -static-libgcc -static-libstdc++

if %ERRORLEVEL% EQU 0 (
    echo.
    echo BUILD SUCCESSFUL: %OUT% created!
) else (
    echo.
    echo BUILD FAILED with error level %ERRORLEVEL%
)
