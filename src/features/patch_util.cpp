#include "patch_util.h"
#include "../memory.h"
#include "../logger.h"
#include <string>

namespace PatchUtil {
    bool VerifiedNop(const char* name, uintptr_t offset,
                     const uint8_t* expected, size_t size) {
        uintptr_t addr = Memory::GetGameBase() + offset;
        std::string before = Memory::BytesToHex(addr, size);

        if (Memory::VerifyBytes(addr, expected, size)) {
            if (Memory::PatchNOP(addr, size)) {
                Logger::Log("%s applied at 0x%08X (NOP %u bytes, was [%s])",
                            name, addr, static_cast<unsigned>(size), before.c_str());
                return true;
            }
            Logger::Log("%s FAILED (VirtualProtect) at 0x%08X (was [%s])", name, addr, before.c_str());
            return false;
        }

        if (before != "<unreadable>" && before.find_first_not_of("90 ") == std::string::npos) {
            Logger::Log("%s SKIPPED at 0x%08X: already NOP [%s] (already patched?).", name, addr, before.c_str());
        } else {
            Logger::Log("%s SKIPPED at 0x%08X: signature mismatch, got [%s] (wrong game version/base?).",
                        name, addr, before.c_str());
        }
        return false;
    }

    bool CaptureNop(const char* name, uintptr_t offset, size_t size) {
        uintptr_t addr = Memory::GetGameBase() + offset;
        std::string before = Memory::BytesToHex(addr, size);

        if (before == "<unreadable>") {
            Logger::Log("%s SKIPPED at 0x%08X: region unreadable (wrong base/version?).", name, addr);
            return false;
        }
        if (before.find_first_not_of("90 ") == std::string::npos) {
            Logger::Log("%s SKIPPED at 0x%08X: already NOP [%s] (already patched?).", name, addr, before.c_str());
            return false;
        }

        if (Memory::PatchNOP(addr, size)) {
            Logger::Log("%s applied at 0x%08X (NOP %u bytes, was [%s]) -- capture this signature",
                        name, addr, static_cast<unsigned>(size), before.c_str());
            return true;
        }
        Logger::Log("%s FAILED (VirtualProtect) at 0x%08X (was [%s])", name, addr, before.c_str());
        return false;
    }

    bool VerifiedPatch(const char* name, uintptr_t offset,
                       const uint8_t* expected, const uint8_t* replacement, size_t size) {
        uintptr_t addr = Memory::GetGameBase() + offset;
        std::string before = Memory::BytesToHex(addr, size);

        if (Memory::VerifyBytes(addr, replacement, size)) {
            Logger::Log("%s SKIPPED at 0x%08X: already patched [%s].", name, addr, before.c_str());
            return true;
        }
        if (!Memory::VerifyBytes(addr, expected, size)) {
            Logger::Log("%s ABORTED at 0x%08X: bytes [%s] don't match signature (wrong version/base?).",
                        name, addr, before.c_str());
            return false;
        }
        if (Memory::PatchBytes(addr, replacement, size)) {
            Logger::Log("%s applied at 0x%08X (was [%s]).", name, addr, before.c_str());
            return true;
        }
        Logger::Log("%s FAILED (VirtualProtect) at 0x%08X (was [%s]).", name, addr, before.c_str());
        return false;
    }
}
