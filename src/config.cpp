#include "config.h"
#include "logger.h"
#include <windows.h>
#include <cstdlib>

ConfigStruct g_Config;

namespace {
    float ReadIniFloat(const char* section, const char* key, float def, LPCSTR iniPath) {
        char defBuf[64];
        char outBuf[64];
        snprintf(defBuf, sizeof(defBuf), "%g", def);
        GetPrivateProfileStringA(section, key, defBuf, outBuf, sizeof(outBuf), iniPath);
        return static_cast<float>(atof(outBuf));
    }
}

namespace Config {
    void Load(const std::string& iniFilePath) {
        LPCSTR iniPath = iniFilePath.c_str();

        g_Config.DebugLog             = GetPrivateProfileIntA("MAIN",         "DebugLog",            1,  iniPath);
        g_Config.EnableFramerateUnlocker = GetPrivateProfileIntA("GRAPHICS_FPS", "EnableFramerateUnlocker", 1, iniPath);
        g_Config.FPSLimit             = GetPrivateProfileIntA("GRAPHICS_FPS", "FPSLimit",            60, iniPath);
        g_Config.UnlockCutsceneFPS    = GetPrivateProfileIntA("GRAPHICS_FPS", "UnlockCutsceneFPS",   0,  iniPath);
        g_Config.ClampSimRateWhenNoControl = GetPrivateProfileIntA("GRAPHICS_FPS", "ClampSimRateWhenNoControl", 1, iniPath);
        g_Config.EnableExtraUIOptions = GetPrivateProfileIntA("UI_DEBUG",     "EnableExtraUIOptions", 0,  iniPath);

        g_Config.DisableCheckpointTimer   = GetPrivateProfileIntA("TRACK_RULES", "DisableCheckpointTimer",   0, iniPath);
        g_Config.DisableResetOOB          = GetPrivateProfileIntA("TRACK_RULES", "DisableResetOOB",          0, iniPath);
        g_Config.DisableWrongWayRespawn   = GetPrivateProfileIntA("TRACK_RULES", "DisableWrongWayRespawn",   0, iniPath);

        g_Config.EnableTrafficControls = GetPrivateProfileIntA("TRAFFIC", "EnableTrafficControls", 0, iniPath);
        g_Config.TrafficDensityScale   = ReadIniFloat("TRAFFIC", "TrafficDensityScale", 0.05f, iniPath);
        g_Config.TrafficMaxDensity     = ReadIniFloat("TRAFFIC", "TrafficMaxDensity",   0.15f, iniPath);
        g_Config.TrafficVehicleLimit   = GetPrivateProfileIntA("TRAFFIC", "TrafficVehicleLimit", 25, iniPath);

        g_Config.RandomizeTimeOfDay   = GetPrivateProfileIntA("GAMEPLAY", "RandomizeTimeOfDay", 0, iniPath);
        g_Config.RunForYourLife       = GetPrivateProfileIntA("GAMEPLAY", "RunForYourLife", 1, iniPath);
        g_Config.DeadlyPlayerNosRechargeScale = ReadIniFloat("GAMEPLAY", "DeadlyPlayerNosRechargeScale", 0.1f, iniPath);
        g_Config.DeadlyPlayerNosStrengthScale = ReadIniFloat("GAMEPLAY", "DeadlyPlayerNosStrengthScale", 1.0f, iniPath);
        g_Config.DeadlyPlayerNosBonusScale    = ReadIniFloat("GAMEPLAY", "DeadlyPlayerNosBonusScale",    10.0f, iniPath);
        g_Config.DeadlyPlayerNosBoostScale    = ReadIniFloat("GAMEPLAY", "DeadlyPlayerNosBoostScale",    1.0f, iniPath);
        g_Config.DeadlyAiNosRechargeScale     = ReadIniFloat("GAMEPLAY", "DeadlyAiNosRechargeScale",     2.0f, iniPath);
        g_Config.DeadlyAiSkillScale   = ReadIniFloat("GAMEPLAY", "DeadlyAiSkillScale", 5.0f, iniPath);
        g_Config.DeadlyAiGlueScale    = ReadIniFloat("GAMEPLAY", "DeadlyAiGlueScale",  2.0f, iniPath);

        g_Config.DisablePlayerAssists       = GetPrivateProfileIntA("VEHICLE", "DisablePlayerAssists", 0, iniPath);
        g_Config.VehicleHealth              = ReadIniFloat("VEHICLE", "VehicleHealth", -1.0f, iniPath);

        g_Config.FixEngineAudioSlew    = GetPrivateProfileIntA("HIGH_FPS_FIXES", "FixEngineAudioSlew",    1, iniPath);
        g_Config.FixKickupParticles     = GetPrivateProfileIntA("HIGH_FPS_FIXES", "FixKickupParticles", 1, iniPath);
        g_Config.KickupVelocityScale    = ReadIniFloat("HIGH_FPS_FIXES", "KickupVelocityScale", -1.0f, iniPath);
        g_Config.EnableRenderTweaks      = GetPrivateProfileIntA("RENDER", "EnableRenderTweaks",      1, iniPath);
        g_Config.ForceFov                = ReadIniFloat("RENDER", "ForceFov",                    60.0f, iniPath);
        g_Config.ForceFovOnlyWhileDriving = GetPrivateProfileIntA("RENDER", "ForceFovOnlyWhileDriving", 1, iniPath);
        g_Config.ForceRenderShiftEnabled = GetPrivateProfileIntA("RENDER", "ForceRenderShiftEnabled", 0, iniPath);
        g_Config.ForceRenderShiftX       = ReadIniFloat("RENDER", "ForceRenderShiftX",            0.0f, iniPath);
        g_Config.ForceRenderShiftY       = ReadIniFloat("RENDER", "ForceRenderShiftY",         -0.017f, iniPath);
        g_Config.ForceRoll               = ReadIniFloat("RENDER", "ForceRoll",                    0.0f, iniPath);
        g_Config.FixMinimapRendering     = GetPrivateProfileIntA("RENDER", "FixMinimapRendering",     1, iniPath);

        g_Config.EnableWorldRenderTweaks  = GetPrivateProfileIntA("WORLDRENDER", "EnableWorldRenderTweaks",  0, iniPath);
        g_Config.ShadowmapResolution      = GetPrivateProfileIntA("WORLDRENDER", "ShadowmapResolution",     -1, iniPath);
        g_Config.ShadowmapQuality         = GetPrivateProfileIntA("WORLDRENDER", "ShadowmapQuality",        -1, iniPath);
        g_Config.ShadowmapViewDistance    = ReadIniFloat("WORLDRENDER", "ShadowmapViewDistance",         -1.0f, iniPath);

        g_Config.AnisotropicFiltering     = GetPrivateProfileIntA("TEXTURE",     "AnisotropicFiltering",    -1, iniPath);

        g_Config.LogNosAwards          = GetPrivateProfileIntA("DIAGNOSTICS",    "LogNosAwards",          0, iniPath);
        g_Config.LogGinsuDiagnostics   = GetPrivateProfileIntA("DIAGNOSTICS",    "LogGinsuDiagnostics",   0, iniPath);
        g_Config.LogSettingsContainers = GetPrivateProfileIntA("DIAGNOSTICS",    "LogSettingsContainers", 0, iniPath);
    }

