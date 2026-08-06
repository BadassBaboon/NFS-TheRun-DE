#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// Nitrous tuning: scarcer and weaker for the player, more plentiful for the AI.
//
// This replaces the nitrous SUPPRESSION that used to be part of the mode and is
// now parked in research/nos.cpp. Removing nitrous outright made several timed and
// chase events close to unwinnable, because the game is designed around it. Making
// it a scarce resource asks the same question of the player without ever making an
// event impossible.
//
// The game splits these by driver already, which is what makes this clean. In
// fb::NFSVehicle::collectRaceCarInputState:
//
//     0x0069B5BD  cmp   byte [esp+13h], 0       the isHumanPlayer test
//     0x0069B5D6  fld   dword [edi+1330h]
//     0x0069B5DC  fstp  dword [esi+2A8h]        nosStrengthScalar, every car
//     0x0069B5E2  jz    <AI branch>
//     0x0069B601  fld   dword [edi+132Ch]       PLAYER branch
//     0x0069B607  fstp  dword [esi+2A4h]        nosRechargeScalar, from car tuning
//     0x0069B60F  movss xmm0, [023DE148h]       AI branch: the constant 1.0
//     0x0069B617  movss [esi+2A4h], xmm0        nosRechargeScalar = 1.0 for AI
//
// So the AI's recharge rate is a hardcoded 1.0 and the player's comes from the
// car. Two hooks, one per branch, and neither needs an isHumanPlayer check of its
// own because the game has already branched on it.
//
// PLAYER HOOK, 0x0069B601, twelve bytes. Scales the recharge scalar as it is
// stored, then scales the strength value that was already stored at 0x0069B5DC.
// Doing the second one here rather than at its own site is deliberate: 0x0069B5DC
// runs for every car, while this point is reached only by the player.
//
// AI HOOK, 0x0069B60F, eight bytes. Substitutes our own constant for the 1.0 the
// game loads. The store that follows is left alone and writes whatever we put in
// xmm0.
//
// ON FLAGS. The comparison at 0x0069B5BD sets the flags that the jz at 0x0069B5E2
// reads, thirty-seven bytes later. Nothing between them may disturb EFLAGS, which
// is fine for the game because every instruction in that span is x87 and x87 does
// not write flags. Both caves here sit past the jz, so they are clear of it — but
// anything added inside that window later has to respect it.
//
// The caves use x87 rather than SSE. Each fld is matched by an fstp, so the FPU
// stack is balanced, and no flags are touched either way.

extern "C" {
    float     g_PlayerNosBoost = 1.0f;
    float     g_LastNosAward = 0.0f;   // diagnostic peek, written every frame
    uintptr_t g_pNosBoostReturn = 0;
    void PlayerNosBoostHookAsm();

    float     g_PlayerNosRecharge = 1.0f;
    float     g_PlayerNosStrength = 1.0f;
    float     g_AiNosRecharge     = 1.0f;
    uintptr_t g_pPlayerNosReturn = 0;
    uintptr_t g_pAiNosReturn     = 0;
    void PlayerNosHookAsm();
    void AiNosHookAsm();
}

asm(
    ".text\n"
    ".globl _PlayerNosHookAsm\n"
    "_PlayerNosHookAsm:\n"
    "    flds  0x132C(%edi)\n"                 // the car's own recharge scalar
    "    fmuls _g_PlayerNosRecharge\n"
    "    fstps 0x2A4(%esi)\n"                  // the instruction pair this replaced
    "    flds  0x2A8(%esi)\n"                  // strength, stored earlier for all cars
    "    fmuls _g_PlayerNosStrength\n"
    "    fstps 0x2A8(%esi)\n"                  // scaled here, so player only
    "    jmpl *_g_pPlayerNosReturn\n"
);

// The instant award, 0x0069AB40. CollectPowerTrainDynamicState does
//
//     remainingNOSCapacity += requestNosBoostAmount * NOSstageCapacity
//
// so this tops the bar up directly and does NOT pass through nosRechargeScalar.
// It is the one path where a reward can be paid without the passive trickle rate
// being involved, which is what lets the two be tuned against each other.
//
// The game accumulates pending awards on the vehicle at +0xA78, hands the total
// over here, and clears it on the next instruction.
asm(
    ".text\n"
    ".globl _PlayerNosBoostHookAsm\n"
    "_PlayerNosBoostHookAsm:\n"
    "    flds  0xA78(%edi)\n"                 // the pending award
    "    fmuls _g_PlayerNosBoost\n"
    "    fsts  _g_LastNosAward\n"             // peek for the log, without popping
    "    fstps 0x60(%esi)\n"                  // the instruction pair this replaced
    "    jmpl *_g_pNosBoostReturn\n"
);

