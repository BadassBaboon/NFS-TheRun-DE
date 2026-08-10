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

    // The rest of the mode's numbers, arrived at by playing it. Everything here
    // has an equivalent in [VEHICLE] that the player can set for themselves; the
    // mode overrides that while it is engaged.
    const float kAiSkillScale          = 1.10f;
    const float kAiGlueScale           = 0.97f;
    const float kAiNosRechargeScale    = 2.00f;
    const float kPlayerNosRechargeScale = 0.10f;
    const float kPlayerNosBonusScale    = 10.0f;
    // A little more push when you do get to use it, to offset how scarce the bar
    // now is. This is the one thing the mode gives the player rather than takes.
    const float kPlayerNosStrengthScale = 1.12f;
    // How fast the player's draft meter builds. The slingshot it pays out is NOT
    // scaled — sit in the slipstream twice as long and you get the same reward.
    // Drafting was removed outright at one point, which was the nitrous mistake
    // again: it is a core mechanic, and deleting it removes a skill expression
    // instead of demanding one.
    const float kPlayerDraftRateScale   = 0.50f;
    // A MULTIPLIER on whatever ceiling the event was authored with, not a fixed
    // number. A flat 0.25 was tried first and made sparse events as busy as city
    // ones, which took the speed out of the game. Scaling keeps each event's own
    // character and just makes the busy ones busier.
    const float kTrafficMaxDensityScale = 2.0f;

    // The scripted-grant scale is deliberately NOT part of the mode. It stays
    // wherever [VEHICLE] leaves it.
    const char* const kNewLabel = "DEADLY";
    const char* const kNewDescription = "Run for your life, one mistake and you're dead";

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

    // Forces the pause menu's photo mode entry visible on its own, rather than
    // unhiding every hidden entry the way InitExtraUIOptions does.
    void InitPhotoMode();

    // Unlocks framerate cap via GameTime hook; adjustable via INI
    void InitFramerateUnlocker();
    void UpdateFramerateUnlocker();

    // Relaxes track rules (checkpoint timer, OOB reset, wrong-way respawn).
    // All INI-gated and OFF by default.
    void InitTrackRules();

    // Turns the out-of-bounds reset off while Run For Your Life is engaged, and
    // puts it back when it is not. Live, because the difficulty is not known at
    // init and can change between events.
    void UpdateTrackRules();

    // Forces traffic density scale, max density, and vehicle limit. INI-gated, OFF by default.
    void InitTrafficControls();
    void UpdateTrafficControls();

    // Player draft rate and driving-assist suppression, both PLAYER ONLY. One
    // cave keyed on the game's own isHumanPlayer flag, so AI cars are never
    // affected and keep their full slipstream.
    void InitInputStateHook();
    void UpdateInputState();

    // Scales the AI's per-difficulty performance multipliers while Run For Your
    // Life is engaged. AI-side only, so it cannot affect the player's car.
    void InitAiDifficulty();
    void UpdateAiDifficulty();

    // Nitrous tuning: scarcer and weaker for the player, more plentiful for the
    // AI. Each branch of the game's own player/AI split gets its own hook.
    void InitNosTuning();
    void UpdateNosTuning();

    // Randomises each level's time of day within the presets it supports. Not
    // tied to the difficulty mode; applies everywhere including challenge series.
    void InitTodRandomizer();
    void UpdateTodRandomizer();

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
