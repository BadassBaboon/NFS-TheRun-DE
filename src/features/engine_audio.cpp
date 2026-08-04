#include "features.h"
#include "patch_util.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// Engine-audio fix for high framerates, plus an optional Ginsu render-state
// sampler used to diagnose it. See InitEngineAudioSlewFix for the full writeup.

extern "C" {
    uintptr_t g_pGinsuDiagReturn = 0;
    void GinsuDiagHookAsm();
    void GinsuDiagSample(void* player);

    float     g_GinsuSlewThreshold = 1.0f;  // engine's own "slew complete" threshold
    uintptr_t g_pGinsuSlewReturn = 0;
    void GinsuSlewCaveAsm();
}

// Mode 2 cave. Replaces the 28-byte reset block at 0xD9D424:
//     fld [ebx+0x1C8] ; movss [ebx+0x1CC],xmm1 ; fstp [ebx+0x1C0] ; movss [ebx+0x1C4],xmm0
// i.e. prev = cur ; slewRem = total ; tgt = attr.
// We keep the glide but only re-arm it once the previous one has finished, so the
// interpolation ratio can actually advance instead of being reset to 0 every update.
// xmm0 (requested frequency) and xmm1 (total slew samples) must survive; xmm2 is
// reloaded by the instruction we return to, so it is free to clobber.
asm(
    ".text\n"
    ".globl _GinsuSlewCaveAsm\n"
    "_GinsuSlewCaveAsm:\n"
    "    movss 0x1CC(%ebx), %xmm2\n"          // xmm2 = slewSamplesRemaining
    "    comiss _g_GinsuSlewThreshold, %xmm2\n"
    "    jae 1f\n"                            // still gliding -> don't re-arm
    "    flds 0x1C8(%ebx)\n"                  // prev = cur
    "    fstps 0x1C0(%ebx)\n"
    "    movss %xmm1, 0x1CC(%ebx)\n"          // slewRem = total
    "1:\n"
    "    movss %xmm0, 0x1C4(%ebx)\n"          // tgt = requested frequency (always)
    "    jmpl *_g_pGinsuSlewReturn\n"
);

// ---------------------------------------------------------------------------
// [DIAGNOSTIC] Ginsu render-state sampler.
//
// Hooks the entry of the Ginsu voice-render function (0xD9D2D0). Its first stack
// argument is the GinsuPlayer*. We snapshot the fields that could carry the
// high-FPS coupling and log ~1x/sec per voice, including two derived rates:
//   calls/s   - how often the render runs (should track framerate)
//   advance/s - how fast mPlaybackPos walks the sample data (should depend on
//               PITCH ONLY, not framerate). If this differs between 30 and 144
//               FPS at the same RPM, that is the bug.
// Field offsets verified from the disassembly (see InitEngineAudioSlewFix notes).
// ---------------------------------------------------------------------------
namespace {
    struct GinsuTrack {
        void*    ptr;
        DWORD    lastLogMs;
        int32_t  lastPlaybackPos;
        uint32_t calls;
    };
    GinsuTrack g_ginsuTracks[4] = {};
}

