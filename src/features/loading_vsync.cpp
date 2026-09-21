#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>
#include <cstring>

// Port of the hardcoded VSync bypass from mRally2's Framerate Unlocker table.
//
// Imported from the wefalltomorrow fork, which did the porting work. Unchanged
// apart from this note and the INI section it reads from.
//
// The original table writes to seven sites in the display/present setup routine:
//   004106F8, 004106FF, 00410706, 0041070B,
//   00410710, 00410717, 0041076F.
//
// The game contains absolute addresses in the mov-byte instructions. Rather than
// writing mRally2's literal 0x027xxxxx operands back into the executable, build
// the expected instructions from the live module base and patch only their final
// immediate byte. That keeps the patch correct if the image is relocated.
//
// This is intentionally independent of GameTime/MaxSimFps. It changes only the
// game's hardcoded display/VSync path and therefore cannot reintroduce the 2x
// simulation-speed regression seen when MaxSimFps was modified.

namespace {
    bool g_Installed = false;

    constexpr uintptr_t kImageBase = 0x00400000;
    constexpr uintptr_t kFlagAAbs  = 0x02714099;
    constexpr uintptr_t kFlagBAbs  = 0x02753F00;

    constexpr uintptr_t kSite1 = 0x004106F8;
    constexpr uintptr_t kSite2 = 0x004106FF;
    constexpr uintptr_t kSite3 = 0x00410706;
    constexpr uintptr_t kSite4 = 0x0041070B;
    constexpr uintptr_t kSite5 = 0x00410710;
    constexpr uintptr_t kSite6 = 0x00410717;
    constexpr uintptr_t kSite7 = 0x0041076F;

    void BuildMovByteAbs(uint8_t out[7], uintptr_t target, uint8_t value) {
        out[0] = 0xC6;
        out[1] = 0x05;
        const uint32_t p = static_cast<uint32_t>(target);
        std::memcpy(out + 2, &p, sizeof(p));
        out[6] = value;
    }

    bool VerifyMov(uintptr_t site, uintptr_t target, uint8_t value) {
        uint8_t expected[7];
        BuildMovByteAbs(expected, target, value);
        return Memory::VerifyBytes(site, expected, sizeof(expected));
    }

    bool PatchByte(uintptr_t address, uint8_t value) {
        return Memory::PatchBytes(address, &value, 1);
    }
}

namespace Features {
    void InitLoadingVSyncOptimization() {
        if (!g_Config.FastLoadingVSyncBypass) {
            Logger::Log("Fast-loading VSync bypass disabled in INI.");
            return;
        }

        const uintptr_t base = Memory::GetGameBase();
        if (!base) {
            Logger::Log("Fast-loading VSync bypass: game base unavailable.");
            return;
        }

        const uintptr_t delta = base - kImageBase;
        const uintptr_t flagA = kFlagAAbs + delta;
        const uintptr_t flagB = kFlagBAbs + delta;

        const uintptr_t s1 = kSite1 + delta;
        const uintptr_t s2 = kSite2 + delta;
        const uintptr_t s3 = kSite3 + delta;
        const uintptr_t s4 = kSite4 + delta;
        const uintptr_t s5 = kSite5 + delta;
        const uintptr_t s6 = kSite6 + delta;
        const uintptr_t s7 = kSite7 + delta;

        // Verify the complete original instruction set before changing anything.
        // This makes the patch all-or-nothing on unsupported executables.
        const uint8_t expectedJmp[5]   = { 0xE9, 0x8B, 0x00, 0x00, 0x00 };
        const uint8_t expectedStack[5] = { 0xC6, 0x44, 0x24, 0x13, 0x01 };

        bool ok = true;
        ok &= VerifyMov(s1, flagA, 1);
        ok &= VerifyMov(s2, flagB, 1);
        ok &= Memory::VerifyBytes(s3, expectedJmp, sizeof(expectedJmp));
        ok &= Memory::VerifyBytes(s4, expectedStack, sizeof(expectedStack));
        ok &= VerifyMov(s5, flagA, 1);
        ok &= VerifyMov(s6, flagB, 1);
        ok &= VerifyMov(s7, flagB, 0);

        if (!ok) {
            Logger::Log("Fast-loading VSync bypass skipped: the expected v1.1 display routine did not match.");
            Logger::Log("  0x%08X [%s]", s1, Memory::BytesToHex(s1, 7).c_str());
            Logger::Log("  0x%08X [%s]", s2, Memory::BytesToHex(s2, 7).c_str());
            Logger::Log("  0x%08X [%s]", s3, Memory::BytesToHex(s3, 5).c_str());
            Logger::Log("  0x%08X [%s]", s4, Memory::BytesToHex(s4, 5).c_str());
            Logger::Log("  0x%08X [%s]", s5, Memory::BytesToHex(s5, 7).c_str());
            Logger::Log("  0x%08X [%s]", s6, Memory::BytesToHex(s6, 7).c_str());
            Logger::Log("  0x%08X [%s]", s7, Memory::BytesToHex(s7, 7).c_str());
            return;
        }

        // Exact ENABLE state from mRally2's VSync entry:
        //   flag writes 1 -> 0 at sites 1/2/5/6
        //   skip the original jump at site 3
        //   stack-local byte 1 -> 0 at site 4
        //   final flag write 0 -> 1 at site 7
        ok  = PatchByte(s1 + 6, 0);
        ok &= PatchByte(s2 + 6, 0);
        ok &= Memory::PatchNOP(s3, 5);
        ok &= PatchByte(s4 + 4, 0);
        ok &= PatchByte(s5 + 6, 0);
        ok &= PatchByte(s6 + 6, 0);
        ok &= PatchByte(s7 + 6, 1);

        if (!ok) {
            Logger::Log("Fast-loading VSync bypass: one or more writes failed.");
            return;
        }

        g_Installed = true;
        Logger::Log("Fast-loading VSync bypass applied to the game's hardcoded display path (mRally2 port; no GameTime changes).");
    }
}
