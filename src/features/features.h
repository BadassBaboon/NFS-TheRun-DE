#ifndef FEATURES_H
#define FEATURES_H

namespace Features {
    // Garage Car Render — BACKLOGGED
    // 3D inspect works natively via Enter key in garage. Carousel AOB patch scrapped.
    void InitGarageCarRender();

    // Unlocks QA debug menu, Photo Mode, and hidden UI entries
    void InitExtraUIOptions();

    // Unlocks framerate cap via GameTime hook; adjustable via INI
    void InitFramerateUnlocker();
    void UpdateFramerateUnlocker();

    // Relaxes track rules (checkpoint timer, OOB reset, wrong-way respawn).
    // All INI-gated and OFF by default.
    void InitTrackRules();

    // Forces traffic density scale, max density, and vehicle limit. INI-gated, OFF by default.
    void InitTrafficControls();

    // Strips driving assists (RaceLineAssist, drift/raceline analyzers, extra forces).
    // INI-gated, OFF by default.
    void InitVehicleAssists();

    // Fixes the engine-audio pitch above 30 FPS. INI-gated.
    void InitEngineAudioSlewFix();

    // High-FPS particle scaling & component frame interpolation fix (Path A)
    void InitParticleFix();
    void UpdateParticleFix();

    // Logs Ginsu render state for troubleshooting. INI-gated, OFF by default.
    void InitGinsuDiagnostics();

    // Applies fb::GameRenderSettings tweaks (FOV, viewport shift, roll, minimap fix).
    // Runs from the ticker because the settings pointer is populated lazily.
    void UpdateRenderSettings();

    // Logs every Frostbite settings-container address once. Troubleshooting/research.
    void UpdateSettingsProbe();
}

#endif // FEATURES_H