extern "C" void GinsuDiagSample(void* player) {
    if (!g_Config.LogGinsuDiagnostics || player == nullptr) return;

    uint8_t* p = static_cast<uint8_t*>(player);
    int32_t samplesReq  = *reinterpret_cast<int32_t*>(p + 0x64);
    float   freqAttr    = *reinterpret_cast<float*>(p + 0x30);
    float   slewTimeMs  = *reinterpret_cast<float*>(p + 0x58);
    float   prevFreq    = *reinterpret_cast<float*>(p + 0x1C0);
    float   tgtFreq     = *reinterpret_cast<float*>(p + 0x1C4);
    float   curFreq     = *reinterpret_cast<float*>(p + 0x1C8);
    float   slewRemain  = *reinterpret_cast<float*>(p + 0x1CC);
    float   sampleRate  = *reinterpret_cast<float*>(p + 0x1D0);
    int32_t playbackPos = *reinterpret_cast<int32_t*>(p + 0x1E0);

    DWORD now = GetTickCount();

    GinsuTrack* t = nullptr;
    for (auto& e : g_ginsuTracks) { if (e.ptr == player) { t = &e; break; } }
    if (!t) {
        // Take a free slot, otherwise evict the stalest one. Without eviction, menu
        // and cutscene voices claim every slot before the race starts and the engine
        // voice is never tracked.
        GinsuTrack* victim = nullptr;
        for (auto& e : g_ginsuTracks) {
            if (e.ptr == nullptr) { victim = &e; break; }
            if (!victim || (now - e.lastLogMs) > (now - victim->lastLogMs)) victim = &e;
        }
        victim->ptr = player;
        victim->lastLogMs = now;
        victim->lastPlaybackPos = playbackPos;
        victim->calls = 0;
        t = victim;
    }

    ++t->calls;
    DWORD dt  = now - t->lastLogMs;
    if (dt < 1000) return;

    int32_t posDelta = playbackPos - t->lastPlaybackPos;
    float advance  = posDelta * 1000.0f / static_cast<float>(dt);
    float callRate = t->calls  * 1000.0f / static_cast<float>(dt);

    Logger::Log("[GINSU] p=%08X calls/s=%.0f req=%d sr=%.0f | attr=%.1f prev=%.1f tgt=%.1f cur=%.1f slewRem=%.1f slewMs=%.2f | pos=%d dPos=%d advance=%.0f/s",
                reinterpret_cast<uintptr_t>(player), callRate, samplesReq, sampleRate,
                freqAttr, prevFreq, tgtFreq, curFreq, slewRemain, slewTimeMs,
                playbackPos, posDelta, advance);

    t->lastLogMs = now;
    t->lastPlaybackPos = playbackPos;
    t->calls = 0;
}

// Sample state, then replay the stolen prologue:
//   sub esp,0x24 ; push ebx ; mov ebx,[esp+0x2C]
// At cave entry esp is unchanged, so arg1 sits at [esp+4]; after pushal(32)+
// pushfl(4) that becomes [esp+0x28].
asm(
    ".text\n"
    ".globl _GinsuDiagHookAsm\n"
    "_GinsuDiagHookAsm:\n"
    "    pushal\n"
    "    pushfl\n"
    "    movl 0x28(%esp), %eax\n"
    "    pushl %eax\n"
    "    call _GinsuDiagSample\n"
    "    addl $4, %esp\n"
    "    popfl\n"
    "    popal\n"
    "    subl $0x24, %esp\n"        // stolen: 83 EC 24
    "    pushl %ebx\n"              // stolen: 53
    "    movl 0x2C(%esp), %ebx\n"   // stolen: 8B 5C 24 2C
    "    jmpl *_g_pGinsuDiagReturn\n"
);

