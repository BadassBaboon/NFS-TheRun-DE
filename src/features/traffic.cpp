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
//   Max density   : +E5EEE9  movss xmm2,[eax+1C]               (5 bytes)
//   Vehicle limit : +E5A9A3  mov eax,[eax+60]  ; cmp eax,19    (6 bytes)

extern "C" {
    float     g_TrafficDensityScaleVal = 0.05f;
    float     g_TrafficMaxDensityVal   = 0.15f;
    int32_t   g_TrafficVehicleLimitVal = 25;

    uintptr_t g_TrafficDensityReturn      = 0;
    uintptr_t g_TrafficMaxDensityReturn   = 0;
    uintptr_t g_TrafficVehicleLimitReturn = 0;

    void TrafficDensityHookAsm();
    void TrafficMaxDensityHookAsm();
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

// Force [eax+0x1C] = our max density, then replay: movss xmm2,[eax+1C]
asm(
    ".text\n"
    ".globl _TrafficMaxDensityHookAsm\n"
    "_TrafficMaxDensityHookAsm:\n"
    "    movss _g_TrafficMaxDensityVal, %xmm2\n"
    "    movss %xmm2, 0x1C(%eax)\n"
    "    jmpl *_g_TrafficMaxDensityReturn\n"
);

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
        if (!g_Config.EnableTrafficControls) {
            Logger::Log("EnableTrafficControls disabled in INI.");
            return;
        }

        g_TrafficDensityScaleVal = g_Config.TrafficDensityScale;
        g_TrafficMaxDensityVal   = g_Config.TrafficMaxDensity;
        g_TrafficVehicleLimitVal = g_Config.TrafficVehicleLimit;
        Logger::Log("Traffic controls: DensityScale=%.3f MaxDensity=%.3f VehicleLimit=%d",
                    g_TrafficDensityScaleVal, g_TrafficMaxDensityVal, g_TrafficVehicleLimitVal);

        static const uint8_t densSig[7] = { 0xF3, 0x0F, 0x10, 0x01, 0x83, 0xE0, 0xFC };
        static const uint8_t maxdSig[5] = { 0xF3, 0x0F, 0x10, 0x50, 0x1C };
        static const uint8_t limSig[6]  = { 0x8B, 0x40, 0x60, 0x83, 0xF8, 0x19 };

        InstallTrafficHook("Traffic Density Scale", 0xE5EEF6, 7, densSig,
                           &g_TrafficDensityReturn, reinterpret_cast<void*>(TrafficDensityHookAsm));
        InstallTrafficHook("Traffic Max Density", 0xE5EEE9, 5, maxdSig,
                           &g_TrafficMaxDensityReturn, reinterpret_cast<void*>(TrafficMaxDensityHookAsm));
        InstallTrafficHook("Traffic Vehicle Limit", 0xE5A9A3, 6, limSig,
                           &g_TrafficVehicleLimitReturn, reinterpret_cast<void*>(TrafficVehicleLimitHookAsm));
    }
}
