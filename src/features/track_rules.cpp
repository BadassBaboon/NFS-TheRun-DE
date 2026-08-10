#include "features.h"
#include "patch_util.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <cstdint>

namespace {
    // fld dword ptr [ecx+0x68] — the out-of-bounds check, three bytes.
    const uintptr_t kOobOffset = 0x3FAA8C;
    const uint8_t   kOobOriginal[3] = { 0xD9, 0x41, 0x68 };
    const uint8_t   kOobNopped[3]   = { 0x90, 0x90, 0x90 };

    // Set when InitTrackRules NOPs the check for good, because the INI asked for
    // it or the time-of-day feature needs it. Run For Your Life must not put it
    // back in that case, so its toggle stands down entirely.
    bool g_OobHeldOff = false;

    // What the mode last left the site as, so the patch is only written on a
    // transition rather than every tick.
    bool g_OobModeApplied = false;
}

namespace Features {
    // Track-rule relaxations for free-roam / experimentation / speedrun sync.
    // All OFF by default (see INI) — they change core race behaviour and are
    // only desirable when the player opts in.
    //
    // Offsets derived from the DriftAttack / Master research tables
    // (absolute CT address - 0x00400000 = module offset):
    //   Checkpoint timer:  008FBF06 -> 0x4FBF06 (NOP 8),  013DB998 -> 0xFDB998 (NOP 5)
    //   Reset OOB:         007FAA8C -> 0x3FAA8C (NOP 3)   fld [ecx+0x68]
    //   Wrong-way respawn: exe+408915 -> 0x408915 (NOP 6) fld [esi+0x2924]
    //
    // The OOB and wrong-way signatures were captured from a live run and are now
    // verified before the NOP goes in, so a different build refuses instead of
    // corrupting code. The two checkpoint-timer sites have not been observed yet
    // and still use CaptureNop, which logs the original bytes for promotion.
    void InitTrackRules() {
        if (g_Config.DisableCheckpointTimer) {
            PatchUtil::CaptureNop("Checkpoint Timer Disable (a)", 0x4FBF06, 8);
            PatchUtil::CaptureNop("Checkpoint Timer Disable (b)", 0xFDB998, 5);
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

        if (g_Config.DisableResetOOB || todNeedsThem) {
            PatchUtil::VerifiedNop("Reset OOB Disable", kOobOffset,
                                   kOobOriginal, sizeof(kOobOriginal));
            g_OobHeldOff = true;
        }

        if (g_Config.DisableWrongWayRespawn || todNeedsThem) {
            // fld dword ptr [esi+0x2924]
            static const uint8_t kWrongWay[6] = { 0xD9, 0x86, 0x24, 0x29, 0x00, 0x00 };
            PatchUtil::VerifiedNop("Wrong Way Respawn Disable", 0x408915, kWrongWay, sizeof(kWrongWay));
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
    // between events. So the three bytes are written and restored on transition.
    // Three bytes, no code cave, and it reverts cleanly the moment the mode
    // disengages — which is why this is worth doing as a live patch at all.
    void UpdateTrackRules() {
        // Something already turned the check off for good. Leave it alone rather
        // than restoring it out from under the setting that asked for it.
        if (g_OobHeldOff) return;
        if (!g_Config.RunForYourLife) return;

        const bool want = Difficulty::RunForYourLifeActive();
        if (want == g_OobModeApplied) return;

        const uintptr_t addr = Memory::GetGameBase() + kOobOffset;
        const uint8_t* from = want ? kOobOriginal : kOobNopped;
        const uint8_t* to   = want ? kOobNopped   : kOobOriginal;

        if (!Memory::VerifyBytes(addr, from, sizeof(kOobOriginal))) {
            // Someone else owns these bytes. Stop trying, and stop logging about
            // it, rather than fighting another mod every time the mode toggles.
            Logger::Log("Track rules: OOB reset site at 0x%08X reads [%s], not what this "
                        "build expects. Run For Your Life leaves it alone.",
                        addr, Memory::BytesToHex(addr, sizeof(kOobOriginal)).c_str());
            g_OobHeldOff = true;
            return;
        }

        if (Memory::PatchBytes(addr, to, sizeof(kOobOriginal))) {
            g_OobModeApplied = want;
            Logger::Log("Track rules: OOB reset %s at 0x%08X, Run For Your Life %s.",
                        want ? "disabled" : "restored", addr,
                        want ? "engaged" : "disengaged");
        }
    }
}
