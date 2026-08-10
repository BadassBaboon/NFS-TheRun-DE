#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// Traffic controls. Each site computes a traffic value and immediately consumes
// it; we hook the site, overwrite the game's value with our configured one, then
// run the original instruction(s). Ported from mRally2's Master Table code caves.
//
// Module offsets. The CT writes these as "…exe"+OFFSET, so the offset is used
// directly with GetGameBase() (verified against the exe: .text maps file-offset
// 1:1 to RVA, ImageBase 0x00400000, and each signature is unique in the binary):
//   Density scale : +E5EEF6  movss xmm0,[ecx]  ; and eax,-04   (7 bytes)
//   Max density   : +E5EEE9  movss xmm2,[eax+1C]               (5 bytes)  term
//                   +E5EFDE  movss xmm2,[eax+1C]               (5 bytes)  clamp
//   Vehicle limit : +E5A9A3  mov eax,[eax+60]  ; cmp eax,19    (6 bytes)

extern "C" {
    // Absolute override, driven by [TRAFFIC]. 0 leaves the ceiling alone.
    uint8_t   g_TrafficMaxDensityForce = 0;
    // Multiplier path, used by Run For Your Life. Mutually exclusive with the
    // absolute override above, which [TRAFFIC] drives.
    uint8_t   g_TrafficMaxDensityScaleOn = 0;
    float     g_TrafficMaxDensityMul     = 1.0f;
    float     g_TrafficDensityScaleVal = 0.05f;
    float     g_TrafficMaxDensityVal   = 0.15f;
    int32_t   g_TrafficVehicleLimitVal = 25;

    uintptr_t g_TrafficDensityReturn      = 0;
    uintptr_t g_TrafficMaxDensityReturn   = 0;
    uintptr_t g_TrafficMaxDensityReturn2  = 0;
    uintptr_t g_TrafficVehicleLimitReturn = 0;

    void TrafficDensityHookAsm();
    void TrafficMaxDensityHookAsm();
    void TrafficMaxDensityHookAsm2();
    void TrafficVehicleLimitHookAsm();
}

// Force [ecx] = our density, then replay: movss xmm0,[ecx] ; and eax,-04
asm(
    ".text\n"
    ".globl _TrafficDensityHookAsm\n"
    "_TrafficDensityHookAsm:\n"
    "    movss _g_TrafficDensityScaleVal, %xmm0\n"
    "    movss %xmm0, (%ecx)\n"
    "    andl $-4, %eax\n"
    "    jmpl *_g_TrafficDensityReturn\n"
);

// Max density. Two modes and, unlike the other two hooks, NO memory write.
//
// sub_125EEE0 reads [eax+0x1C] twice and uses it for two different things:
//
//     0x0125EEE9  movss xmm2,[eax+1Ch]    a term:  density = scale * maxDensity
//     0x0125EFDE  movss xmm2,[eax+1Ch]    the hard ceiling the result is clamped to
//
// so both reads have to agree or the calculation and the clamp disagree. An
// earlier version wrote the value into [eax+0x1C] once and let both reads pick it
// up, which works for an absolute override but is fatal for a multiplier: the
// site is hit repeatedly and each pass would multiply the already-multiplied
// value, compounding 0.15 to 0.225 to 0.34 and upward without limit.
//
// Scaling in the register instead is idempotent by construction. Nothing is
// written back, so the event's authored value stays intact and every pass starts
// from the same number. Both read sites carry an identical copy of this cave.
//
// The flag test writes EFLAGS, and neither site sets its own flags beforehand, so
// pushfl/popfl brackets it rather than assuming nothing downstream reads them.
#define TRAFFIC_MAXDENSITY_CAVE(NAME, RET)                     \
    ".text\n"                                                  \
    ".globl _" NAME "\n"                                       \
    "_" NAME ":\n"                                             \
    "    movss 0x1C(%eax), %xmm2\n"   /* the instruction this replaced */ \
    "    pushfl\n"                                             \
    "    cmpb $0, _g_TrafficMaxDensityForce\n"                 \
    "    je   1f\n"                                            \
    "    movss _g_TrafficMaxDensityVal, %xmm2\n"  /* absolute, from [TRAFFIC] */ \
    "    jmp  2f\n"                                            \
    "1:\n"                                                     \
    "    cmpb $0, _g_TrafficMaxDensityScaleOn\n"               \
    "    je   2f\n"                                            \
    "    mulss _g_TrafficMaxDensityMul, %xmm2\n"  /* multiplier, from the mode */ \
    "2:\n"                                                     \
    "    popfl\n"                                              \
    "    jmpl *_" RET "\n"

asm(TRAFFIC_MAXDENSITY_CAVE("TrafficMaxDensityHookAsm",  "g_TrafficMaxDensityReturn"));
asm(TRAFFIC_MAXDENSITY_CAVE("TrafficMaxDensityHookAsm2", "g_TrafficMaxDensityReturn2"));

// Force [eax+0x60] = our limit, then replay: mov eax,[eax+60] ; cmp eax,19
// edx is scratch; pop it before the cmp so its flags reach the returned-to code.
asm(
    ".text\n"
    ".globl _TrafficVehicleLimitHookAsm\n"
    "_TrafficVehicleLimitHookAsm:\n"
    "    pushl %edx\n"
    "    movl _g_TrafficVehicleLimitVal, %edx\n"
    "    movl %edx, 0x60(%eax)\n"
    "    popl %edx\n"
    "    movl 0x60(%eax), %eax\n"
    "    cmpl $0x19, %eax\n"
    "    jmpl *_g_TrafficVehicleLimitReturn\n"
);

