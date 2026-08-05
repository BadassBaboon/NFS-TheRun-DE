#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// Nitrous suppression, used by Run For Your Life.
//
// The site is inside fb::NFSVehicle::collectRaceCarInputState, which IDA names,
// so there is no guesswork about what it is:
//
//     0x0069B064  mov [esi+0B5h], bl        <- the NOS input flag
//     0x0069B06A  test byte ptr [edi+1882h], 8
//     0x0069B08C  mov [esi+0B6h], al        <- the second "extra NOS" flag
//
// esi is the EA::VehiclePhysics::RaceCar::InputState the function is filling in,
// and edi is the NFSVehicle (0x1882 sits just past m_health at 0x1878). Both
// mRally2's Master Table and the All American Run table hook this same
// instruction to kill nitrous, which is a useful second opinion on what it does.
//
// The important part is that this runs every frame as input is collected, not
// once when the car spawns. Zeroing the flag here means nitrous simply never
// registers as pressed, and it can be turned on and off live — no race reload is
// needed for the difficulty to take effect or stop applying.
//
// PLAYER ONLY. This is not optional. The decompiler shows nosEnabled being set
// for AI cars as well:
//
//     v25 = a4 == 0 && playerSpawnType;   // playerSpawnType != 0 means an AI car
//     v26 = (this->dword187C & 0x20000) != 0 || v25;
//     raceInputState->nosEnabled = v26;
//
// so a cave that zeroes the flag for everyone takes nitrous away from the whole
// field, which makes a race easier rather than harder. The same mistake the
// community assist patches made. The cave therefore checks isHumanPlayer at
// [esi+0x102], stored earlier in this same function at 0x0069AAD3, and leaves AI
// cars exactly as they were.
//
// Only the first flag is touched. 0xB6 is computed from the vehicle's own state
// rather than from input, and killing the input flag is already enough: with it
// held at zero the boost is never requested in the first place.
//
// WHY A CAVE RATHER THAN A BYTE PATCH. The natural replacement,
// "mov byte ptr [esi+0B5h], 0", assembles to seven bytes and the original is six,
// so it does not fit. The cheat tables solve that with a jump to allocated memory
// and this does the same. It also has to be conditional: the mode engages and
// disengages with the difficulty, and a cave reading a flag avoids rewriting
// executable bytes every time that changes.
//
// The cave clobbers only the flags register. That is safe here because the
// instruction it returns to is a `test`, which sets its own flags before anything
// reads them.

extern "C" {
    uint8_t   g_DisableNos = 0;
    uintptr_t g_pNosReturn = 0;
    void NosHookAsm();
}

asm(
    ".text\n"
    ".globl _NosHookAsm\n"
    "_NosHookAsm:\n"
    "    cmpb $0, _g_DisableNos\n"
    "    je   1f\n"
    "    cmpb $0, 0x102(%esi)\n"      // isHumanPlayer — AI keeps its nitrous
    "    je   1f\n"
    "    movb $0, 0xB5(%esi)\n"       // nitrous suppressed, player only
    "    jmp  2f\n"
    "1:\n"
    "    movb %bl, 0xB5(%esi)\n"      // stock behaviour: pass the real input through
    "2:\n"
    "    jmpl *_g_pNosReturn\n"
);

namespace {
    const uintptr_t kSiteNos = 0x69B064 - 0x400000;
    const uint8_t   kExpectNos[6] = { 0x88, 0x9E, 0xB5, 0x00, 0x00, 0x00 };

    bool g_Installed = false;
    bool g_LoggedState = false;
    uint8_t g_LastState = 0xFF;
}

namespace Features {
    void InitNosControl() {
        // Only the difficulty mode uses this, so without it the game's code is
        // left completely alone.
        if (!g_Config.RunForYourLife) return;

        uintptr_t addr = Memory::GetGameBase() + kSiteNos;
        if (!Memory::VerifyBytes(addr, kExpectNos, sizeof(kExpectNos))) {
            Logger::Log("NOS control: ABORTED at 0x%08X, bytes [%s] don't match.",
                        addr, Memory::BytesToHex(addr, sizeof(kExpectNos)).c_str());
            return;
        }

        g_pNosReturn = addr + sizeof(kExpectNos);
        if (Memory::InjectJMP(addr, reinterpret_cast<uintptr_t>(NosHookAsm), sizeof(kExpectNos))) {
            Logger::Log("NOS control: hook installed at 0x%08X.", addr);
            g_Installed = true;
        } else {
            Logger::Log("NOS control: hook FAILED at 0x%08X.", addr);
        }
    }

    void SetNosDisabled(bool disabled) {
        if (!g_Installed) return;

        uint8_t want = disabled ? 1 : 0;
        g_DisableNos = want;

        if (want != g_LastState) {
            // The very first pass is the mode simply not being engaged yet, which
            // is not worth a line in the log.
            if (g_LoggedState || want != 0) {
                Logger::Log("NOS %s.", want ? "disabled" : "restored");
                g_LoggedState = true;
            }
            g_LastState = want;
        }
    }
}
