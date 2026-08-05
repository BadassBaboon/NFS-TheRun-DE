#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// Checkpoint resets — "rewinds", in the game's own vocabulary.
//
// _c4/gameplay/TheRun/TheRunInfo.xml (a StoryModeInfo) carries the career config:
//
//     Difficulties          = [Easy, Normal, Hard, Extreme]
//     RewindsPerDifficulty  = [ 10,    5,     3,     1    ]
//     UnlimitedRewindsValue = 0x63 (99)
//
// which matches the single reset Extreme actually gives you, and incidentally
// confirms the RaceAIDifficulty ordering difficulty.cpp relies on.
//
// This is an EBX asset instance rather than a settings container, so getContainer
// cannot reach it and there is no static pointer to follow. It is found by its
// contents instead: four consecutive int32s of 10, 5, 3, 1 make a sixteen-byte
// signature that is not likely to occur by accident.
//
// "Not likely" is not "cannot", so the search does not simply trust the first hit:
//
//   - Every match is logged with the dword that follows it, so the log shows what
//     was actually found rather than only what was changed.
//   - A match followed by 99 is taken as confirmed, because that is
//     UnlimitedRewindsValue sitting where the asset layout puts it.
//   - Failing that, a single unambiguous match is accepted.
//   - Several unconfirmed matches means the signature is not unique on this build,
//     and nothing is written at all. Guessing between them risks corrupting an
//     unrelated array, and a mode that quietly does the wrong thing is worse than
//     one that reports it cannot.
//
// Only index 3 is touched, so this can be applied regardless of the difficulty
// currently selected: entry 3 is Extreme's and nothing else reads it. Writing it
// early rather than waiting for the difficulty to be published also means the
// value is already in place before an event loads.

namespace {
    const int32_t kSignature[4] = { 10, 5, 3, 1 };      // RewindsPerDifficulty
    const int32_t kSigWithUnlimited[5] = { 10, 5, 3, 1, 99 }; // ...and the value after it

    const size_t kMaxHits = 16;
    const uintptr_t kOffExtreme = 3 * sizeof(int32_t);

    bool g_Resolved = false;
    bool g_Failed = false;
    uintptr_t g_Array = 0;

    int g_ScanTicks = 0;
    const int kTicksBetweenScans = 300;  // ~5s at the ticker's 16ms

    // Scanning the address space is not free, so this does not retry forever. If
    // the career data has not appeared in this many passes it is not going to.
    int g_Attempts = 0;
    const int kMaxAttempts = 24;

    // Reports every candidate before anything is written, then applies the rules
    // in the header comment.
    void Resolve() {
        uintptr_t hits[kMaxHits];

        // Look for the array together with UnlimitedRewindsValue first. Twenty
        // bytes is specific enough that a collision is not a real concern, and it
        // usually settles the question in one pass.
        size_t n = Memory::ScanWritableAll(kSigWithUnlimited, sizeof(kSigWithUnlimited),
                                           hits, kMaxHits);
        if (n == 1) {
            g_Array = hits[0];
            Logger::Log("Rewinds: found RewindsPerDifficulty followed by UnlimitedRewindsValue "
                        "at 0x%08X.", g_Array);
        } else if (n > 1) {
            for (size_t i = 0; i < n; ++i) Logger::Log("Rewinds: candidate at 0x%08X.", hits[i]);
            Logger::Log("Rewinds: %u places match the full signature, which should not happen. "
                        "Nothing written.", static_cast<unsigned>(n));
            g_Failed = true;
            return;
        } else {
            // Nothing with the trailing value. Either the career data is not loaded
            // yet, or the asset stores that field somewhere else, so fall back to
            // the array on its own and require it to be unambiguous.
            n = Memory::ScanWritableAll(kSignature, sizeof(kSignature), hits, kMaxHits);
            if (n == 0) return;   // not loaded yet; try again next pass

            for (size_t i = 0; i < n; ++i) Logger::Log("Rewinds: candidate at 0x%08X.", hits[i]);
            if (n > 1) {
                Logger::Log("Rewinds: %u candidates and none confirmed by a trailing 99. "
                            "Nothing written — the signature is not unique on this build.",
                            static_cast<unsigned>(n));
                g_Failed = true;
                return;
            }
            g_Array = hits[0];
            Logger::Log("Rewinds: one candidate and no UnlimitedRewindsValue after it. "
                        "Accepting it because it is unambiguous.");
        }

        if (!Memory::IsReadable(g_Array, sizeof(kSignature))) {
            g_Array = 0;
            return;
        }

        Logger::Log("Rewinds: RewindsPerDifficulty at 0x%08X. Extreme goes from %d to 0 — "
                    "any wreck or wrong turn now ends the run.",
                    g_Array, *reinterpret_cast<int32_t*>(g_Array + kOffExtreme));
        g_Resolved = true;
    }
}

namespace Features {
    void UpdateRewinds() {
        if (!g_Config.RunForYourLife || g_Failed) return;

        if (!g_Resolved) {
            // The career data is not loaded at boot, so this keeps looking rather
            // than giving up after a single pass.
            if (++g_ScanTicks < kTicksBetweenScans) return;
            g_ScanTicks = 0;

            if (++g_Attempts > kMaxAttempts) {
                Logger::Log("Rewinds: RewindsPerDifficulty not found after %d passes. Giving up; "
                            "Extreme keeps its one checkpoint reset.", kMaxAttempts);
                g_Failed = true;
                return;
            }
            Resolve();
            return;
        }

        // Re-applied rather than written once: the value is reloaded with the
        // career data. If the page goes away the asset was unloaded, so go back to
        // scanning instead of writing to memory that is no longer ours.
        if (!Memory::IsReadable(g_Array, sizeof(kSignature))) {
            Logger::Log("Rewinds: the array at 0x%08X is gone, searching again.", g_Array);
            g_Resolved = false;
            g_Array = 0;
            return;
        }

        int32_t* extreme = reinterpret_cast<int32_t*>(g_Array + kOffExtreme);
        if (*extreme != 0) *extreme = 0;
    }
}
