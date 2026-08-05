#include "features.h"
#include "patch_util.h"
#include "../config.h"
#include "../logger.h"
#include <cstdint>

// Vehicle driving-assist removal. Ported from mRally2's "Disable All/Basic
// Vehicle Assists" and verified byte-for-byte against the 1.1.0.0 exe.
//
// The assists are EA::VehiclePhysics aids that read the road and nudge the car:
// RaceLineAssist (auto-line/grip), DriftIntentAnalyzer, RaceLineAnalyzer,
// RaceCarExtraForces, AlignToRoad, OverrideDriftIntent, Wallride grief protect.
// ERaceLineAssistStatus: RaceLineAssist_Off = 0, _Grip = 2 (from the ReClass map).
//
// Every patch verifies the original instruction before writing, so a version
// mismatch aborts instead of corrupting code. Offsets are module-relative
// (VA - 0x400000), used with GetGameBase().

namespace {
    struct BytePatch {
        const char* name;
        uintptr_t   offset;      // module offset (VA - 0x400000)
        uint8_t     expected[10];
        uint8_t     replacement[10];
        size_t      size;
    };

    // The four analyzer branches + the RaceLineStatus write. mRally2's "Basic".
    const BytePatch kBasicPatches[] = {
        // 0181A09B  je 0181A191 -> jne : disable DriftIntentAnalyzer
        {"Assist: DriftIntentAnalyzer",      0x141A09B, {0x0F,0x84,0xF0,0x00,0x00,0x00}, {0x0F,0x85,0xF0,0x00,0x00,0x00}, 6},
        // 0181A1EF  je 0181A24A -> jne : disable RaceLineAnalyzer
        {"Assist: RaceLineAnalyzer",         0x141A1EF, {0x74,0x59}, {0x75,0x59}, 2},
        // 0181A4D7  jne 0181A512 -> je : disable RaceCarExtraForces
        {"Assist: RaceCarExtraForces",       0x141A4D7, {0x75,0x39}, {0x74,0x39}, 2},
        // 0181A35D  jne 0181A3E9 -> je : disable RaceLineAssist state prep
        {"Assist: RaceLineAssist state prep",0x141A35D, {0x0F,0x85,0x86,0x00,0x00,0x00}, {0x0F,0x84,0x86,0x00,0x00,0x00}, 6},
        // 0181A43D  mov [ebx+4640],2 -> 0 : force RaceLineStatus = RaceLineAssist_Off
        {"Assist: RaceLineStatus=Off",       0x141A43D, {0xC7,0x83,0x40,0x46,0x00,0x00,0x02,0x00,0x00,0x00},
                                                        {0xC7,0x83,0x40,0x46,0x00,0x00,0x00,0x00,0x00,0x00}, 10},
    };

    // Everything "All" adds on top of Basic: forces, calc skips, road alignment.
    const BytePatch kAllExtraPatches[] = {
        // 0069B167  jne 0069B1A7 -> je : disable AlignToRoad
        {"Assist: AlignToRoad",              0x29B167, {0x75,0x3E}, {0x74,0x3E}, 2},
        // 0069B5E2  je 0069B60F -> jne : disable OverrideDriftIntent
        {"Assist: OverrideDriftIntent",      0x29B5E2, {0x74,0x2B}, {0x75,0x2B}, 2},
        // 0069B003  jne 0069B01E -> je : disable Wallride grief protection
        {"Assist: Wallride grief protect",   0x29B003, {0x75,0x19}, {0x74,0x19}, 2},
        // 01819981  mov [edi+50],2 -> 0 : RaceLineAssist status = Off
        {"Assist: RaceLineAssist status=Off",0x1419981, {0xC7,0x47,0x50,0x02,0x00,0x00,0x00},
                                                        {0xC7,0x47,0x50,0x00,0x00,0x00,0x00}, 7},
        // 018199A6  ja 01819CDC -> jmp 01819CDC (+nop) : skip RaceLineAssist calc
        {"Assist: skip RaceLineAssist calc", 0x14199A6, {0x0F,0x87,0x30,0x03,0x00,0x00}, {0xE9,0x31,0x03,0x00,0x00,0x90}, 6},
        // 01819AB1  je 01819CAC -> jne : disable all RaceLineAssist forces
        {"Assist: RaceLineAssist forces",    0x1419AB1, {0x0F,0x84,0xF5,0x01,0x00,0x00}, {0x0F,0x85,0xF5,0x01,0x00,0x00}, 6},
        // 0181AA64  call 0181A8E0 -> nop x5 : disable drift assist forces
        {"Assist: drift assist forces",      0x141AA64, {0xE8,0x77,0xFE,0xFF,0xFF}, {0x90,0x90,0x90,0x90,0x90}, 5},
        // 01828E73  je 018293A0 -> jmp 018293A0 (+nop) : skip DriftIntents calc
        {"Assist: skip DriftIntents calc",   0x1428E73, {0x0F,0x84,0x27,0x05,0x00,0x00}, {0xE9,0x28,0x05,0x00,0x00,0x90}, 6},
    };

    const size_t kBasicCount = sizeof(kBasicPatches) / sizeof(BytePatch);
    const size_t kExtraCount = sizeof(kAllExtraPatches) / sizeof(BytePatch);

    // VerifiedPatch is symmetric: swapping expected and replacement puts the
    // original instruction back, with the same signature check in the other
    // direction. Both forms are no-ops if the bytes are already what is wanted,
    // so a redundant call cannot corrupt anything.
    void ApplyGroup(const BytePatch* patches, size_t count, bool on) {
        for (size_t i = 0; i < count; ++i) {
            const BytePatch& p = patches[i];
            if (on) PatchUtil::VerifiedPatch(p.name, p.offset, p.expected, p.replacement, p.size);
            else    PatchUtil::VerifiedPatch(p.name, p.offset, p.replacement, p.expected, p.size);
        }
    }

    // 0 = stock, 1 = basic set, 2 = everything. Tracked so transitions only touch
    // the groups that actually change.
    int g_AssistLevel = 0;

    int ConfiguredAssistLevel() {
        if (g_Config.DisableAllVehicleAssists)   return 2;
        if (g_Config.DisableBasicVehicleAssists) return 1;
        return 0;
    }
}

namespace Features {
    void SetVehicleAssistLevel(int level) {
        if (level == g_AssistLevel) return;

        static const char* kNames[] = { "stock", "BASIC", "ALL" };
        Logger::Log("Vehicle assists: %s -> %s.", kNames[g_AssistLevel], kNames[level]);

        // The basic group is a subset of the full set, so each group is simply
        // driven to whether the new level needs it.
        ApplyGroup(kBasicPatches,    kBasicCount, level >= 1);
        ApplyGroup(kAllExtraPatches, kExtraCount, level >= 2);

        g_AssistLevel = level;
    }

    void InitVehicleAssists() {
        // When Run For Your Life is enabled it owns the assists outright, and the
        // level is chosen from the difficulty each tick instead. Leaving the code
        // untouched here keeps the two from writing the same bytes at boot.
        if (g_Config.RunForYourLife) {
            Logger::Log("Vehicle assists: managed by Run For Your Life, [VEHICLE] toggles ignored.");
            return;
        }

        int level = ConfiguredAssistLevel();
        if (level == 0) {
            Logger::Log("Vehicle assists: no changes (both toggles off).");
            return;
        }
        SetVehicleAssistLevel(level);
    }

    int VehicleAssistLevelFromConfig() { return ConfiguredAssistLevel(); }
}
