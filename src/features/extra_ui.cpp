#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"

namespace Features {
    void InitExtraUIOptions() {
        if (!g_Config.EnableExtraUIOptions) {
            Logger::Log("EnableExtraUIOptions disabled in INI.");
            return;
        }

        // UILabelTreeMenuItem::Visibility::InvertValue (NFSTR 1.1.0.0)
        // Module offset 0x568F50 (absolute 0x00968F50 at the default 0x00400000 base).
        // JNE -> JMP: makes hidden UI label entries (QA debug, photo mode) always visible
        uintptr_t addr = Memory::GetGameBase() + 0x568F50;
        const uint8_t expected[] = { 0x83, 0x7C, 0x24, 0x38, 0x02, 0x75, 0x29 };

        if (Memory::VerifyBytes(addr - 5, expected, sizeof(expected))) {
            // Fast path: exact address match
            uint8_t jmp = 0xEB;
            if (Memory::PatchBytes(addr, &jmp, 1)) {
                Logger::Log("EnableExtraUIOptions: JNE->JMP at 0x%08X", addr);
            } else {
                Logger::Log("EnableExtraUIOptions: patch failed at 0x%08X", addr);
            }
        } else {
            // Fallback: AOB scan in case of a minor exe version difference
            HMODULE mainMod = GetModuleHandle(NULL);
            // Match the same comparison and branch as the verified fixed site.
            const char pattern[] = "\x83\x7C\x24\x38\x02\x75\x29";
            const char mask[]    = "xxxxxxx";
            uintptr_t foundAddr = Memory::FindPatternModule(mainMod, pattern, mask);
            if (foundAddr != 0) {
                uintptr_t jneAddr = foundAddr + 5;
                uint8_t jmp = 0xEB;
                if (Memory::PatchBytes(jneAddr, &jmp, 1)) {
                    Logger::Log("EnableExtraUIOptions: AOB fallback JNE->JMP at 0x%08X (expected 0x%08X)", jneAddr, addr);
                } else {
                    Logger::Log("EnableExtraUIOptions: AOB fallback patch failed at 0x%08X", jneAddr);
                }
            } else {
                Logger::Log("EnableExtraUIOptions: byte at 0x%08X is [%s] (not 0x75), AOB fallback also failed.", addr, Memory::BytesToHex(addr, 1).c_str());
            }
        }
    }
}
