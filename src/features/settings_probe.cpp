#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// [DIAGNOSTIC] Settings-container probe.
//
// Frostbite keeps one settings object per settings class, reachable at runtime via
//     fb::SettingsManager::getContainer(fb::g_settingsManager, &fb::XxxSettings::c_TypeInfo)
// (see sub_E68920, which uses it to print "WorldRender.Enable", "VisualTerrain.Enable",
// "EmitterSystem.Enable" and friends). Rather than hunting each class's cached pointer,
// we call that function directly and log where every container lives, so the addresses
// can be pasted straight into ReClass for exploring fields.
//
// Config prefix convention: class name minus the trailing "Settings"
//   GameRenderSettings -> "GameRender.", VisualTerrainSettings -> "VisualTerrain."
namespace {
    // Module offsets (VA - 0x400000).
    const uintptr_t kSettingsManagerPtr = 0x2446C74; // fb::g_settingsManager @ 0x02846C74
    const uintptr_t kGetContainerFn     = 0x0E72D0;  // fb::SettingsManager::getContainer @ 0x004E72D0

    struct SettingsClass {
        const char* name;
        uintptr_t   typeInfo; // module offset (VA - 0x400000)
    };

    // Curated list: the classes most likely to hold something player-visible.
    // The binary defines 73 in total; see docs/RESEARCH.md for the full list.
    const SettingsClass kClasses[] = {
        { "GameRenderSettings",        0x2AACCBC - 0x400000 },
        { "WorldRenderSettings",       0x2AE6E98 - 0x400000 },
        { "VisualEnvironmentSettings", 0x2AE6E4C - 0x400000 },
        { "GlobalPostProcessSettings", 0x2AA26AC - 0x400000 },
        { "VisualTerrainSettings",     0x2AAF720 - 0x400000 },
        { "VegetationSystemSettings",  0x2AE741C - 0x400000 },
        { "EmitterSystemSettings",     0x2AD2B34 - 0x400000 },
        { "DebrisSystemSettings",      0x2AE735C - 0x400000 },
        { "EnlightenRuntimeSettings",  0x2AE5FF8 - 0x400000 },
        { "OcclusionSettings",         0x2AE641C - 0x400000 },
        { "DecalSettings",             0x2AA1CA4 - 0x400000 },
        { "TextureSettings",           0x2AA0A38 - 0x400000 },
        { "TextureStreamingSettings",  0x2AA09E0 - 0x400000 },
        { "MeshSettings",              0x2AA22C8 - 0x400000 },
        { "ShaderSystemSettings",      0x2AA3428 - 0x400000 },
        { "DxDisplaySettings",         0x2AA0884 - 0x400000 },
        { "DebugRenderSettings",       0x2A96600 - 0x400000 },
        { "PhysicsSettings",           0x2A9EB70 - 0x400000 },
        { "AudioSettings",             0x2A9A2D4 - 0x400000 },
        { "SoundSettings",             0x2ABD55C - 0x400000 },
        { "GameSettings",              0x2AB5484 - 0x400000 },
        { "NfsGameSettings",           0x2ADF658 - 0x400000 },
        { "EffectManagerSettings",     0x2ABD588 - 0x400000 },
        { "GameTimeSettings",          0x2AB54DC - 0x400000 }, // sanity check: expect [0x02753F20]
    };
    const int kClassCount = sizeof(kClasses) / sizeof(kClasses[0]);
    bool g_Resolved[kClassCount] = {};

    // getContainer is __thiscall(this, const TypeInfo*). MinGW has no __thiscall for
    // free functions, but __fastcall matches it here: ecx carries `this`, edx is
    // ignored by the callee, the remaining argument is on the stack, and both
    // conventions have the callee clean up.
    typedef uintptr_t (__fastcall *GetContainerFn)(uintptr_t self, uintptr_t edx, uintptr_t typeInfo);

