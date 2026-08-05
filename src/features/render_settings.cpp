#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// fb::GameRenderSettings tweaks.
//
// The engine keeps its settings objects in a table of cached pointers. The
// accessor at 0x00418010 shows how GameRenderSettings is fetched:
//     container = dword_2753F28;
//     if (!container)
//         container = fb::SettingsManager::getContainer(g_settingsManager,
//                                                       &GameRenderSettings::c_TypeInfo);
// so the live object is simply *[0x02753F28] (module +0x2353F28). GameTimeSettings
// sits 8 bytes earlier in the same table at [0x02753F20].
//
// The pointer is populated lazily, so this runs from the ticker rather than at
// startup, and each field is only written when it differs from what's there.
//
// Field offsets confirmed live in ReClass against the running game:
//   0x0C float ForceRoll
//   0x14 float ForceRenderShiftX
//   0x20 float ForceFov                 (-1 = engine default, which is 48)
//   0x34 float ForceRenderShiftY
//   0x77 bool  InitialClearEnable
//   0x8B bool  ForceRenderShiftEnabled  (gates ForceRenderShiftX/Y)

// Captured by the control-check hook in fps_unlocker.cpp. Non-null once that hook
// is installed; nonzero while the player has control of the car.
extern "C" uint8_t* g_pHasControl;

namespace {
    const uintptr_t kSettingsPtrOffset = 0x2353F28; // -> [0x02753F28]

    // What the engine itself stores in ForceFov: -1 means "no override".
    const float kFovDisabled = -1.0f;

    const uintptr_t kOffForceRoll         = 0x0C;
    const uintptr_t kOffForceRenderShiftX = 0x14;
    const uintptr_t kOffForceFov          = 0x20;
    const uintptr_t kOffForceRenderShiftY = 0x34;
    const uintptr_t kOffInitialClear      = 0x77;
    const uintptr_t kOffForceShiftEnabled = 0x8B;

    bool g_LoggedApply = false;

    inline void WriteFloat(uintptr_t base, uintptr_t off, float v) {
        float* p = reinterpret_cast<float*>(base + off);
        if (*p != v) *p = v;
    }
    inline void WriteBool(uintptr_t base, uintptr_t off, bool v) {
        uint8_t* p = reinterpret_cast<uint8_t*>(base + off);
        uint8_t want = v ? 1 : 0;
        if (*p != want) *p = want;
    }
}

// ---------------------------------------------------------------------------
// fb::WorldRenderSettings — shadow settings.
//
// Unlike GameRenderSettings this has no static cached pointer, so it is resolved
// through the settings manager the same way settings_probe.cpp does. It also only
// exists once a level is loaded, so this quietly does nothing until then.
//
// IMPORTANT: these are read once when the renderer initialises a level, because
// they size the shadow render targets. Changing them mid-race does nothing. That
// is fine here because the ticker keeps writing them, so the values are already in
// place the next time a level loads.
//
// Motion blur, MSAA and the cascade slice count live in this same object and were
// wired up at one point, but testing showed none of them do anything in the retail
// build, so they were removed rather than shipped as settings that quietly fail.
namespace {
    const uintptr_t kSettingsManagerPtr   = 0x2446C74; // fb::g_settingsManager
    const uintptr_t kGetContainerFn       = 0x0E72D0;  // SettingsManager::getContainer
    const uintptr_t kWorldRenderTypeInfo  = 0x2AE6E98 - 0x400000;

    typedef uintptr_t (__fastcall *GetContainerFn)(uintptr_t self, uintptr_t edx, uintptr_t typeInfo);

    // WorldRenderSettings offsets, from the game's own reflection data.
    const uintptr_t kOffShadowmapResolution   = 0x044;
    const uintptr_t kOffShadowmapQuality      = 0x048;
    const uintptr_t kOffShadowmapViewDistance = 0x058;

    bool g_LoggedWorld = false;

    inline void WriteInt(uintptr_t base, uintptr_t off, int v) {
        int32_t* p = reinterpret_cast<int32_t*>(base + off);
        if (*p != v) *p = v;
    }

    void ApplyWorldRender() {
        if (!g_Config.EnableWorldRenderTweaks) return;

        uintptr_t gameBase = Memory::GetGameBase();
        uintptr_t mgr = *reinterpret_cast<uintptr_t*>(gameBase + kSettingsManagerPtr);
        if (mgr < 0x10000) return;

        GetContainerFn getContainer =
            reinterpret_cast<GetContainerFn>(gameBase + kGetContainerFn);
        uintptr_t w = getContainer(mgr, 0, gameBase + kWorldRenderTypeInfo);
        if (w < 0x10000) return;   // not registered until a level is loaded

        if (!g_LoggedWorld) {
            Logger::Log("World render tweaks: WorldRenderSettings at 0x%08X.", w);
            g_LoggedWorld = true;
        }

        if (g_Config.ShadowmapResolution   > 0) WriteInt(w, kOffShadowmapResolution,   g_Config.ShadowmapResolution);
        if (g_Config.ShadowmapQuality      >= 0) WriteInt(w, kOffShadowmapQuality,     g_Config.ShadowmapQuality);
        if (g_Config.ShadowmapViewDistance > 0.0f) WriteFloat(w, kOffShadowmapViewDistance, g_Config.ShadowmapViewDistance);
    }
}

namespace Features {
    void UpdateRenderSettings() {
        ApplyWorldRender();

        if (!g_Config.EnableRenderTweaks) return;

        uintptr_t slot = Memory::GetGameBase() + kSettingsPtrOffset;
        uintptr_t settings = *reinterpret_cast<uintptr_t*>(slot);
        if (settings < 0x10000) return; // not populated yet

        if (!g_LoggedApply) {
            Logger::Log("Render tweaks: GameRenderSettings at 0x%08X (via [0x%08X]).", settings, slot);
            g_LoggedApply = true;
        }

        // ForceFov applies globally, including the garage, car select and menus,
        // where a widened view is not wanted. Gate it on the same vehicle-control
        // flag the sim-rate clamp uses, so the override is live while driving and
        // released back to the engine's own FOV everywhere else. If the control
        // hook was never installed we cannot tell, so the override just stays on.
        if (g_Config.ForceFov > 0.0f) {
            bool applyFov = true;
            if (g_Config.ForceFovOnlyWhileDriving && g_pHasControl != nullptr) {
                applyFov = (*g_pHasControl != 0);
            }
            WriteFloat(settings, kOffForceFov, applyFov ? g_Config.ForceFov : kFovDisabled);
        }

        // Viewport shift. The enable flag gates both axes; on its own it lifts the
        // chase camera's viewport slightly.
        WriteBool(settings, kOffForceShiftEnabled, g_Config.ForceRenderShiftEnabled != 0);
        if (g_Config.ForceRenderShiftEnabled) {
            WriteFloat(settings, kOffForceRenderShiftX, g_Config.ForceRenderShiftX);
            WriteFloat(settings, kOffForceRenderShiftY, g_Config.ForceRenderShiftY);
        }

        // Camera roll works independently of the shift enable.
        WriteFloat(settings, kOffForceRoll, g_Config.ForceRoll);

        // InitialClearEnable: clears the render target each frame. Fixes the minimap
        // rendering glitchy, invisible or missing road segments on some events.
        if (g_Config.FixMinimapRendering) {
            WriteBool(settings, kOffInitialClear, true);
        }
    }
}
