#include "features.h"
#include "tod_table.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// Time-of-day randomizer. Ported from _mRally2's TOD Randomizer cheat table.
//
// Every event in The Run is authored at one fixed time of day, so the twentieth
// run down a stretch looks exactly like the first. Randomizing it inside what
// each level actually supports makes the game feel less rehearsed without
// touching how it plays, which is why this is not tied to the difficulty mode.
//
// THE SITE. sub_99BEB0 is a state-transition handler, and on the transition into
// a level it does:
//
//     0x0099BF25  mov ecx, [eax+64h]     the time-of-day preset
//     0x0099BF28  push 1
//     0x0099BF31  call sub_E650D0        applies it
//
// Five bytes exactly, which is a jump with nothing left over. The cheat table
// hooks the same instruction to capture &[eax+0x64]; this goes further and does
// the work in place, so the value is already randomized by the time the original
// instruction loads it. Nothing has to race a ticker to get there first.
//
// The level is identified by a GUID four bytes below the object. The cheat table
// reads it as "[timeofday] - 0x68", and since timeofday is eax+0x64 that resolves
// to eax-4.
//
// WHY A TABLE AND NOT A RANDOM NUMBER. Levels do not all implement every preset.
// Picking one a level cannot render gives a broken or black scene, so each level
// has its own legal set. A GUID that is not in the table is left completely
// alone: an unknown level is not a licence to guess.
//
// The cave preserves everything. pushal/popfl bracket a plain cdecl call, so the
// C side can use whatever it likes without disturbing the game's registers or
// flags. The randomizer itself is integer-only and touches no floating point,
// which keeps it clear of the x87 and SSE state the surrounding code cares about.

extern "C" {
    // 0 off, 1 randomize within each level's legal set, 2 night everywhere.
    uint8_t   g_TodMode = 0;
    uintptr_t g_pTodReturn = 0;
    void TodHookAsm();
    void TodRandomizeC(void* obj);
}

asm(
    ".text\n"
    ".globl _TodHookAsm\n"
    "_TodHookAsm:\n"
    "    cmpb $0, _g_TodMode\n"
    "    je   1f\n"
    "    pushal\n"
    "    pushfl\n"
    "    pushl %eax\n"                    // the object; preset at +0x64, GUID at -4
    "    call _TodRandomizeC\n"
    "    addl $4, %esp\n"
    "    popfl\n"
    "    popal\n"
    "1:\n"
    "    movl 0x64(%eax), %ecx\n"         // the two instructions this replaced
    "    pushl $1\n"
    "    jmpl *_g_pTodReturn\n"
);

namespace {
    const uintptr_t kSite = 0x99BF25 - 0x400000;
    // mov ecx,[eax+64h] ; push 1
    const uint8_t kExpect[5] = { 0x8B, 0x48, 0x64, 0x6A, 0x01 };

    const intptr_t kOffPreset = 0x64;
    const intptr_t kOffGuid   = -0x04;

    const uint8_t kModeRandom = 1;
    const uint8_t kModeNight  = 2;

    // Night Run, from the source table's ForceNightVisEnv arm.
    const int32_t  kNightDefault         = 4;
    const uint32_t kGuidSanFrancisco     = 2702358496u;  // no night preset exists
    const uint32_t kGuidLasVegasEastA    = 2494877324u;
    const int32_t  kNightLasVegasEastA   = 3;

    bool g_Installed = false;
    bool g_Logged = false;

    // Own generator rather than rand(): this runs on the game's thread from inside
    // a detour, and it should not be sharing CRT state with anything else.
    uint32_t g_RngState = 0;

    uint32_t NextRandom() {
        if (g_RngState == 0) g_RngState = GetTickCount() | 1u;
        g_RngState ^= g_RngState << 13;
        g_RngState ^= g_RngState >> 17;
        g_RngState ^= g_RngState << 5;
        return g_RngState;
    }

    const TodEntry* Lookup(uint32_t guid) {
        for (size_t i = 0; i < kTodTableCount; ++i) {
            if (kTodTable[i].guid == guid) return &kTodTable[i];
        }
        return nullptr;
    }
}

extern "C" void TodRandomizeC(void* obj) {
    uintptr_t base = reinterpret_cast<uintptr_t>(obj);
    if (base < 0x10000) return;

    uintptr_t guidAddr   = base + kOffGuid;
    uintptr_t presetAddr = base + kOffPreset;
    if (!Memory::IsReadable(guidAddr, sizeof(uint32_t))) return;
    if (!Memory::IsReadable(presetAddr, sizeof(int32_t))) return;

    uint32_t guid = *reinterpret_cast<uint32_t*>(guidAddr);
    int32_t* preset = reinterpret_cast<int32_t*>(presetAddr);

    if (g_TodMode == kModeNight) {
        // Night is preset 4 nearly everywhere, with two levels the source table
        // singles out. Unlike the randomizer this is applied blind rather than
        // from a per-level list, which is what the original does too — night is
        // the one setting essentially every level implements.
        if (guid == kGuidSanFrancisco) {
            // The source fakes night here with a hand-built VisEnv, because this
            // level has no night preset at all. That VisEnv is nine more code
            // caves and is not ported, so writing its preset would change the
            // lighting without producing night. Left as the developers set it.
            return;
        }
        *preset = (guid == kGuidLasVegasEastA) ? kNightLasVegasEastA : kNightDefault;
        return;
    }

    const TodEntry* e = Lookup(guid);
    if (e == nullptr || e->count == 0) return;   // unknown level, leave it alone

    uint8_t chosen = e->presets[NextRandom() % e->count];
    *preset = static_cast<int32_t>(chosen);
}

namespace Features {
    void InitTodRandomizer() {
        if (!g_Config.RandomizeTimeOfDay) return;

        uintptr_t addr = Memory::GetGameBase() + kSite;
        if (!Memory::VerifyBytes(addr, kExpect, sizeof(kExpect))) {
            Logger::Log("Time of day: ABORTED at 0x%08X, bytes [%s] don't match.",
                        addr, Memory::BytesToHex(addr, sizeof(kExpect)).c_str());
            return;
        }

        g_pTodReturn = addr + sizeof(kExpect);
        if (Memory::InjectJMP(addr, reinterpret_cast<uintptr_t>(TodHookAsm), sizeof(kExpect))) {
            Logger::Log("Time of day: hook installed at 0x%08X, mode %d (%s), %u levels known.",
                        addr, g_Config.RandomizeTimeOfDay,
                        g_Config.RandomizeTimeOfDay == kModeNight ? "night" : "randomize",
                        static_cast<unsigned>(kTodTableCount));
            g_Installed = true;
        } else {
            Logger::Log("Time of day: hook FAILED at 0x%08X.", addr);
        }
    }

    void UpdateTodRandomizer() {
        if (!g_Installed) return;

        int mode = g_Config.RandomizeTimeOfDay;
        if (mode < 0 || mode > kModeNight) mode = 0;
        g_TodMode = static_cast<uint8_t>(mode);

        if (!g_Logged && mode != 0) {
            Logger::Log("Time of day: %s.",
                        mode == kModeRandom ? "randomizing within each level's own presets"
                                            : "night forced on every level that has one");
            g_Logged = true;
        }
    }
}