namespace Features {
    // Verify the expected instruction bytes, then redirect to our cave.
    static void InstallTrafficHook(const char* name, uintptr_t offset, size_t size,
                                   const uint8_t* expected, uintptr_t* pReturn, void* cave) {
        uintptr_t addr = Memory::GetGameBase() + offset;
        if (!Memory::VerifyBytes(addr, expected, size)) {
            Logger::Log("%s ABORTED at 0x%08X: bytes [%s] don't match signature (wrong version/base?).",
                        name, addr, Memory::BytesToHex(addr, size).c_str());
            return;
        }
        *pReturn = addr + size;
        if (Memory::InjectJMP(addr, reinterpret_cast<uintptr_t>(cave), size)) {
            Logger::Log("%s installed at 0x%08X (%u bytes).", name, addr, static_cast<unsigned>(size));
        } else {
            Logger::Log("%s FAILED (VirtualProtect) at 0x%08X.", name, addr);
        }
    }

    void InitTrafficControls() {
        static const uint8_t densSig[7] = { 0xF3, 0x0F, 0x10, 0x01, 0x83, 0xE0, 0xFC };
        static const uint8_t maxdSig[5] = { 0xF3, 0x0F, 0x10, 0x50, 0x1C };
        static const uint8_t limSig[6]  = { 0x8B, 0x40, 0x60, 0x83, 0xF8, 0x19 };

        // Run For Your Life scales the density ceiling and nothing else, so it needs
        // only the two max-density hooks. Forcing the density scale itself would
        // override the value the game picks per event, flattening busy and quiet
        // roads to the same number.
        if (!g_Config.EnableTrafficControls) {
            if (g_Config.RunForYourLife) {
                InstallTrafficHook("Traffic Max Density (term)", 0xE5EEE9, 5, maxdSig,
                                   &g_TrafficMaxDensityReturn,
                                   reinterpret_cast<void*>(TrafficMaxDensityHookAsm));
                InstallTrafficHook("Traffic Max Density (clamp)", 0xE5EFDE, 5, maxdSig,
                                   &g_TrafficMaxDensityReturn2,
                                   reinterpret_cast<void*>(TrafficMaxDensityHookAsm2));
            } else {
                Logger::Log("EnableTrafficControls disabled in INI.");
            }
            return;
        }

        g_TrafficDensityScaleVal = g_Config.TrafficDensityScale;
        g_TrafficMaxDensityVal   = g_Config.TrafficMaxDensity;
        g_TrafficVehicleLimitVal = g_Config.TrafficVehicleLimit;
        g_TrafficMaxDensityForce = 1;
        Logger::Log("Traffic controls: DensityScale=%.3f MaxDensity=%.3f VehicleLimit=%d",
                    g_TrafficDensityScaleVal, g_TrafficMaxDensityVal, g_TrafficVehicleLimitVal);

        InstallTrafficHook("Traffic Density Scale", 0xE5EEF6, 7, densSig,
                           &g_TrafficDensityReturn, reinterpret_cast<void*>(TrafficDensityHookAsm));
        InstallTrafficHook("Traffic Max Density (term)", 0xE5EEE9, 5, maxdSig,
                           &g_TrafficMaxDensityReturn, reinterpret_cast<void*>(TrafficMaxDensityHookAsm));
        InstallTrafficHook("Traffic Max Density (clamp)", 0xE5EFDE, 5, maxdSig,
                           &g_TrafficMaxDensityReturn2, reinterpret_cast<void*>(TrafficMaxDensityHookAsm2));
        InstallTrafficHook("Traffic Vehicle Limit", 0xE5A9A3, 6, limSig,
                           &g_TrafficVehicleLimitReturn, reinterpret_cast<void*>(TrafficVehicleLimitHookAsm));
    }

    // Raises the ceiling only while the mode is engaged. When [TRAFFIC] is driving
    // things the INI value already won at startup and this leaves it alone.
    void UpdateTrafficControls() {
        if (!g_TrafficMaxDensityReturn) return;   // hook never installed

        // The mode wins over [TRAFFIC] rather than standing aside for it. An
        // earlier version returned early whenever EnableTrafficControls was set,
        // which let a player put TrafficMaxDensity at 0.05 and quietly soften the
        // difficulty. The whole point of the fixed values is that the INI cannot
        // do that.
        static bool loggedMode = false;
        const bool rfyl = Difficulty::RunForYourLifeActive();

        if (rfyl) {
            // Multiply what the event was authored with rather than replacing it,
            // so a quiet stretch stays quieter than a city one.
            g_TrafficMaxDensityMul     = Difficulty::kTrafficMaxDensityScale;
            g_TrafficMaxDensityScaleOn = 1;
            g_TrafficMaxDensityForce   = 0;   // the multiplier wins over [TRAFFIC]
            if (!loggedMode) {
                Logger::Log("Traffic: each event's max density multiplied by %.2f%s. The "
                            "density the game picks per event is untouched.",
                            Difficulty::kTrafficMaxDensityScale,
                            g_Config.EnableTrafficControls ? ", overriding [TRAFFIC]" : "");
                loggedMode = true;
            }
        } else if (g_Config.EnableTrafficControls) {
            // Hand the ceiling back to the INI's absolute value, which is what the
            // other two hooks have been using all along.
            g_TrafficMaxDensityVal     = g_Config.TrafficMaxDensity;
            g_TrafficMaxDensityForce   = 1;
            g_TrafficMaxDensityScaleOn = 0;
            loggedMode = false;
        } else {
            g_TrafficMaxDensityForce   = 0;   // pass the game's own value through
            g_TrafficMaxDensityScaleOn = 0;
            loggedMode = false;
        }
    }
}
