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
        const uint8_t jne = 0x75;

        if (Memory::VerifyBytes(addr, &jne, 1)) {
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
            // 8B 46 40 38 18 75 29 — the JNE is at pattern+5
            const char pattern[] = "\x8B\x46\x40\x38\x18\x75\x29";
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
