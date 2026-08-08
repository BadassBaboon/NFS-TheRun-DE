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
        g_Config.AlwaysShowPhotoMode  = GetPrivateProfileIntA("UI_DEBUG",     "AlwaysShowPhotoMode",  1,  iniPath);

        g_Config.DisableCheckpointTimer   = GetPrivateProfileIntA("TRACK_RULES", "DisableCheckpointTimer",   0, iniPath);
        g_Config.DisableResetOOB          = GetPrivateProfileIntA("TRACK_RULES", "DisableResetOOB",          0, iniPath);
        g_Config.DisableWrongWayRespawn   = GetPrivateProfileIntA("TRACK_RULES", "DisableWrongWayRespawn",   0, iniPath);

        g_Config.EnableTrafficControls = GetPrivateProfileIntA("TRAFFIC", "EnableTrafficControls", 0, iniPath);
        g_Config.TrafficDensityScale   = ReadIniFloat("TRAFFIC", "TrafficDensityScale", 0.05f, iniPath);
        g_Config.TrafficMaxDensity     = ReadIniFloat("TRAFFIC", "TrafficMaxDensity",   0.15f, iniPath);
        g_Config.TrafficVehicleLimit   = GetPrivateProfileIntA("TRAFFIC", "TrafficVehicleLimit", 25, iniPath);

        g_Config.RandomizeTimeOfDay   = GetPrivateProfileIntA("GAMEPLAY", "RandomizeTimeOfDay", 0, iniPath);
        g_Config.RunForYourLife       = GetPrivateProfileIntA("GAMEPLAY", "RunForYourLife", 1, iniPath);

        g_Config.DisablePlayerAssists       = GetPrivateProfileIntA("VEHICLE", "DisablePlayerAssists", 0, iniPath);
        g_Config.VehicleHealth              = ReadIniFloat("VEHICLE", "VehicleHealth", -1.0f, iniPath);
        g_Config.AiSkillScale           = ReadIniFloat("VEHICLE", "AiSkillScale",           1.0f, iniPath);
        g_Config.AiGlueScale            = ReadIniFloat("VEHICLE", "AiGlueScale",            1.0f, iniPath);
        g_Config.AiNosRechargeScale     = ReadIniFloat("VEHICLE", "AiNosRechargeScale",     1.0f, iniPath);
        g_Config.PlayerNosRechargeScale = ReadIniFloat("VEHICLE", "PlayerNosRechargeScale", 1.0f, iniPath);
        g_Config.PlayerNosBonusScale    = ReadIniFloat("VEHICLE", "PlayerNosBonusScale",    1.0f, iniPath);
        g_Config.PlayerNosStrengthScale = ReadIniFloat("VEHICLE", "PlayerNosStrengthScale", 1.0f, iniPath);
        g_Config.PlayerNosBoostScale    = ReadIniFloat("VEHICLE", "PlayerNosBoostScale",    1.0f, iniPath);

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
        Logger::Log("  AlwaysShowPhotoMode=%d", g_Config.AlwaysShowPhotoMode);
        Logger::Log("  EnableFramerateUnlocker=%d  ClampSimRateWhenNoControl=%d",
            g_Config.EnableFramerateUnlocker, g_Config.ClampSimRateWhenNoControl);
        Logger::Log("  DisableCheckpointTimer=%d  DisableResetOOB=%d  DisableWrongWayRespawn=%d",
            g_Config.DisableCheckpointTimer, g_Config.DisableResetOOB, g_Config.DisableWrongWayRespawn);
        Logger::Log("  EnableTrafficControls=%d  TrafficDensityScale=%.3f  TrafficMaxDensity=%.3f  TrafficVehicleLimit=%d",
            g_Config.EnableTrafficControls, g_Config.TrafficDensityScale, g_Config.TrafficMaxDensity, g_Config.TrafficVehicleLimit);
        Logger::Log("  DisablePlayerAssists=%d", g_Config.DisablePlayerAssists);
        Logger::Log("  VehicleHealth=%.1f", g_Config.VehicleHealth);
        Logger::Log("  RandomizeTimeOfDay=%d", g_Config.RandomizeTimeOfDay);
        Logger::Log("  RunForYourLife=%d", g_Config.RunForYourLife);
        Logger::Log("  AiSkillScale=%.2f  AiGlueScale=%.2f  AiNosRechargeScale=%.2f",
            g_Config.AiSkillScale, g_Config.AiGlueScale, g_Config.AiNosRechargeScale);
        Logger::Log("  PlayerNos: Recharge=%.2f  Bonus=%.2f  Strength=%.2f  Boost=%.2f",
            g_Config.PlayerNosRechargeScale, g_Config.PlayerNosBonusScale,
            g_Config.PlayerNosStrengthScale, g_Config.PlayerNosBoostScale);
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
