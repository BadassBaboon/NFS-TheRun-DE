#include "features.h"
#include "patch_util.h"
#include "../config.h"
#include "../logger.h"

namespace Features {
    void InitCrashFixes() {
        if (!g_Config.FixKnownCrashes) {
            Logger::Log("FixKnownCrashes is disabled in INI.");
            return;
        }

        // Signatures captured from a known-good NFSTR 1.1.0.0 run (2026-08-02).
        // Each block is one complete instruction, so the NOP is aligned.
        // Module offsets (absolute at the default 0x00400000 base):
        //   0xA4EB60 -> 0x00E4EB60   Chicago Interstate crash 1: mov [eax+0x90], edx
        //   0xA50F0E -> 0x00E50F0E   Chicago Interstate crash 2: mov [edi+0x90], eax
        //   0xE1D23B -> 0x0121D23B   Tunnel of Pain crash:       cmp [esi], dx
        static const uint8_t chicago1[6] = { 0x89, 0x90, 0x90, 0x00, 0x00, 0x00 };
        static const uint8_t chicago2[6] = { 0x89, 0x87, 0x90, 0x00, 0x00, 0x00 };
        static const uint8_t tunnel[3]   = { 0x66, 0x39, 0x16 };

        if (g_Config.FixChicagoInterstateCrash) {
            PatchUtil::VerifiedNop("Chicago Interstate Crash Fix 1", 0xA4EB60, chicago1, sizeof(chicago1));
            PatchUtil::VerifiedNop("Chicago Interstate Crash Fix 2", 0xA50F0E, chicago2, sizeof(chicago2));
        } else {
            Logger::Log("Chicago Interstate Crash Fix SKIPPED (FixChicagoInterstateCrash=0).");
        }

        if (g_Config.FixTunnelOfPainCrash) {
            PatchUtil::VerifiedNop("Tunnel of Pain Crash Fix", 0xE1D23B, tunnel, sizeof(tunnel));
        } else {
            Logger::Log("Tunnel of Pain Crash Fix SKIPPED (FixTunnelOfPainCrash=0).");
        }
    }
}
