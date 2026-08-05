#ifndef FEATURES_H
#define FEATURES_H

namespace Difficulty {
    // What Run For Your Life actually does. Deliberately not INI settings: the
    // mode is meant to mean the same thing on every machine, and a preset that
    // can be dialled back is not a difficulty, it is a suggestion.
    //
    // The health figure is a ceiling rather than a lock, so damage still
    // accumulates and still wrecks you — you just start with half the buffer.
    //
    // The label has a hard limit of seven characters: the game stores its copies
    // of "EXTREME" back to back with no padding between them.
    const float kHealthCap      = 50.0f;
    const char* const kNewLabel = "DEADLY";
    const char* const kNewDescription = "Run for your life, one mistake and you're dead";

    // RaceAIDifficulty enum: 0 Easy, 1 Normal, 2 Hard, 3 Expert ("Extreme").
    // Returns -1 until the game has set it.
    int  GetCurrent();
    // True while the player is on Extreme and the mode is enabled. When this is
    // true the mode owns vehicle health and assists, and [VEHICLE] is ignored.
    bool RunForYourLifeActive();
}

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
    // 0 = stock, 1 = basic set, 2 = everything. Idempotent; only the groups that
    // change are rewritten. Call only while the player has no vehicle control.
    void SetVehicleAssistLevel(int level);
    // The level the [VEHICLE] toggles ask for, ignoring Run For Your Life.
    int  VehicleAssistLevelFromConfig();

    // Run For Your Life: recognises the game's Extreme difficulty and hardens it.
    // Runs from the ticker. INI-gated.
    void UpdateDifficulty();

    // Renames Extreme in the menus by rewriting the loaded localisation strings
    // in place. Searches for them by content, since they live on the heap.
    void UpdateDifficultyText();

    // Fixes the engine-audio pitch above 30 FPS. INI-gated.
    void InitEngineAudioSlewFix();

    // Kickup particle fix: corrects inherited emitter velocity above 30 FPS.
    void InitParticleFix();
    void UpdateParticleFix();

    // Logs Ginsu render state for troubleshooting. INI-gated, OFF by default.
    void InitGinsuDiagnostics();

    // Applies fb::GameRenderSettings tweaks (FOV, viewport shift, roll, minimap fix).
    // Runs from the ticker because the settings pointer is populated lazily.
    void UpdateRenderSettings();

    // Holds the player car's health high so damage never reaches the wreck
    // threshold. INI-gated, OFF by default. Runs from the ticker.
    void UpdatePlayerVehicle();

    // Logs every Frostbite settings-container address once. Troubleshooting/research.
    void UpdateSettingsProbe();
}

#endif // FEATURES_H
