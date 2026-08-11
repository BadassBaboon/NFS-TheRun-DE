#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// Player-only overrides on the vehicle input state: drafting and driving assists.
//
// Both live in fb::NFSVehicle::collectRaceCarInputState, which IDA names, so the
// fields are not guesswork. esi is the EA::VehiclePhysics::RaceCar::InputState
// the function fills in each frame:
//
//     0x0069AAD3  mov   [esi+102h], al       isHumanPlayer
//     0x0069B1A9  mov   [esi+299h], al       alignToRoad                 (byte)
//     0x0069B48E  fstp  dword [esi+290h]     racelineAssistForceScalar
//     0x0069B4C4  fstp  dword [esi+294h]     racelineAssistTorqueScalar
//     0x0069B65C  movss [esi+2B0h], xmm0     drafting
//     0x0069B680  movss [esi+2B4h], xmm0     draftingSpeed
//
// WHY THIS REPLACED THE OLD ASSIST PATCHES. The previous approach came from the
// community cheat tables: thirteen byte patches that flipped branches in the
// physics code. Testing found they barely changed the player's car while the AI
// slid, crashed and fell off the pace, and the decompiler showed why. The
// raceline scalars are written for every vehicle with no isHumanPlayer check, so
// patching them degrades the AI, which leans on the racing line far harder than a
// human does. Worse, two of those patches did not disable anything at all — they
// inverted the isHumanPlayer test, handing the assist to the AI and taking it from
// the player. The net effect was an AI handicap that made races easier.
//
// This does what those patches were reaching for. One cave, keyed on the
// isHumanPlayer byte the function has already stored, blanks the fields for the
// player and leaves every AI car untouched.
//
// The hook sits on the LAST of those writes, at 0x0069B680. By then all the values
// have been computed, so a single cave can rewrite the ones already stored and
// substitute its own for the draftingSpeed about to be stored. Eight bytes there
// leaves room for the 5-byte jump.
//
// WHY DRAFTING IS SCALED RATHER THAN REMOVED. It was disabled outright first, and
// that was the same mistake the nitrous work made: drafting is a core mechanic,
// not a convenience, and taking it away removes a skill expression rather than
// demanding one. Slipstreaming well means holding a line directly behind a car at
// speed, which is hard, and the reward for doing it is the slingshot. So the mode
// halves the rate the draft builds at and leaves the payoff alone. You have to
// sit in it twice as long to earn the same slingshot.
//
// That split works for the same reason the nitrous one does: the accumulation and
// the payoff are separate fields. [esi+2B0h] is the per-frame draft contribution
// the game integrates into the meter — zeroing it removed drafting entirely,
// which is what proves it is the input side — and [esi+2B4h] is left untouched.
//
// Three constraints. The x87 stack has a live value pushed at 0x0069B67A and
// popped at 0x0069B688, so the cave stays off the FPU entirely. Flags are
// clobbered, which is safe: nothing between here and the function's return reads
// them. And xmm1 may be live across this site, so scaling — which needs a second
// SSE register, xmm0 being the draftingSpeed about to be stored — saves and
// restores it around the multiply rather than assuming it is free.

extern "C" {
    uint8_t   g_ScaleDraft = 0;
    float     g_DraftScale = 1.0f;
    uint8_t   g_DisablePlayerAssists = 0;
    uintptr_t g_pInputStateReturn = 0;
    void InputStateHookAsm();
}

asm(
    ".text\n"
    ".globl _InputStateHookAsm\n"
    "_InputStateHookAsm:\n"
    "    cmpb $0, 0x102(%esi)\n"              // isHumanPlayer?
    "    je   2f\n"                           // an AI car -> change nothing at all
    "    cmpb $0, _g_ScaleDraft\n"
    "    je   1f\n"
    "    subl $16, %esp\n"                    // borrow xmm1, then hand it back
    "    movups %xmm1, (%esp)\n"
    "    movss 0x2B0(%esi), %xmm1\n"          // the draft the game just computed
    "    mulss _g_DraftScale, %xmm1\n"        // built at our rate instead
    "    movss %xmm1, 0x2B0(%esi)\n"
    "    movups (%esp), %xmm1\n"
    "    addl $16, %esp\n"
    "1:\n"
    "    cmpb $0, _g_DisablePlayerAssists\n"
    "    je   2f\n"
    "    movl $0, 0x290(%esi)\n"              // racelineAssistForceScalar
    "    movl $0, 0x294(%esi)\n"              // racelineAssistTorqueScalar
    "    movb $0, 0x299(%esi)\n"              // alignToRoad
    "2:\n"
    "    movss %xmm0, 0x2B4(%esi)\n"          // the instruction this replaced
    "    jmpl *_g_pInputStateReturn\n"
);

