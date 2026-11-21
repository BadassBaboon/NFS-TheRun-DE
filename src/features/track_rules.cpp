#include "features.h"
#include "patch_util.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <cstdint>
#include <cfloat>

extern "C" {
    uint8_t g_DisableOobTimer = 0;
    float g_OobDisabledDuration = FLT_MAX;
    uintptr_t g_OobReturn = 0;
    void OobTimerHookAsm();
}

// The caller stores ST(0) in a countdown and consumes it with fstp. NOPing
// its fld returned an empty x87 stack. Supply a non-expiring finite countdown
// while disabled, and replay the original load otherwise. Install only once.
asm(
    ".text\n"
    ".globl _OobTimerHookAsm\n"
    "_OobTimerHookAsm:\n"
    "    movl 0x14(%ecx), %ecx\n"
    "    pushfl\n"
    "    cmpb $0, _g_DisableOobTimer\n"
    "    je 1f\n"
    "    flds _g_OobDisabledDuration\n"
    "    jmp 2f\n"
    "1:\n"
    "    flds 0x68(%ecx)\n"
    "2:\n"
    "    popfl\n"
    "    jmpl *_g_OobReturn\n"
);

namespace {
    // mov ecx,[ecx+14h]; fld dword ptr [ecx+68h], followed by ret 4.
    const uintptr_t kOobOffset = 0x3FAA89;
    const uint8_t kOobOriginal[6] = { 0x8B, 0x49, 0x14, 0xD9, 0x41, 0x68 };

    // Set when InitTrackRules disables the countdown for good, because the INI asked for
    // it or the time-of-day feature needs it. Run For Your Life must not put it
    // back in that case, so its toggle stands down entirely.
    bool g_OobHeldOff = false;

    // Set only after the verified hook has been installed successfully.
    bool g_OobInstalled = false;
}

namespace Features {
    // Track-rule relaxations for free-roam / experimentation / speedrun sync.
    // All OFF by default (see INI) — they change core race behaviour and are
    // only desirable when the player opts in.
    //
    // Offsets derived from the DriftAttack / Master research tables
    // (absolute CT address - 0x00400000 = module offset):
    //   Checkpoint timer:  008FBF06 -> 0x4FBF06 (NOP 8),  013DB998 -> 0xFDB998 (NOP 5)
    //   Reset OOB:         007FAA89 -> 0x3FAA89 (hook 6)
    //   Wrong-way respawn: exe+408915 -> 0x408915 (fldz + padding)
    //
    // All signatures checked against the retail v1.1 executable before patching.
    void InitTrackRules() {
        if (g_Config.DisableCheckpointTimer) {
            static const uint8_t a[] = { 0xF3, 0x0F, 0x11, 0x81, 0x80, 0x05, 0x00, 0x00 };
            static const uint8_t b[] = { 0xF3, 0x0F, 0x11, 0x40, 0x04 };
            const uintptr_t base = Memory::GetGameBase();
            if (Memory::VerifyBytes(base + 0x4FBF06, a, sizeof(a)) &&
                Memory::VerifyBytes(base + 0xFDB998, b, sizeof(b))) {
                PatchUtil::VerifiedNop("Checkpoint Timer Disable (a)", 0x4FBF06, a, sizeof(a));
                PatchUtil::VerifiedNop("Checkpoint Timer Disable (b)", 0xFDB998, b, sizeof(b));
            } else {
                Logger::Log("Checkpoint timer patches skipped: signatures do not match.");
            }
        }

        // The time-of-day feature forces these two on regardless of the INI.
        // Several presets swap map assets, and the out-of-bounds and wrong-way
        // volumes are authored against the daytime layout, so a randomized level
        // triggers them where nothing is actually wrong. The tool this was ported
        // from disables both for the same reason, and lists it as a known issue.
        const bool todNeedsThem = g_Config.RandomizeTimeOfDay != 0;
        if (todNeedsThem) {
            Logger::Log("Track rules: OOB reset and wrong-way respawn disabled, required by "
                        "the time-of-day feature.");
        }

        g_OobHeldOff = g_Config.DisableResetOOB || todNeedsThem;
        if (g_OobHeldOff || g_Config.RunForYourLife) {
            const uintptr_t addr = Memory::GetGameBase() + kOobOffset;
            if (Memory::VerifyBytes(addr, kOobOriginal, sizeof(kOobOriginal))) {
                g_DisableOobTimer = g_OobHeldOff ? 1 : 0;
                g_OobReturn = addr + sizeof(kOobOriginal);
                g_OobInstalled = Memory::InjectJMP(addr,
                    reinterpret_cast<uintptr_t>(OobTimerHookAsm), sizeof(kOobOriginal));
            }
            Logger::Log("OOB countdown hook %s.", g_OobInstalled ? "installed" : "skipped/failed");
        }

        if (g_Config.DisableWrongWayRespawn || todNeedsThem) {
            // fld dword ptr [esi+0x2924]
            static const uint8_t kWrongWay[6] = { 0xD9, 0x86, 0x24, 0x29, 0x00, 0x00 };
            // +2924 is elapsed wrong-way time (incremented at 0x867BEB).
            // Pass zero to the consumer while preserving its subsequent fstp.
            static const uint8_t kWrongWayDisabled[6] = { 0xD9, 0xEE, 0x90, 0x90, 0x90, 0x90 };
            PatchUtil::VerifiedPatch("Wrong Way Respawn Disable", 0x408915,
                                     kWrongWay, kWrongWayDisabled, sizeof(kWrongWay));
        }
    }

    // Run For Your Life turns the out-of-bounds reset off while it is engaged.
    //
    // It is the one rule the mode RELAXES rather than tightens, and it is there
    // for the same reason the rest of the mode exists. With the AI quicker and
    // your car fragile, some events are close enough that the racing line alone
    // will not win them, so the answer has to be a better line — cutting a corner,
    // crossing a median, taking a route the event did not anticipate. The OOB
    // volume punishes exactly that, and it fires on geometry rather than on
    // anything you did wrong. Turning it off lets a creative route be a real
    // option instead of an instant reset.
    //
    // Unlike everything else in this file, this cannot be decided at init: the
    // difficulty is not known until the player picks one, and it can change
    // between events. Toggle the hook's data flag instead of rewriting live code.
    // A countdown already started retains its duration until the next trigger.
    void UpdateTrackRules() {
        // Something already turned the check off for good. Leave it alone rather
        // than restoring it out from under the setting that asked for it.
        if (g_OobHeldOff || !g_OobInstalled) return;
        if (!g_Config.RunForYourLife) return;

        const bool want = Difficulty::RunForYourLifeActive();
        if (want == (g_DisableOobTimer != 0)) return;
        g_DisableOobTimer = want ? 1 : 0;
        Logger::Log("Track rules: OOB countdown %s for subsequent triggers.",
                    want ? "disabled" : "restored");
    }
}
