#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// Photo mode in the pause menu, without unlocking the QA debug menu with it.
//
// Photo mode is hidden because its visibility is bound to a data source that
// depends on Autolog, and EA shut Autolog down. The existing EnableExtraUIOptions
// patch brings it back by flipping one branch so that NO menu item is ever hidden,
// which also drags in the debug entries. This does the same job for photo mode
// alone.
//
// menu_pause.xml gives each item three bindings in order: IsLocked, IsEnabled,
// Visibility. Photo mode's visibility carries DataKey 0x3FF1B819, the debug menu's
// is 0xE4EE8972, so the two are distinguishable by key.
//
// sub_968EF0 is the menu-item builder, and its first block is the visibility test:
//
//     0x00968F43  mov ecx, [esi+3Ch]        the DataKey
//     0x00968F49  call edx                  resolve it
//     0x00968F4B  cmp dword [esp+38h], 2    did the resolve succeed
//     0x00968F50  jne  +29h                 not resolved -> leave the item visible
//     0x00968F52  cmp [esi+40h], bl         InvertValue
//     ...                                   -> early return, item hidden
//
// esi is the item data throughout, so the key is still in reach at 0x00968F4B.
// That instruction is five bytes, which is a jump with nothing left over.
//
// The cave compares the key and, for photo mode only, makes the following jne
// fall down the "not resolved" path, which is the branch the game already uses to
// leave an item visible. Everything else runs the original comparison and behaves
// exactly as before.
//
// ON FLAGS AND REGISTERS. The forced path sets ZF with `test %esi, %esi` rather
// than a compare against a constant: esi is the item data and cannot be null here,
// so the test always clears ZF, and unlike an arithmetic instruction it needs no
// scratch register. Nothing is clobbered. esp is untouched, so the [esp+38h] in
// the replayed instruction still refers to the same slot.
//
// This sits five bytes below the byte EnableExtraUIOptions patches, so the two do
// not overlap and can both be on. If that option is enabled it makes every item
// visible anyway and this becomes redundant rather than conflicting.

extern "C" {
    uintptr_t g_pPhotoModeReturn = 0;
    void PhotoModeHookAsm();
}

asm(
    ".text\n"
    ".globl _PhotoModeHookAsm\n"
    "_PhotoModeHookAsm:\n"
    "    cmpl $0x3FF1B819, 0x3C(%esi)\n"   // the photo mode entry?
    "    jne  1f\n"
    "    testl %esi, %esi\n"               // never zero -> ZF=0 -> item left visible
    "    jmpl *_g_pPhotoModeReturn\n"
    "1:\n"
    "    cmpl $2, 0x38(%esp)\n"            // the instruction this replaced
    "    jmpl *_g_pPhotoModeReturn\n"
);

namespace {
    const uintptr_t kSite = 0x968F4B - 0x400000;
    // cmp dword ptr [esp+38h], 2
    const uint8_t kExpect[5] = { 0x83, 0x7C, 0x24, 0x38, 0x02 };
}

namespace Features {
    void InitPhotoMode() {
        if (!g_Config.AlwaysShowPhotoMode) return;

        uintptr_t addr = Memory::GetGameBase() + kSite;
        if (!Memory::VerifyBytes(addr, kExpect, sizeof(kExpect))) {
            Logger::Log("Photo mode: ABORTED at 0x%08X, bytes [%s] don't match.",
                        addr, Memory::BytesToHex(addr, sizeof(kExpect)).c_str());
            return;
        }

        g_pPhotoModeReturn = addr + sizeof(kExpect);
        if (Memory::InjectJMP(addr, reinterpret_cast<uintptr_t>(PhotoModeHookAsm), sizeof(kExpect))) {
            Logger::Log("Photo mode: pause-menu entry forced visible (hook at 0x%08X).", addr);
        } else {
            Logger::Log("Photo mode: hook FAILED at 0x%08X.", addr);
        }
    }
}
