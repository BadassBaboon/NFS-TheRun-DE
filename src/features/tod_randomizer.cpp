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
    uint8_t   g_TodEnabled = 0;
    uintptr_t g_pTodReturn = 0;
    void TodHookAsm();
    void TodRandomizeC(void* obj);
}

asm(
    ".text\n"
    ".globl _TodHookAsm\n"
    "_TodHookAsm:\n"
    "    cmpb $0, _g_TodEnabled\n"
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
    const TodEntry* e = Lookup(guid);
    if (e == nullptr || e->count == 0) return;   // unknown level, leave it alone

    uint8_t chosen = e->presets[NextRandom() % e->count];
    *reinterpret_cast<int32_t*>(presetAddr) = static_cast<int32_t>(chosen);
}

namespace Features {
    void InitTodRandomizer() {
        if (!g_Config.RandomizeTimeOfDay) return;

        uintptr_t addr = Memory::GetGameBase() + kSite;
        if (!Memory::VerifyBytes(addr, kExpect, sizeof(kExpect))) {
            Logger::Log("Time-of-day randomizer: ABORTED at 0x%08X, bytes [%s] don't match.",
                        addr, Memory::BytesToHex(addr, sizeof(kExpect)).c_str());
            return;
        }

        g_pTodReturn = addr + sizeof(kExpect);
        if (Memory::InjectJMP(addr, reinterpret_cast<uintptr_t>(TodHookAsm), sizeof(kExpect))) {
            Logger::Log("Time-of-day randomizer: hook installed at 0x%08X, %u levels known.",
                        addr, static_cast<unsigned>(kTodTableCount));
            g_Installed = true;
        } else {
            Logger::Log("Time-of-day randomizer: hook FAILED at 0x%08X.", addr);
        }
    }

    void UpdateTodRandomizer() {
        if (!g_Installed) return;

        uint8_t want = g_Config.RandomizeTimeOfDay ? 1 : 0;
        g_TodEnabled = want;

        if (!g_Logged && want) {
            Logger::Log("Time-of-day randomizer active.");
            g_Logged = true;
        }
    }
}