    void LogSummary() {
        Logger::Log("Config loaded:");
        Logger::Log("  DebugLog=%d  FPSLimit=%d  UnlockCutsceneFPS=%d  EnableExtraUIOptions=%d",
            g_Config.DebugLog, g_Config.FPSLimit, g_Config.UnlockCutsceneFPS,
            g_Config.EnableExtraUIOptions);
        Logger::Log("  EnableFramerateUnlocker=%d  ClampSimRateWhenNoControl=%d",
            g_Config.EnableFramerateUnlocker, g_Config.ClampSimRateWhenNoControl);
        Logger::Log("  DisableCheckpointTimer=%d  DisableResetOOB=%d  DisableWrongWayRespawn=%d",
            g_Config.DisableCheckpointTimer, g_Config.DisableResetOOB, g_Config.DisableWrongWayRespawn);
        Logger::Log("  EnableTrafficControls=%d  TrafficDensityScale=%.3f  TrafficMaxDensity=%.3f  TrafficVehicleLimit=%d",
            g_Config.EnableTrafficControls, g_Config.TrafficDensityScale, g_Config.TrafficMaxDensity, g_Config.TrafficVehicleLimit);
        Logger::Log("  DisablePlayerAssists=%d", g_Config.DisablePlayerAssists);
        Logger::Log("  VehicleHealth=%.1f", g_Config.VehicleHealth);
        Logger::Log("  RandomizeTimeOfDay=%d", g_Config.RandomizeTimeOfDay);
        Logger::Log("  RunForYourLife=%d  DeadlyAiSkillScale=%.2f  DeadlyAiGlueScale=%.2f",
            g_Config.RunForYourLife, g_Config.DeadlyAiSkillScale, g_Config.DeadlyAiGlueScale);
        Logger::Log("  NOS: PlayerRecharge=%.2f  PlayerBonus=%.2f  PlayerStrength=%.2f  PlayerBoost=%.2f  AiRecharge=%.2f",
            g_Config.DeadlyPlayerNosRechargeScale, g_Config.DeadlyPlayerNosBonusScale,
            g_Config.DeadlyPlayerNosStrengthScale, g_Config.DeadlyPlayerNosBoostScale,
            g_Config.DeadlyAiNosRechargeScale);
        Logger::Log("  EnableRenderTweaks=%d  ForceFov=%.1f  FovDrivingOnly=%d  ForceRenderShiftEnabled=%d  ShiftX=%.3f  ShiftY=%.3f  ForceRoll=%.3f  FixMinimapRendering=%d",
            g_Config.EnableRenderTweaks, g_Config.ForceFov, g_Config.ForceFovOnlyWhileDriving, g_Config.ForceRenderShiftEnabled,
            g_Config.ForceRenderShiftX, g_Config.ForceRenderShiftY, g_Config.ForceRoll, g_Config.FixMinimapRendering);
        Logger::Log("  EnableWorldRenderTweaks=%d  ShadowmapRes=%d  ShadowQuality=%d  ShadowDist=%.0f",
            g_Config.EnableWorldRenderTweaks, g_Config.ShadowmapResolution,
            g_Config.ShadowmapQuality, g_Config.ShadowmapViewDistance);
        Logger::Log("  AnisotropicFiltering=%d", g_Config.AnisotropicFiltering);
        Logger::Log("  FixEngineAudioSlew=%d  FixKickupParticles=%d  KickupVelocityScale=%.4f",
            g_Config.FixEngineAudioSlew, g_Config.FixKickupParticles, g_Config.KickupVelocityScale);
    }
}
