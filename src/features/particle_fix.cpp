#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// Kickup particle fix (snow spray, drift smoke, wet spray, dirt dust).
//
// THE BUG. Kickup emitters set, in their EBX data:
//     InheritSpeedAndDirectionFromEmitter = True
//     InheritSpeedScaleAmount             = ~0.759
// so each spawned particle inherits a slice of the emitter's own velocity. The
// emitter here is the wheel, which moves fast, and above 30 FPS that inherited
// velocity comes out over-scaled. The particles spawn in the right place but then
// shoot off far too fast, streaking sideways or straight up. It is repeatable at
// the same point on a track because the direction comes from the wheel's motion
// and the terrain there, which is identical every lap.
//
// Of the 897 emitters in the game data, 124 set the inherit flag. The reason only
// kickup looks broken is that the rest are either cutscene effects (which run at a
// clamped 30 FPS anyway) or effects on near-stationary emitters, where there is no
// meaningful velocity to over-scale.
//
// THE FIX. Inherited velocity is computed as
//     contribution = InheritSpeedScaleAmount * emitterVelocity
// so scaling the first term by the inverse of the over-scale cancels it. Two sites
// read that field, same code shape with different base registers:
//
//   0x01385D3B  movss xmm0,[esi+0x34]   guarded by cmp byte [esi+0x38],0 @ 0x01385D14
//   0x013895FB  movss xmm0,[eax+0x34]   guarded by cmp byte [eax+0x38],0 @ 0x013895D4
//
// Each is exactly 5 bytes, so each becomes a jump to a cave that loads the same
// field and multiplies it by our correction factor before continuing.
//
// This touches ONLY inherited emitter velocity. It does not alter the simulation
// rate, so unlike FixSimTickWhenDriving it cannot affect camera, collisions or input.

extern "C" {
    float     g_InheritVelScale = 1.0f;   // correction factor, updated per tick
    uintptr_t g_pInheritEsiReturn = 0;
    uintptr_t g_pInheritEaxReturn = 0;
    void InheritVelEsiHookAsm();
    void InheritVelEaxHookAsm();
}

asm(
    ".text\n"
    ".globl _InheritVelEsiHookAsm\n"
    "_InheritVelEsiHookAsm:\n"
    "    movss 0x34(%esi), %xmm0\n"
    "    mulss _g_InheritVelScale, %xmm0\n"
    "    jmpl *_g_pInheritEsiReturn\n"
);

asm(
    ".text\n"
    ".globl _InheritVelEaxHookAsm\n"
    "_InheritVelEaxHookAsm:\n"
    "    movss 0x34(%eax), %xmm0\n"
    "    mulss _g_InheritVelScale, %xmm0\n"
    "    jmpl *_g_pInheritEaxReturn\n"
);

namespace {
    const uintptr_t kSiteEsi = 0x1385D3B - 0x400000;
    const uintptr_t kSiteEax = 0x13895FB - 0x400000;

    const uint8_t kExpectEsi[5] = { 0xF3, 0x0F, 0x10, 0x46, 0x34 };
    const uint8_t kExpectEax[5] = { 0xF3, 0x0F, 0x10, 0x40, 0x34 };

    bool  g_Installed = false;
    float g_LastLoggedScale = -1.0f;

    void InstallSite(const char* name, uintptr_t off, const uint8_t* expect,
                     void* cave, uintptr_t* pReturn) {
        uintptr_t addr = Memory::GetGameBase() + off;
        if (!Memory::VerifyBytes(addr, expect, 5)) {
            Logger::Log("Kickup fix: site %s ABORTED at 0x%08X, bytes [%s] don't match.",
                        name, addr, Memory::BytesToHex(addr, 5).c_str());
            return;
        }
        *pReturn = addr + 5;
        if (Memory::InjectJMP(addr, reinterpret_cast<uintptr_t>(cave), 5)) {
            Logger::Log("Kickup fix: site %s installed at 0x%08X.", name, addr);
            g_Installed = true;
        } else {
            Logger::Log("Kickup fix: site %s FAILED at 0x%08X.", name, addr);
        }
    }
}

namespace Features {
    void InitParticleFix() {
        if (!g_Config.FixKickupParticles) {
            Logger::Log("FixKickupParticles disabled in INI.");
            return;
        }
        InstallSite("esi", kSiteEsi, kExpectEsi,
                    reinterpret_cast<void*>(InheritVelEsiHookAsm), &g_pInheritEsiReturn);
        InstallSite("eax", kSiteEax, kExpectEax,
                    reinterpret_cast<void*>(InheritVelEaxHookAsm), &g_pInheritEaxReturn);
    }

    void UpdateParticleFix() {
        if (!g_Config.FixKickupParticles || !g_Installed) return;

        float scale;
        if (g_Config.KickupVelocityScale > 0.0f) {
            // Manual override, for finding the right value by eye.
            scale = g_Config.KickupVelocityScale;
        } else {
            // Auto: the effects were authored at 30 FPS, so undo the ratio between
            // the running framerate and that. At 30 this is 1.0 and changes nothing.
            float fps = (g_Config.FPSLimit > 0) ? static_cast<float>(g_Config.FPSLimit) : 60.0f;
            scale = (fps > 30.0f) ? (30.0f / fps) : 1.0f;
        }

        g_InheritVelScale = scale;
        if (scale != g_LastLoggedScale) {
            Logger::Log("Kickup fix: inherited-velocity scale = %.4f", scale);
            g_LastLoggedScale = scale;
        }
    }
}
