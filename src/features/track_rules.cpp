#include "features.h"
#include "patch_util.h"
#include "../config.h"
#include "../logger.h"

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
    // No byte signatures are locked in yet: these fire in normal races, so the
    // first run logs the originals via CaptureNop for promotion to VerifiedNop.
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
            PatchUtil::CaptureNop("Reset OOB Disable", 0x3FAA8C, 3);
        }

        if (g_Config.DisableWrongWayRespawn || todNeedsThem) {
            PatchUtil::CaptureNop("Wrong Way Respawn Disable", 0x408915, 6);
        }
    }
}
