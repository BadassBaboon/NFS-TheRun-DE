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
// PLAYER HOOK, 0x0069B601, twelve bytes. Scales three things:
//
//   nosRechargeScalar  the passive trickle, as it is stored here
//   nosStrengthScalar  stored earlier at 0x0069B5DC, for EVERY car -- scaling it
//                      here instead of at its own site is what makes it player-only
//   nosRechargeBonus   the reward for risky driving, stored just above at
//                      0x0069B5FB when one is earned
//
// The bonus is safe to scale here because this address is the join point of the
// branch that computes it: the jz at 0x0069B5EB jumps to exactly 0x0069B601, so
// this runs whether or not a reward was paid, and scaling a zero is still zero.
//
// WHAT THE BONUS IS. Found by watching the fields live rather than by reading the
// code: with the diagnostic on, nosRechargeBonus pulses to 3.0 and back to 0 in
// bursts that line up with near misses. requestNosBoostAmount, the obvious
// candidate, never fired at all -- that path only carries scripted grants.
//
// The recharge appears to be (base + bonus) * scalar rather than
// base * scalar + bonus, because dropping the scalar to 0 killed the reward NOS
// as well as the passive fill. That is why raising the bonus is the way to keep
// rewards intact while the trickle is turned down. The two are not independent
// dials: a bonus scale of exactly 1/scalar cancels the scalar out of the reward
// term, so a near miss pays what it pays in the stock game while passive
// accumulation stays at whatever the scalar says. The mode derives one from the
// other for that reason rather than tuning them separately.
//
// DRAFTING IS COUPLED TO THIS. The risky-driving reward for a slipstream is
// computed from the drafting field that input_state.cpp scales, which was
// established by accident: when the mode disabled drafting outright, the nitrous
// reward for drafting disappeared with it. So halving the draft rate also halves
// draft-earned nitrous. That is left as it is, and it is consistent — twice as
// long in the slipstream for the same total. Near misses and the oncoming lane
// are unaffected and still pay in full.
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
    // The player's RaceCar::InputState, captured for the field watch below. Only
    // ever written from the player branch, so it is never an AI car's.
    uintptr_t g_pPlayerInputState = 0;
    uintptr_t g_pNosBoostReturn = 0;
    void PlayerNosBoostHookAsm();

    float     g_PlayerNosRecharge = 1.0f;
    float     g_PlayerNosStrength = 1.0f;
    float     g_PlayerNosBonus    = 1.0f;
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
    "    movl  %esi, _g_pPlayerInputState\n"   // for the diagnostic; player's state only
    "    flds  0x132C(%edi)\n"                 // the car's own recharge scalar
    "    fmuls _g_PlayerNosRecharge\n"
    "    fstps 0x2A4(%esi)\n"                  // the instruction pair this replaced
    "    flds  0x2A8(%esi)\n"                  // strength, stored earlier for all cars
    "    fmuls _g_PlayerNosStrength\n"
    "    fstps 0x2A8(%esi)\n"                  // scaled here, so player only
    "    flds  0x2A0(%esi)\n"                  // the risky-driving reward
    "    fmuls _g_PlayerNosBonus\n"
    "    fstps 0x2A0(%esi)\n"
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
//
// PLAYER ONLY, and unlike the other two hooks this one has to check for itself.
// The player and AI hooks sit inside branches the game has already taken, but this
// write happens near the top of the function, before any of that:
//
//     0x0069AAD3  isHumanPlayer stored
//     0x0069AB46  requestNosBoostAmount stored   <- here, for EVERY vehicle
//     0x0069B5E2  the isHumanPlayer branch
//
// so without the check a setting named PlayerNosBoostScale would also scale the
// AI's scripted grants. The flag is already in place by this point, so the cave
// reads it directly.
//
// Clobbering EFLAGS is safe here: everything between the return address and the
// next compare at 0x0069AB5A is movss and mov, none of which read flags. The x87
// stack stays balanced on both paths, one push and one pop.
asm(
    ".text\n"
    ".globl _PlayerNosBoostHookAsm\n"
    "_PlayerNosBoostHookAsm:\n"
    "    flds  0xA78(%edi)\n"                 // the pending award
    "    cmpb $0, 0x102(%esi)\n"              // isHumanPlayer?
    "    je   1f\n"                           // an AI car -> pass its award through
    "    fmuls _g_PlayerNosBoost\n"
    "    fsts  _g_LastNosAward\n"             // peek for the log, without popping
    "1:\n"
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
    float g_LastBonus = -1.0f;
    float g_LastReportedAward = -1.0f;

    // Watches the four recharge inputs on the player's own input state.
    //
    // The award path turned out not to carry the risky-driving rewards: with
    // LogNosAwards on, a run full of near misses produced no award lines at all,
    // so requestNosBoostAmount is only used for scripted grants. The rewards must
    // therefore reach the bar through one of these instead, and this reports which
    // one moves when a near miss, an oncoming pass or a draft is scored.
    //
    // The values are read from the ticker rather than the detour, so the pointer
    // is range-checked every time: it is only as valid as the object behind it.
    //
    // It also has to re-check isHumanPlayer on every sample, not just trust that
    // the pointer was captured in the player branch. The game reuses this struct
    // for every vehicle it processes, so between the capture and the read the
    // object behind the pointer may have become an AI car. A run caught it doing
    // exactly that: samples reading strength=1.0000 when the player hook always
    // leaves 1.12 there, and scalar=2.0000, which is the AI recharge constant.
    // Those readings were correct about the car they described and wrong about
    // whose car it was — the same confusion the player/AI split exists to end.
    const uintptr_t kOffIsHumanPlayer    = 0x102;
    const uintptr_t kOffRechargeOverride = 0x29C;
    const uintptr_t kOffRechargeBonus    = 0x2A0;
    const uintptr_t kOffRechargeScalar   = 0x2A4;
    const uintptr_t kOffStrengthScalar   = 0x2A8;

    float g_WatchLast[4] = { -1.0f, -1.0f, -1.0f, -1.0f };

    void WatchRechargeFields() {
        uintptr_t s = g_pPlayerInputState;
        if (s < 0x10000) return;
        if (!Memory::IsReadable(s + kOffIsHumanPlayer, sizeof(uint8_t))) return;
        if (!Memory::IsReadable(s + kOffRechargeOverride, 4 * sizeof(float))) return;

        // Not the player's car any more. Skip rather than report an AI car's
        // nitrous economy under a heading that says "player".
        if (*reinterpret_cast<uint8_t*>(s + kOffIsHumanPlayer) == 0) return;

        const uintptr_t offs[4] = { kOffRechargeOverride, kOffRechargeBonus,
                                    kOffRechargeScalar, kOffStrengthScalar };
        static const char* kNames[4] = { "override", "bonus", "scalar", "strength" };

        float now[4];
        bool changed = false;
        for (int i = 0; i < 4; ++i) {
            now[i] = *reinterpret_cast<float*>(s + offs[i]);
            // Only a real move counts; these carry float noise frame to frame.
            if (now[i] < g_WatchLast[i] - 0.0005f || now[i] > g_WatchLast[i] + 0.0005f) {
                changed = true;
            }
        }
        if (!changed) return;

        Logger::Log("NOS fields: %s=%.4f  %s=%.4f  %s=%.4f  %s=%.4f",
                    kNames[0], now[0], kNames[1], now[1], kNames[2], now[2], kNames[3], now[3]);
        for (int i = 0; i < 4; ++i) g_WatchLast[i] = now[i];
    }

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
        if (!g_Config.RunForYourLife
            && g_Config.PlayerNosRechargeScale == 1.0f && g_Config.PlayerNosBonusScale == 1.0f
            && g_Config.PlayerNosStrengthScale == 1.0f && g_Config.PlayerNosBoostScale == 1.0f
            && g_Config.AiNosRechargeScale == 1.0f) return;
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
        // Recharge, bonus, strength and the AI rate are all fixed by the mode.
        // Only the scripted-grant scale is left out, so that one follows the INI
        // whether the mode is engaged or not.
        const bool rfyl = Difficulty::RunForYourLifeActive();
        float recharge = rfyl ? Difficulty::kPlayerNosRechargeScale : g_Config.PlayerNosRechargeScale;
        float bonus    = rfyl ? Difficulty::kPlayerNosBonusScale    : g_Config.PlayerNosBonusScale;
        float ai       = rfyl ? Difficulty::kAiNosRechargeScale     : g_Config.AiNosRechargeScale;
        float strength = rfyl ? Difficulty::kPlayerNosStrengthScale : g_Config.PlayerNosStrengthScale;
        float boost    = g_Config.PlayerNosBoostScale;

        g_PlayerNosRecharge = recharge;
        g_PlayerNosStrength = strength;
        g_AiNosRecharge     = ai;
        g_PlayerNosBoost    = boost;
        g_PlayerNosBonus    = bonus;

        // Reports the award the moment one is actually paid. This is how to tell
        // whether a near miss, an oncoming pass or a draft tops the bar up through
        // this path or through the passive recharge rate, which decides whether
        // PlayerNosBoostScale can do what it is meant to.
        if (g_Config.LogNosAwards) {
            float award = g_LastNosAward;
            if (award > 0.0f && award != g_LastReportedAward) {
                Logger::Log("NOS award: %.4f of a full bar (before scaling, x%.2f applied).",
                            award / (boost > 0.0f ? boost : 1.0f), boost);
                g_LastReportedAward = award;
            }
            WatchRechargeFields();
        }

        if ((recharge != g_LastRecharge || strength != g_LastStrength
             || bonus != g_LastBonus || ai != g_LastAi)
            && (g_Logged || rfyl)) {
            Logger::Log("NOS tuning: player recharge x%.2f, bonus x%.2f, strength x%.2f, "
                        "AI recharge %.2f.", recharge, bonus, strength, ai);
            g_Logged = true;
        }
        g_LastRecharge = recharge;
        g_LastStrength = strength;
        g_LastBonus = bonus;
        g_LastAi = ai;
    }
}