asm(
    ".text\n"
    ".globl _AiNosHookAsm\n"
    "_AiNosHookAsm:\n"
    "    movss _g_AiNosRecharge, %xmm0\n"      // replaces the hardcoded 1.0
    "    jmpl *_g_pAiNosReturn\n"
);

namespace {
    const uintptr_t kSiteBoost  = 0x69AB40 - 0x400000;
    // fld dword [edi+0A78h] ; fstp dword [esi+60h]
    const uint8_t kExpectBoost[9] = { 0xD9,0x87,0x78,0x0A,0x00,0x00, 0xD9,0x5E,0x60 };

    const uintptr_t kSitePlayer = 0x69B601 - 0x400000;
    const uintptr_t kSiteAi     = 0x69B60F - 0x400000;

    // fld dword [edi+132Ch] ; fstp dword [esi+2A4h]
    const uint8_t kExpectPlayer[12] = { 0xD9,0x87,0x2C,0x13,0x00,0x00,
                                        0xD9,0x9E,0xA4,0x02,0x00,0x00 };
    // movss xmm0, [023DE148h]
    const uint8_t kExpectAi[8] = { 0xF3,0x0F,0x10,0x05,0x48,0xE1,0x3D,0x02 };

    bool g_Installed = false;
    bool g_Logged = false;
    float g_LastRecharge = -1.0f, g_LastStrength = -1.0f, g_LastAi = -1.0f;
    float g_LastReportedAward = -1.0f;

    void InstallSite(const char* name, uintptr_t off, const uint8_t* expect, size_t size,
                     void* cave, uintptr_t* pReturn) {
        uintptr_t addr = Memory::GetGameBase() + off;
        if (!Memory::VerifyBytes(addr, expect, size)) {
            Logger::Log("NOS tuning: site %s ABORTED at 0x%08X, bytes [%s] don't match.",
                        name, addr, Memory::BytesToHex(addr, size).c_str());
            return;
        }
        *pReturn = addr + size;
        if (Memory::InjectJMP(addr, reinterpret_cast<uintptr_t>(cave), size)) {
            Logger::Log("NOS tuning: site %s installed at 0x%08X.", name, addr);
            g_Installed = true;
        } else {
            Logger::Log("NOS tuning: site %s FAILED at 0x%08X.", name, addr);
        }
    }
}

namespace Features {
    void InitNosTuning() {
        if (!g_Config.RunForYourLife) return;
        InstallSite("player", kSitePlayer, kExpectPlayer, sizeof(kExpectPlayer),
                    reinterpret_cast<void*>(PlayerNosHookAsm), &g_pPlayerNosReturn);
        InstallSite("ai",     kSiteAi,     kExpectAi,     sizeof(kExpectAi),
                    reinterpret_cast<void*>(AiNosHookAsm),     &g_pAiNosReturn);
        InstallSite("boost",  kSiteBoost,  kExpectBoost,  sizeof(kExpectBoost),
                    reinterpret_cast<void*>(PlayerNosBoostHookAsm), &g_pNosBoostReturn);
    }

    void UpdateNosTuning() {
        if (!g_Installed) return;

        // 1.0 everywhere reproduces the game's own behaviour exactly, including
        // the AI's hardcoded constant, so a disengaged mode changes nothing.
        const bool rfyl = Difficulty::RunForYourLifeActive();
        float recharge = rfyl ? g_Config.DeadlyPlayerNosRechargeScale : 1.0f;
        float strength = rfyl ? g_Config.DeadlyPlayerNosStrengthScale : 1.0f;
        float ai       = rfyl ? g_Config.DeadlyAiNosRechargeScale     : 1.0f;
        float boost    = rfyl ? g_Config.DeadlyPlayerNosBoostScale    : 1.0f;

        g_PlayerNosRecharge = recharge;
        g_PlayerNosStrength = strength;
        g_AiNosRecharge     = ai;
        g_PlayerNosBoost    = boost;

        // Reports the award the moment one is actually paid. This is how to tell
        // whether a near miss, an oncoming pass or a draft tops the bar up through
        // this path or through the passive recharge rate, which decides whether
        // DeadlyPlayerNosBoostScale can do what it is meant to.
        if (g_Config.LogNosAwards) {
            float award = g_LastNosAward;
            if (award > 0.0f && award != g_LastReportedAward) {
                Logger::Log("NOS award: %.4f of a full bar (before scaling, x%.2f applied).",
                            award / (boost > 0.0f ? boost : 1.0f), boost);
                g_LastReportedAward = award;
            }
        }

        if ((recharge != g_LastRecharge || strength != g_LastStrength || ai != g_LastAi)
            && (g_Logged || rfyl)) {
            Logger::Log("NOS tuning: player recharge x%.2f, player strength x%.2f, AI recharge %.2f.",
                        recharge, strength, ai);
            g_Logged = true;
        }
        g_LastRecharge = recharge;
        g_LastStrength = strength;
        g_LastAi = ai;
    }
}
