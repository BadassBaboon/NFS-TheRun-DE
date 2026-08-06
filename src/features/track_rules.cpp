#include "features.h"
#include "patch_util.h"
#include "../config.h"
#include "../logger.h"
#include <cstdint>

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
            // fld dword ptr [ecx+0x68]
            static const uint8_t kOob[3] = { 0xD9, 0x41, 0x68 };
            PatchUtil::VerifiedNop("Reset OOB Disable", 0x3FAA8C, kOob, sizeof(kOob));
        }

        if (g_Config.DisableWrongWayRespawn || todNeedsThem) {
            // fld dword ptr [esi+0x2924]
            static const uint8_t kWrongWay[6] = { 0xD9, 0x86, 0x24, 0x29, 0x00, 0x00 };
            PatchUtil::VerifiedNop("Wrong Way Respawn Disable", 0x408915, kWrongWay, sizeof(kWrongWay));
        }
    }
}