namespace {
    const uintptr_t kSite = 0x69B680 - 0x400000;
    const uint8_t   kExpect[8] = { 0xF3, 0x0F, 0x11, 0x86, 0xB4, 0x02, 0x00, 0x00 };

    bool g_Installed = false;

    float   g_LastDraftScale = -1.0f;
    uint8_t g_LastAssists = 0xFF;
    bool    g_LoggedDraft = false;
    bool    g_LoggedAssists = false;

    // Only reports a change once the feature has actually been on, so the initial
    // "everything off" state does not produce a line on every boot.
    void Report(uint8_t want, uint8_t& last, bool& logged, const char* on, const char* off) {
        if (want == last) return;
        if (logged || want != 0) {
            Logger::Log("%s", want ? on : off);
            logged = true;
        }
        last = want;
    }
}

namespace Features {
    void InitInputStateHook() {
        // Nothing that uses this is switched on, so leave the game's code alone.
        if (!g_Config.RunForYourLife && !g_Config.DisablePlayerAssists
            && g_Config.PlayerDraftRateScale == 1.0f) return;

        uintptr_t addr = Memory::GetGameBase() + kSite;
        if (!Memory::VerifyBytes(addr, kExpect, sizeof(kExpect))) {
            Logger::Log("Input-state hook: ABORTED at 0x%08X, bytes [%s] don't match.",
                        addr, Memory::BytesToHex(addr, sizeof(kExpect)).c_str());
            return;
        }

        g_pInputStateReturn = addr + sizeof(kExpect);
        if (Memory::InjectJMP(addr, reinterpret_cast<uintptr_t>(InputStateHookAsm), sizeof(kExpect))) {
            Logger::Log("Input-state hook installed at 0x%08X (draft rate and player assists).", addr);
            g_Installed = true;
        } else {
            Logger::Log("Input-state hook FAILED at 0x%08X.", addr);
        }
    }

    void UpdateInputState() {
        if (!g_Installed) return;

        // Run For Your Life forces both on while engaged. Outside it the INI
        // decides, and the mode overrides whatever [VEHICLE] asked for rather
        // than combining with it — the same rule the rest of the mode follows.
        const bool rfyl = Difficulty::RunForYourLifeActive();

        float   draftScale = rfyl ? Difficulty::kPlayerDraftRateScale
                                  : g_Config.PlayerDraftRateScale;
        uint8_t assists    = (rfyl || g_Config.DisablePlayerAssists) ? 1 : 0;

        // A scale of exactly 1.0 means the cave leaves the field alone entirely,
        // rather than multiplying by one — so a disengaged mode is byte-for-byte
        // the stock game, not an arithmetic no-op that could still round.
        g_DraftScale = draftScale;
        g_ScaleDraft = (draftScale != 1.0f) ? 1 : 0;
        g_DisablePlayerAssists = assists;

        if (draftScale != g_LastDraftScale && (g_LoggedDraft || draftScale != 1.0f)) {
            if (draftScale == 1.0f) {
                Logger::Log("Player draft rate restored to stock.");
            } else {
                Logger::Log("Player draft rate x%.2f. The slingshot it pays out is "
                            "unscaled, and AI cars keep their full slipstream.", draftScale);
            }
            g_LoggedDraft = true;
        }
        g_LastDraftScale = draftScale;
        Report(assists, g_LastAssists, g_LoggedAssists,
               "Driving assists disabled for the player. AI cars keep theirs.",
               "Driving assists restored.");
    }
}