namespace Features {
    // Engine-audio frequency-slew fix.
    //
    // The Ginsu voice-render function (0xD9D2D0-0xD9DB3A) slews the synth pitch from
    // mfPreviousFrequency [ebx+0x1C0] to mfTargetFrequency [ebx+0x1C4], writing
    // mfCurrentFrequency [ebx+0x1C8], counting down mfSlewSamplesRemaining [ebx+0x1CC].
    // At 0xD9D3EC-0xD9D410 the total slew length is computed as
    //     totalSlewSamples = slewTimeMs[ebx+0x58] * sampleRate[ebx+0x1D0] / 1000
    // and it is RESET to that full value every time the requested frequency changes
    // (0xD9D424-0xD9D438).
    //
    // Diagnostic logging (LogGinsuDiagnostics) proved the mechanism:
    //   30 FPS  -> prev/tgt/cur are all live engine frequencies (sound correct)
    //   144 FPS -> tgt tracks RPM, but prev and cur are FROZEN at the 1000.0 init value
    //
    // On every frequency change the code resets prev = cur and slewRem = total, then
    // interpolates cur = prev + (tgt - prev) * (total - slewRem)/total. Immediately
    // after a reset that ratio is 0, so cur = prev, unchanged. At 30 FPS many render
    // calls elapse between RPM changes, so slewRem decrements, the ratio grows and cur
    // climbs toward tgt. Above 30 FPS a reset lands on nearly every block-run, the
    // ratio is always ~0, and cur never leaves its initial 1000 Hz. The synth plays a
    // fixed 1000 Hz regardless of RPM. Note this is independent of the slew LENGTH,
    // which is why resizing `total` (the earlier attempt at 0xD9D408) did nothing.
    void InitEngineAudioSlewFix() {
        if (!g_Config.FixEngineAudioSlew) {
            Logger::Log("FixEngineAudioSlew disabled in INI.");
            return;
        }

        if (g_Config.FixEngineAudioSlew == 2) {
            // Mode 2: keep the pitch glide, but only re-arm it once it has finished.
            // Redirect the whole 28-byte reset block into our cave.
            uintptr_t addr = Memory::GetGameBase() + 0x99D424;
            g_pGinsuSlewReturn = addr + 28; // 0xD9D440

            static const uint8_t expected28[28] = {
                0xD9, 0x83, 0xC8, 0x01, 0x00, 0x00,                    // fld   [ebx+0x1C8]
                0xF3, 0x0F, 0x11, 0x8B, 0xCC, 0x01, 0x00, 0x00,        // movss [ebx+0x1CC],xmm1
                0xD9, 0x9B, 0xC0, 0x01, 0x00, 0x00,                    // fstp  [ebx+0x1C0]
                0xF3, 0x0F, 0x11, 0x83, 0xC4, 0x01, 0x00, 0x00         // movss [ebx+0x1C4],xmm0
            };
            if (!Memory::VerifyBytes(addr, expected28, sizeof(expected28))) {
                Logger::Log("Engine-audio slew fix (mode 2) ABORTED at 0x%08X: bytes [%s] don't match.",
                            addr, Memory::BytesToHex(addr, 28).c_str());
                return;
            }
            if (Memory::InjectJMP(addr, reinterpret_cast<uintptr_t>(GinsuSlewCaveAsm), 28)) {
                Logger::Log("Engine-audio slew fix mode 2 installed at 0x%08X (glide preserved).", addr);
            } else {
                Logger::Log("Engine-audio slew fix (mode 2) FAILED at 0x%08X.", addr);
            }
            return;
        }

        // Mode 1: stop re-arming mfSlewSamplesRemaining outright. NOP the 8-byte
        //   movss [ebx+0x1CC], xmm1     (slewRem = totalSlewSamples)
        // at 0xD9D42A. slewRem then stays below the 1.0 "slew complete" threshold,
        // so the jb at 0xD9D457 takes the snap path and mfCurrentFrequency is set
        // straight from the requested frequency each update. Accurate pitch, but the
        // glide is gone, which costs the redline burble and the launch sweep.
        static const uint8_t expected[8] = { 0xF3, 0x0F, 0x11, 0x8B, 0xCC, 0x01, 0x00, 0x00 };
        static const uint8_t replacement[8] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

        PatchUtil::VerifiedPatch("Engine-audio slew fix mode 1", 0x99D42A,
                                 expected, replacement, sizeof(expected));
    }

    // [DIAGNOSTIC] Install the Ginsu render-state sampler.
    void InitGinsuDiagnostics() {
        if (!g_Config.LogGinsuDiagnostics) {
            Logger::Log("[GINSU] LogGinsuDiagnostics disabled in INI.");
            return;
        }

        uintptr_t addr = Memory::GetGameBase() + 0x99D2D0; // Ginsu voice render
        g_pGinsuDiagReturn = addr + 8;

        const uint8_t expected[8] = { 0x83, 0xEC, 0x24, 0x53, 0x8B, 0x5C, 0x24, 0x2C };
        if (!Memory::VerifyBytes(addr, expected, sizeof(expected))) {
            Logger::Log("[GINSU] diagnostics ABORTED at 0x%08X: bytes [%s] don't match (wrong version/base?).",
                        addr, Memory::BytesToHex(addr, 8).c_str());
            return;
        }
        if (Memory::InjectJMP(addr, reinterpret_cast<uintptr_t>(GinsuDiagHookAsm), 8)) {
            Logger::Log("[GINSU] diagnostics installed at 0x%08X (logging ~1x/sec per voice).", addr);
        } else {
            Logger::Log("[GINSU] diagnostics FAILED at 0x%08X.", addr);
        }
    }

}
