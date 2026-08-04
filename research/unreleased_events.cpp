/*
 * PARKED — NOT COMPILED. Moved out of src/ during the Phase 1 audit (2026-08-02).
 *
 * Blockers before this can be re-integrated:
 *   1. References g_Config.UnlockUnreleasedEvents and g_Config.DisableTrackBoundResets,
 *      neither of which exists in ConfigStruct (src/config.h). Add them first.
 *   2. Declares Init/UpdateUnreleasedEvents, which are not in features.h and are
 *      never called from main.cpp. Wire them up (Init at startup, Update in the ticker).
 *   3. The pointer-injection approach (fabricating playlist event slots via AOB) is
 *      the suspected crash source. Re-enable incrementally with byte verification,
 *      exactly like the hardened crash_fixes/fps_unlocker now do.
 *
 * Include paths below assume this file's original location (src/features/).
 * They will not resolve from research/ until it's moved back and added to the build.
 */

#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstring>

static bool g_UnreleasedEventsInjected = false;
static DWORD g_InitTime = 0;
static DWORD g_LastUnreleasedScanTime = 0;

namespace Features {
    void InitUnreleasedEvents() {
        g_InitTime = GetTickCount();

        if (!g_Config.UnlockUnreleasedEvents && !g_Config.DisableTrackBoundResets) {
            Logger::Log("UnreleasedEvents feature disabled in INI.");
            return;
        }

        // 1. Apply Track Bound Engine Fixes (OOB & Wrong Way Resets)
        if (g_Config.DisableTrackBoundResets) {
            uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandle(NULL));

            // Disable Out-Of-Bounds Reset at "Need for Speed The Run.exe"+7FAA8C (3 bytes: fld dword ptr [ecx+68])
            // Module offset 0x007FAA8C = Base + 0x007FAA8C = 0x00BFAA8C
            uintptr_t oobAddr = base + 0x007FAA8C;
            if (Memory::PatchNOP(oobAddr, 3)) {
                Logger::Log("Disabled Out-Of-Bounds track auto-reset at 0x%08X (NOP 3 bytes)", oobAddr);
            } else {
                Logger::Log("Failed to patch Out-Of-Bounds reset at 0x%08X", oobAddr);
            }

            // Disable Wrong Way Respawn at "Need for Speed The Run.exe"+408915 (6 bytes: fld dword ptr [esi+0x2924])
            // Module offset 0x00408915 = Base + 0x00408915 = 0x00808915
            uintptr_t wrongWayAddr = base + 0x00408915;
            if (Memory::PatchNOP(wrongWayAddr, 6)) {
                Logger::Log("Disabled Wrong Way track respawn at 0x%08X (NOP 6 bytes)", wrongWayAddr);
            } else {
                Logger::Log("Failed to patch Wrong Way respawn at 0x%08X", wrongWayAddr);
            }
        }

