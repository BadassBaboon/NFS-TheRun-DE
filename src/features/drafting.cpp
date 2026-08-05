#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// Drafting suppression, player only, used by Run For Your Life.
//
// Drafting is the slipstream boost you build by sitting behind another car. It
// is written at the tail of fb::NFSVehicle::collectRaceCarInputState:
//
//     0x0069B65C  movss [esi+2B0h], xmm0     drafting
//     0x0069B680  movss [esi+2B4h], xmm0     draftingSpeed
//
// esi is the EA::VehiclePhysics::RaceCar::InputState. The All American Run cheat
// table writes [esi+2B0h] for its "Drafting Disable", which is a useful second
// opinion on the offset.
//
// PLAYER ONLY. Both fields are filled in for every car, so blanking them outright
// would take the slipstream away from the AI as well and hand the player an
// advantage — the same trap the driving assists fall into (see vehicle.cpp). The
// same function stores a flag one can key off:
//
//     0x0069AAD3  mov [esi+102h], al         isHumanPlayer
//
// so the cave only zeroes the fields when that byte is set, and AI cars draft
// exactly as they always did.
//
// The hook sits on the second write, which is 8 bytes and therefore has room for
// a 5-byte jump. By then both values have been computed, so the cave can clear
// drafting directly and substitute zero for the draftingSpeed about to be stored.
//
// Two things must not be disturbed. The x87 stack has a live value pushed at
// 0x0069B67A and popped at 0x0069B688, so the cave stays off the FPU entirely.
// Flags are clobbered, which is safe: nothing between here and the function's
// return reads them.

extern "C" {
    uint8_t   g_DisableDraft = 0;
    uintptr_t g_pDraftReturn = 0;
    void DraftHookAsm();
}

asm(
    ".text\n"
    ".globl _DraftHookAsm\n"
    "_DraftHookAsm:\n"
    "    cmpb $0, _g_DisableDraft\n"
    "    je   1f\n"                    // feature off -> stock behaviour
    "    cmpb $0, 0x102(%esi)\n"       // isHumanPlayer
    "    je   1f\n"                    // an AI car -> leave its draft alone
    "    xorps %xmm0, %xmm0\n"         // the draftingSpeed about to be stored
    "    movl $0, 0x2B0(%esi)\n"       // and the drafting value already stored
    "1:\n"
    "    movss %xmm0, 0x2B4(%esi)\n"   // the instruction this replaced
    "    jmpl *_g_pDraftReturn\n"
);

namespace {
    const uintptr_t kSiteDraft = 0x69B680 - 0x400000;
    const uint8_t   kExpectDraft[8] = { 0xF3, 0x0F, 0x11, 0x86, 0xB4, 0x02, 0x00, 0x00 };

    bool g_Installed = false;
    bool g_LoggedState = false;
    uint8_t g_LastState = 0xFF;
}

namespace Features {
    void InitDraftingControl() {
        if (!g_Config.RunForYourLife) return;

        uintptr_t addr = Memory::GetGameBase() + kSiteDraft;
        if (!Memory::VerifyBytes(addr, kExpectDraft, sizeof(kExpectDraft))) {
            Logger::Log("Drafting control: ABORTED at 0x%08X, bytes [%s] don't match.",
                        addr, Memory::BytesToHex(addr, sizeof(kExpectDraft)).c_str());
            return;
        }

        g_pDraftReturn = addr + sizeof(kExpectDraft);
        if (Memory::InjectJMP(addr, reinterpret_cast<uintptr_t>(DraftHookAsm), sizeof(kExpectDraft))) {
            Logger::Log("Drafting control: hook installed at 0x%08X.", addr);
            g_Installed = true;
        } else {
            Logger::Log("Drafting control: hook FAILED at 0x%08X.", addr);
        }
    }

    void SetDraftingDisabled(bool disabled) {
        if (!g_Installed) return;

        uint8_t want = disabled ? 1 : 0;
        g_DisableDraft = want;

        if (want != g_LastState) {
            if (g_LoggedState || want != 0) {
                Logger::Log("Drafting %s for the player.", want ? "disabled" : "restored");
                g_LoggedState = true;
            }
            g_LastState = want;
        }
    }
}
