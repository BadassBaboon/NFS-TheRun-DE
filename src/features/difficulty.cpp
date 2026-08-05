#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <cstdint>

// Run For Your Life — an enhanced version of the game's Extreme difficulty.
//
// The Run's four difficulties are baked in far too deeply to add a fifth. The
// menu is a fixed list of four items in an EBX asset, the RaceAIDifficulty enum
// has exactly four values, and the AI tuning data is a struct with four named
// blocks rather than an array, so a fifth value would index nothing. What can be
// done instead is to recognise Extreme at runtime and make it harder.
//
// Reading the current difficulty needs no hook. The function at exe+0x44A6B0 ends:
//
//     mov edi, [eax+2Ch]        ; the RaceAIDifficulty enum
//     mov eax, dword_288AC40
//     mov [eax+24h], edi        ; cached in a global
//
// so the live value is [[exe+0x248AC40] + 0x24]. Expert = 3 is confirmed by
// mRally2's "AI Difficulty Expert" script, which writes that constant.
//
// While the mode is active it owns the vehicle health, so [VEHICLE] VehicleHealth
// is ignored. That is deliberate: the point of the mode is a known, fixed set of
// rules, and letting the INI soften them would make "Run For Your Life" mean
// something different on every machine.
//
// The driving assists are NOT part of this mode, despite looking like an obvious
// fit. Testing showed they barely change the player's car while wrecking the AI,
// and the decompiler explains why — see the note in vehicle.cpp. Disabling them
// here would have made the race easier, not harder.

namespace {
    const uintptr_t kDifficultyGlobal = 0x248AC40; // module offset of dword_288AC40
    const uintptr_t kOffDifficulty    = 0x24;

    const int kDifficultyExpert = 3; // RaceAIDifficulty_Expert, the menu's "Extreme"

    bool g_Active = false;
    bool g_LoggedGlobal = false;
    int  g_LastSeen = -1;
}

// Captured by the control-check hook in fps_unlocker.cpp.
extern "C" uint8_t* g_pHasControl;

namespace Difficulty {
    // Returns the RaceAIDifficulty enum, or -1 before the game has set it.
    int GetCurrent() {
        uintptr_t slot = Memory::GetGameBase() + kDifficultyGlobal;
        if (!Memory::IsReadable(slot, sizeof(uintptr_t))) return -1;

        uintptr_t obj = *reinterpret_cast<uintptr_t*>(slot);
        if (obj < 0x10000) return -1;
        if (!Memory::IsReadable(obj + kOffDifficulty, sizeof(int32_t))) return -1;

        int value = *reinterpret_cast<int32_t*>(obj + kOffDifficulty);
        // The global is uninitialised until the first race is set up, so anything
        // outside the enum means "not known yet" rather than a difficulty.
        if (value < 0 || value > kDifficultyExpert) return -1;
        return value;
    }

    bool RunForYourLifeActive() { return g_Active; }
}

namespace Features {
    void UpdateDifficulty() {
        if (!g_Config.RunForYourLife) return;

        int difficulty = Difficulty::GetCurrent();
        if (difficulty < 0) return;

        if (!g_LoggedGlobal) {
            Logger::Log("Run For Your Life: reading difficulty from [[exe+0x%X]+0x%X].",
                        static_cast<unsigned>(kDifficultyGlobal), static_cast<unsigned>(kOffDifficulty));
            g_LoggedGlobal = true;
        }

        if (difficulty != g_LastSeen) {
            static const char* kNames[] = { "Easy", "Normal", "Hard", "Extreme" };
            Logger::Log("Difficulty is now %s (%d).", kNames[difficulty], difficulty);
            g_LastSeen = difficulty;
        }

        bool shouldBeActive = (difficulty == kDifficultyExpert);
        if (shouldBeActive != g_Active) {
            Logger::Log(shouldBeActive
                        ? "RUN FOR YOUR LIFE engaged: health capped at %.0f, "
                          "nitrous and drafting disabled, [VEHICLE] VehicleHealth ignored."
                        : "Run For Your Life disengaged: nitrous and drafting restored, [VEHICLE] applies again.",
                        Difficulty::kHealthCap);
            g_Active = shouldBeActive;
        }

        // Nitrous is a flag the per-frame hook reads, not a code patch, so it can
        // be switched at any moment including mid-race.
        SetNosDisabled(g_Active);
        SetDraftingDisabled(g_Active);
    }
}
