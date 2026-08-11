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
// WHY DRAFTING IS RAMPED RATHER THAN REMOVED OR SCALED. It was disabled outright
// first, and that was the same mistake the nitrous work made: drafting is a core
// mechanic, and taking it away removes a skill expression rather than demanding
// one. The intent is that you should have to hold the slipstream twice as long to
// earn the same slingshot.
//
// The second attempt multiplied [esi+2B0h] by 0.5 and was also wrong, which the
// telemetry below settled. That field is not a per-frame contribution to a meter,
// it is a NORMALISED 0..1 QUALITY that saturates:
//
//     0.0710 -> 0.1421 -> 0.2079 -> ... -> 0.9945 -> 1.0000
//
// so a flat multiply does not slow anything down. It caps the draft at half power
// permanently — hold a perfect slipstream for ten seconds and the game computes
// 1.0000 while the car receives 0.5000, for as long as you stay there. That is
// "half the benefit forever", not "twice as long to earn it", and it is why
// drafting felt dead and why the nitrous it feeds barely moved.
//
// So the scale is now a RAMP over time instead. The value starts at the
// configured fraction and climbs to the game's full value after the slipstream
// has been held continuously for kDraftRampMs. Break the draft and the timer
// resets. That is the original intent expressed against what the field actually
// is: the full slingshot is still available, it just has to be earned by holding
// a hard line rather than by brushing a bumper.
//
// [esi+2B4h] is left untouched throughout.
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
    // Telemetry, so "drafting feels like it does nothing" becomes a number rather
    // than a feeling. Captured for the PLAYER only, before and after scaling.
    float     g_DraftRaw     = 0.0f;
    float     g_DraftApplied = 0.0f;
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
    "    subl $16, %esp\n"                    // borrow xmm1, then hand it back
    "    movups %xmm1, (%esp)\n"
    "    movss 0x2B0(%esi), %xmm1\n"          // the draft the game just computed
    "    movss %xmm1, _g_DraftRaw\n"          // recorded before we touch it
    "    cmpb $0, _g_ScaleDraft\n"
    "    je   1f\n"
    "    mulss _g_DraftScale, %xmm1\n"        // built at our rate instead
    "    movss %xmm1, 0x2B0(%esi)\n"
    "1:\n"
    "    movss %xmm1, _g_DraftApplied\n"      // and after, scaled or not
    "    movups (%esp), %xmm1\n"
    "    addl $16, %esp\n"
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

    // How long a slipstream must be held continuously before the draft reaches
    // the game's full value. Below this it is interpolated up from the configured
    // fraction, so a brief draft still does something.
    const DWORD kDraftRampMs = 2000;
    const float kDraftActive  = 0.02f;   // raw above this counts as drafting

    // How long the draft may lapse without losing the ramp.
    //
    // Without this, one frame under the threshold throws away the whole ramp — a
    // log caught it going from x0.97 straight back to x0.50. Two things cause
    // that: the value simply flickers around the threshold at the edges of a
    // draft, and the game zeroes drafting outright whenever the car catches air.
    // A bump in the road is not a driving mistake, and it should not cost two
    // seconds of holding a hard line.
    //
    // Short enough that genuinely leaving the slipstream still resets, since
    // pulling out and coming back takes far longer than this.
    const DWORD kDraftGraceMs = 400;

    DWORD g_DraftHoldStart = 0;
    DWORD g_DraftLostAt    = 0;

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

        // Ramp the scale toward 1.0 the longer the slipstream is held. The cave
        // just multiplies by whatever is here, so all the timing lives on this
        // side where a clock is available.
        float applied = 1.0f;
        if (draftScale != 1.0f) {
            const DWORD now = GetTickCount();
            const bool drafting = (g_DraftRaw > kDraftActive);

            if (drafting) {
                if (g_DraftHoldStart == 0) g_DraftHoldStart = now;
                g_DraftLostAt = 0;
            } else if (g_DraftHoldStart != 0) {
                // Lapsed. Start the grace clock, and only give up on the ramp
                // once the draft has stayed gone long enough to be deliberate.
                if (g_DraftLostAt == 0) {
                    g_DraftLostAt = now;
                } else if (now - g_DraftLostAt >= kDraftGraceMs) {
                    g_DraftHoldStart = 0;
                    g_DraftLostAt = 0;
                }
            }

            float t = 0.0f;
            if (g_DraftHoldStart != 0) {
                const DWORD held = now - g_DraftHoldStart;
                t = (held >= kDraftRampMs) ? 1.0f
                                           : static_cast<float>(held) / kDraftRampMs;
            }
            applied = draftScale + (1.0f - draftScale) * t;
        }

        // A scale of exactly 1.0 means the cave leaves the field alone entirely,
        // rather than multiplying by one — so a disengaged mode is byte-for-byte
        // the stock game, not an arithmetic no-op that could still round.
        g_DraftScale = applied;
        g_ScaleDraft = (draftScale != 1.0f) ? 1 : 0;
        g_DisablePlayerAssists = assists;

        if (draftScale != g_LastDraftScale && (g_LoggedDraft || draftScale != 1.0f)) {
            if (draftScale == 1.0f) {
                Logger::Log("Player draft rate restored to stock.");
            } else {
                Logger::Log("Player draft starts at x%.2f and ramps to full over %lu ms of "
                            "held slipstream. AI cars keep theirs in full.",
                            draftScale, static_cast<unsigned long>(kDraftRampMs));
            }
            g_LoggedDraft = true;
        }
        g_LastDraftScale = draftScale;
        Report(assists, g_LastAssists, g_LoggedAssists,
               "Driving assists disabled for the player. AI cars keep theirs.",
               "Driving assists restored.");
    }
}
