#ifndef CONFIG_H
#define CONFIG_H

#include <string>

struct ConfigStruct {
    // [MAIN]
    int DebugLog = 1;

    // [GRAPHICS_FPS]
    int EnableFramerateUnlocker = 1;   // master gate: injects GameTime + control hooks
    int FPSLimit = 60;
    int UnlockCutsceneFPS = 0;
    int ClampSimRateWhenNoControl = 1;

    // [UI_DEBUG]
    int EnableExtraUIOptions = 1;

    // [TRACK_RULES] — all OFF by default; opt-in gameplay changes
    int DisableCheckpointTimer = 0;
    int DisableResetOOB = 0;
    int DisableWrongWayRespawn = 0;

    // [TRAFFIC] — OFF by default; forces the game's traffic values
    int EnableTrafficControls = 0;
    float TrafficDensityScale = 0.05f;
    float TrafficMaxDensity = 0.15f;
    int TrafficVehicleLimit = 25;

    // [VEHICLE] — OFF by default; strips driving assists
    int DisableAllVehicleAssists = 0;
    int DisableBasicVehicleAssists = 0;
    // fb::NFSVehicle::m_health (offset 0x1878, float). Stock 100; the wreck
    // screen fires when damage drives it low enough. <=0 leaves it alone.
    float VehicleHealth = -1.0f;

    // [HIGH_FPS_FIXES]
    // Fixes the engine-audio pitch above 30 FPS. 1 = snap (recommended),
    // 2 = keep the pitch glide but only re-arm it once finished.
    int FixEngineAudioSlew = 1;
    // Fixes kickup particles (snow spray, drift smoke) above 30 FPS.
    int FixKickupParticles = 1;
    // >0 overrides the automatic 30/fps correction, for tuning by eye.
    float KickupVelocityScale = -1.0f;

    // [RENDER] — fb::GameRenderSettings tweaks, applied through [0x02753F28]
    int EnableRenderTweaks = 1;
    float ForceFov = 60.0f;              // <=0 leaves the engine default (48)
    int ForceFovOnlyWhileDriving = 1;    // restore stock FOV in menus/garage
    int ForceRenderShiftEnabled = 0;     // gates the two shifts below
    float ForceRenderShiftX = 0.0f;
    float ForceRenderShiftY = -0.017f;  // the game's own value
    float ForceRoll = 0.0f;
    int FixMinimapRendering = 1;         // InitialClearEnable

    // [WORLDRENDER] — fb::WorldRenderSettings. Resolved through the settings
    // manager, not a static pointer. -1 means "leave the engine's own value".
    // Only the shadow settings are here: motion blur, MSAA and the cascade slice
    // count were all tested and have no effect in the retail build.
    int EnableWorldRenderTweaks = 0;
    int ShadowmapResolution = -1;        // 2048 at the game's highest preset
    int ShadowmapQuality = -1;           // stock 1; 0 or 2 disable the filtering
    float ShadowmapViewDistance = -1.0f; // stock 200

    // [TEXTURE] — fb::ShaderSystemSettings::MaxAnisotropy (offset 0x94, int32).
    // Stock 4. The engine rewrites it back to 4 whenever a level loads, so the
    // ticker keeps reapplying it. -1 leaves the engine's own value.
    int AnisotropicFiltering = -1;

    // [DIAGNOSTICS] Log Ginsu render state ~1x/sec per voice. Troubleshooting only.
    int LogGinsuDiagnostics = 0;
    int LogSettingsContainers = 0;   // dump settings-container addresses once
};

extern ConfigStruct g_Config;

namespace Config {
    void Load(const std::string& iniFilePath);
    // Logs the loaded values. Must be called AFTER Logger::Init, otherwise the
    // output is dropped (the logger isn't ready during Load).
    void LogSummary();
}

#endif // CONFIG_H