    bool  g_Probed = false;
    int   g_Attempts = 0;
    DWORD g_LastTryMs = 0;
    int   g_LastControl = -1;
}

// Captured by the control-check hook in fps_unlocker.cpp.
extern "C" uint8_t* g_pHasControl;

namespace Features {
    void UpdateSettingsProbe() {
        if (!g_Config.LogSettingsContainers || g_Probed) return;

        // World subsystems (emitters, world render, post process, vegetation) only
        // register once a level is loaded, so the useful moment to sample is when the
        // player actually has control of the car. Probe on that transition as well as
        // on a timer, so a misbehaving clock cannot cost us the gameplay sample.
        DWORD now = GetTickCount();
        DWORD sinceLast = now - g_LastTryMs;

        int ctl = (g_pHasControl != nullptr && *g_pHasControl != 0) ? 1 : 0;
        bool controlChanged = (ctl != g_LastControl);
        g_LastControl = ctl;

        // Event-driven only. Every elapsed-time gate tried here misbehaved: the probe
        // reported ~15s between attempts that the log timestamps show happening in the
        // same second, so it exhausted its retries long before a level was loaded. The
        // cause was never pinned down (the same clock times the control-hook delay in
        // fps_unlocker.cpp correctly), so this sidesteps it: control transitions are
        // the signal that actually matters and they need no clock at all.
        bool firstRun = (g_Attempts == 0);
        if (!firstRun && !controlChanged) return;

        g_LastTryMs = now;
        if (++g_Attempts > 40) {
            Logger::Log("Settings probe finished; some containers never registered.");
            g_Probed = true;
            return;
        }

        uintptr_t base = Memory::GetGameBase();
        uintptr_t mgrSlot = base + kSettingsManagerPtr;
        uintptr_t mgr = *reinterpret_cast<uintptr_t*>(mgrSlot);

        Logger::Log("=== Settings containers (attempt %d, %ums since last, driving=%d) ===",
                    g_Attempts, sinceLast, ctl);
        Logger::Log("  g_settingsManager slot 0x%08X holds 0x%08X", mgrSlot, mgr);

        if (mgr < 0x10000) {
            Logger::Log("  manager not ready, will retry.");
            return;
        }

        GetContainerFn getContainer = reinterpret_cast<GetContainerFn>(base + kGetContainerFn);

        // Try both interpretations of the symbol: the value stored at the slot, and
        // the slot address itself (in case g_settingsManager IS the object). Whichever
        // resolves GameTimeSettings to the known-good [0x02753F20] is the right one.
        uintptr_t knownGood = *reinterpret_cast<uintptr_t*>(base + 0x2353F20); // GameTimeSettings
        Logger::Log("  reference: GameTimeSettings via cached ptr = 0x%08X", knownGood);

        uintptr_t thisCandidates[2] = { mgr, mgrSlot };
        int good = -1;
        for (int i = 0; i < 2 && good < 0; ++i) {
            uintptr_t got = getContainer(thisCandidates[i], 0, base + kClasses[kClassCount - 1].typeInfo);
            if (got != 0 && got == knownGood) good = i;
        }
        if (good < 0) {
            Logger::Log("  cannot resolve containers with either form of `this`; will retry.");
            return;
        }

        int found = 0, missing = 0;
        for (int i = 0; i < kClassCount; ++i) {
            uintptr_t container = getContainer(thisCandidates[good], 0, base + kClasses[i].typeInfo);
            if (container >= 0x10000) {
                ++found;
                if (!g_Resolved[i]) {           // only report the first time it appears
                    g_Resolved[i] = true;
                    Logger::Log("  %-26s -> 0x%08X", kClasses[i].name, container);
                }
            } else {
                ++missing;
            }
        }
        Logger::Log("  (%d resolved, %d still unregistered)", found, missing);

        if (missing == 0) {
            Logger::Log("=== all settings containers resolved ===");
            g_Probed = true;
        }
    }
}