        if (g_Config.UnlockUnreleasedEvents) {
            Logger::Log("UnreleasedEvents initialized. Waiting for menu assets in heap memory...");
        }
    }

    void UpdateUnreleasedEvents() {
        if (!g_Config.UnlockUnreleasedEvents || g_UnreleasedEventsInjected) {
            return;
        }

        DWORD now = GetTickCount();

        // 1. Wait at least 20 seconds - the BIK intro movie runs ~10-12s and we must
        //    not scan memory while the video decoder is actively streaming.
        if (now - g_InitTime < 20000) {
            return;
        }

        // 2. Throttle memory scanning to once per second
        if (now - g_LastUnreleasedScanTime < 1000) {
            return;
        }
        g_LastUnreleasedScanTime = now;

        // AOB Patterns for Unreleased Dev Events
        // TOP: Tunnel of Pain (_c4/Gameplay/ChallengeSeries/S02/S02_2)
        const char patternTOP[] = "_c4/Gameplay/ChallengeSeries/S02/S02_2\0ID_CS_COM";
        const char maskTOP[]    = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

        // DDJ: Desert Dash Jump (_c4/Gameplay/ChallengeSeries/S01/S01_5)
        const char patternDDJ[] = "_c4/Gameplay/ChallengeSeries/S01/S01_5\0ID_CS_COM";
        const char maskDDJ[]    = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

        // DVH: Death Valley Highway (_c4/Gameplay/ChallengeSeries/S13/S13_6)
        const char patternDVH[] = "_c4/Gameplay/ChallengeSeries/S13/S13_6\0ID_CS_COM";
        const char maskDVH[]    = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

        // CUT: Dev Cut Track (_c4/Gameplay/ChallengeSeries/S01/S01_3...**CUT**)
        const char patternCUT[] = "_c4/Gameplay/ChallengeSeries/S01/S01_3\0\0**CUT**";
        const char maskCUT[]    = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

        // Coastal Rush Playlist Container (_c4/Gameplay/ChallengeSeries/S01)
        const char patternS01[] = "_c4/Gameplay/ChallengeSeries/S01\0ID_CS_UNLOCK_CH";
        const char maskS01[]    = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

        // FindPatternProcess scans 0x00010000-0x20000000 — confirmed range where
        // Frostbite 2 allocates challenge series data (~0x05000000).
        uintptr_t addrTOP = Memory::FindPatternProcess(patternTOP, maskTOP);
        uintptr_t addrDDJ = Memory::FindPatternProcess(patternDDJ, maskDDJ);
        uintptr_t addrDVH = Memory::FindPatternProcess(patternDVH, maskDVH);
        uintptr_t addrCUT = Memory::FindPatternProcess(patternCUT, maskCUT);
        uintptr_t addrS01 = Memory::FindPatternProcess(patternS01, maskS01);

        if (addrTOP != 0 && addrDDJ != 0 && addrDVH != 0 && addrCUT != 0 && addrS01 != 0) {
            uintptr_t topRef = addrTOP - 0xB4;
            uintptr_t ddjRef = addrDDJ - 0xB4;
            uintptr_t dvhRef = addrDVH - 0xC0;
            uintptr_t cutRef = addrCUT - 0xB4;

            Logger::Log("UnreleasedEvents: patterns found, writing event slots...");
            Logger::Log("  S01 playlist base: 0x%08X", addrS01);
            Logger::Log("  TOP ref: 0x%08X  DDJ ref: 0x%08X  DVH ref: 0x%08X  CUT ref: 0x%08X",
                        topRef, ddjRef, dvhRef, cutRef);

            // Overwrite S01 playlist event slots (-0x14, -0x10, -0xC, -0x8)
            // These 4 DWORDs are pointers to the event struct objects in the playlist array
            bool ok1 = Memory::PatchBytes(addrS01 - 0x14, &ddjRef, sizeof(uintptr_t));
            bool ok2 = Memory::PatchBytes(addrS01 - 0x10, &dvhRef, sizeof(uintptr_t));
            bool ok3 = Memory::PatchBytes(addrS01 - 0x0C, &topRef, sizeof(uintptr_t));
            bool ok4 = Memory::PatchBytes(addrS01 - 0x08, &cutRef, sizeof(uintptr_t));

            Logger::Log("  Slot writes: ok1=%d ok2=%d ok3=%d ok4=%d", ok1, ok2, ok3, ok4);

            // NOTE: Cosmetic string patches (title/texture) removed pending crash diagnosis.
            // If we reach here without crashing, the event pointer injection is safe
            // and we can add string patches back incrementally.

            g_UnreleasedEventsInjected = true;
            Logger::Log("Unreleased Events injected (minimal mode).");
        } else {
            Logger::Log("UnreleasedEvents: one or more patterns not found yet (TOP=%d DDJ=%d DVH=%d CUT=%d S01=%d).",
                        addrTOP != 0, addrDDJ != 0, addrDVH != 0, addrCUT != 0, addrS01 != 0);
        }
    }
}
