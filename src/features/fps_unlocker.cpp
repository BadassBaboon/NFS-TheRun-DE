#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>

extern "C" {
    uint32_t* g_pSimTickEnable = nullptr;
    uintptr_t g_pGameTimeReturn = 0;

    // Captured pointer to the PlayerHasVehicleControl byte (game sets/clears it).
    uint8_t* g_pHasControl = nullptr;
    uintptr_t g_pControlReturn = 0;       // return addr for the direct (unhooked-site) path
    uintptr_t g_pControlChainTarget = 0;  // existing hook's stub, for the coexist path

    void GameTimeHookAsm();
    void ControlCheckHookAsm();
    void ControlChainHookAsm();
}

asm(
    ".text\n"
    ".globl _GameTimeHookAsm\n"
    "_GameTimeHookAsm:\n"
    "    pushl %edi\n"
    "    leal 0x40(%eax), %edi\n"
    "    movl %edi, _g_pSimTickEnable\n"
    "    popl %edi\n"
    "    movb 0x40(%eax), %cl\n"
    "    movl 0x08(%ebx), %eax\n"
    "    jmpl *_g_pGameTimeReturn\n"
);

// Hook at exe+3F6C73. Original stolen bytes: cmp byte ptr [esi+04],00 ; push edi.
// We capture &[esi+04] (the control flag), then re-run the stolen instructions.
asm(
    ".text\n"
    ".globl _ControlCheckHookAsm\n"
    "_ControlCheckHookAsm:\n"
    "    pushl %ebx\n"
    "    leal 0x04(%esi), %ebx\n"
    "    movl %ebx, _g_pHasControl\n"
    "    popl %ebx\n"
    "    cmpb $0x00, 0x04(%esi)\n"   // stolen: cmp byte ptr [esi+04],00
    "    pushl %edi\n"               // stolen: push edi
    "    jmpl *_g_pControlReturn\n"
);

// Coexist path: the site is already hooked by another mod (e.g. FusionFix places
// an E9 -> its SafetyHook stub). We capture &[esi+04], preserve ALL register/flag
// state, then chain into that existing stub so both hooks run. The stub replays
// the original instructions and returns to the game itself.
asm(
    ".text\n"
    ".globl _ControlChainHookAsm\n"
    "_ControlChainHookAsm:\n"
    "    pushl %eax\n"
    "    leal 0x04(%esi), %eax\n"
    "    movl %eax, _g_pHasControl\n"
    "    popl %eax\n"
    "    jmpl *_g_pControlChainTarget\n"
);

static bool g_LogCapturedHook = false;
static bool g_LogCapturedControl = false;
static int  g_LastControlState = -1;   // -1 unknown, 0 no control, 1 has control
static bool g_WarnedStaleControl = false;
static float g_LastLoggedFps = 0.0f;
static uint32_t g_LastLoggedTickEnable = 999;

// Control-hook install is deferred: FusionFix hooks the same site at its own
// startup, so we wait for it to settle and then chain onto it (the safe path).
// Hooking the clean site first and letting FusionFix wrap our raw jmp is what
// intermittently broke boot. If the site is still clean after the delay, no
// other mod is present and hooking it ourselves is safe.
static bool  g_FrameUnlockerActive = false;
static bool  g_ControlHookAttempted = false;
static DWORD g_FpsInitTime = 0;
static const DWORD kControlHookDelayMs = 5000;

// Sim rate the game's QTE/particle/audio timers were hardcoded around.
static const float kBaseSimRate = 30.0f;

// Validated view of the control flag for other features to gate on: -1 unknown,
// 0 no control, 1 driving. Deliberately not the raw pointer — see the stale-read
// handling in UpdateFramerateUnlocker.
extern "C" int PlayerControlState() { return g_LastControlState; }

namespace Features {
    void InitFramerateUnlocker() {
        if (!g_Config.EnableFramerateUnlocker) {
            Logger::Log("EnableFramerateUnlocker=0: skipping GameTime and control hooks.");
            return;
        }

        // Module offset 0xA607F7 (absolute 0x00E607F7 at the default 0x00400000 base).
        // Resolve relative to the actual module base so relocation/ASLR can't misdirect us.
        uintptr_t hookAddr = Memory::GetGameBase() + 0xA607F7;
        g_pGameTimeReturn = hookAddr + 6;

        // Expected stolen bytes: mov cl,[eax+0x40]; mov eax,[ebx+0x08]
        const uint8_t expected[6] = { 0x8A, 0x48, 0x40, 0x8B, 0x43, 0x08 };
        if (!Memory::VerifyBytes(hookAddr, expected, sizeof(expected))) {
            Logger::Log("GameTime hook ABORTED at 0x%08X: unexpected bytes [%s] (expected 8A 48 40 8B 43 08). "
                        "Wrong game version or bad base?", hookAddr, Memory::BytesToHex(hookAddr, 6).c_str());
            return;
        }

        if (Memory::InjectJMP(hookAddr, reinterpret_cast<uintptr_t>(GameTimeHookAsm), 6)) {
            Logger::Log("GameTime hook injected successfully at 0x%08X", hookAddr);
        } else {
            Logger::Log("GameTime hook failed at 0x%08X", hookAddr);
        }


        // The control-check hook is installed later (see InstallControlHook), after a
        // delay, so a co-loaded mod like FusionFix hooks the shared site first and we
        // chain onto it instead of racing it.
        g_FpsInitTime = GetTickCount();
        g_FrameUnlockerActive = true;
    }

    // Hook the vehicle-control check so we can drop the sim rate back to 30 during
    // no-control moments (QTEs, generic cutscenes), where the game assumes a
    // hardcoded 30fps delta. Automates mRally2's manual sim-rate trick.
    static void InstallControlHook() {
        uintptr_t ctlAddr = Memory::GetGameBase() + 0x3F6C73;

        // Expected stolen bytes: cmp byte ptr [esi+04],00 ; push edi
        const uint8_t ctlExpected[5] = { 0x80, 0x7E, 0x04, 0x00, 0x57 };
        std::string ctlBytes = Memory::BytesToHex(ctlAddr, 5);
        bool alreadyHooked = (ctlBytes.compare(0, 2, "E9") == 0);

        if (alreadyHooked) {
            // Preferred path: another mod (FusionFix) hooked first. Chain onto its
            // stub so both run and its trampoline is left intact.
            int32_t rel = *reinterpret_cast<int32_t*>(ctlAddr + 1);
            g_pControlChainTarget = ctlAddr + 5 + rel;
            if (Memory::InjectJMP(ctlAddr, reinterpret_cast<uintptr_t>(ControlChainHookAsm), 5)) {
                Logger::Log("Control-check hook CHAINED at 0x%08X onto existing stub 0x%08X "
                            "(coexisting with another mod, sim-rate clamp enabled).", ctlAddr, g_pControlChainTarget);
            } else {
                Logger::Log("Control-check chain hook failed at 0x%08X", ctlAddr);
            }
        } else if (Memory::VerifyBytes(ctlAddr, ctlExpected, sizeof(ctlExpected))) {
            // Still clean after the delay: no other mod present, safe to hook directly.
            g_pControlReturn = ctlAddr + 5;
            if (Memory::InjectJMP(ctlAddr, reinterpret_cast<uintptr_t>(ControlCheckHookAsm), 5)) {
                Logger::Log("Control-check hook injected at 0x%08X (clean site, sim-rate clamp enabled).", ctlAddr);
            } else {
                Logger::Log("Control-check hook failed at 0x%08X", ctlAddr);
            }
        } else {
            Logger::Log("Control-check hook ABORTED at 0x%08X: unexpected bytes [%s] (expected 80 7E 04 00 57 or E9 jmp).",
                        ctlAddr, ctlBytes.c_str());
        }
    }

    void UpdateFramerateUnlocker() {
        // Deferred control-hook install: wait past startup so FusionFix hooks first.
        // The sim-rate clamp and the driving-only FOV override both gate on the
        // vehicle-control flag this hook captures.
        bool needControlHook = g_Config.ClampSimRateWhenNoControl
                            || (g_Config.EnableRenderTweaks && g_Config.ForceFovOnlyWhileDriving
                                && g_Config.ForceFov > 0.0f);
        if (g_FrameUnlockerActive && needControlHook && !g_ControlHookAttempted
            && (GetTickCount() - g_FpsInitTime) >= kControlHookDelayMs) {
            InstallControlHook();
            g_ControlHookAttempted = true;
        }

        if (!g_pSimTickEnable) return;

        if (!g_LogCapturedHook) {
            Logger::Log("GameTime hook active: g_pSimTickEnable pointer captured at 0x%08X", reinterpret_cast<uintptr_t>(g_pSimTickEnable));
            g_LogCapturedHook = true;
        }

        float* pMaxVariableFps = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(g_pSimTickEnable) - 0x18);

        // Apply target FPS Limit
        float targetFps = (g_Config.FPSLimit == -1) ? 1000.0f : static_cast<float>(g_Config.FPSLimit);

        // Sim-rate clamp: when the player has no vehicle control (QTE / cutscene),
        // pull the sim rate back to 30 so hardcoded-30fps timers behave correctly.
        // The control state is read whether or not the clamp is enabled, because
        // the render settings gate the FOV override on it too.
        if (g_pHasControl) {
            uint8_t ctlByte = *g_pHasControl;

            if (!g_LogCapturedControl) {
                Logger::Log("Control-check hook active: PlayerHasVehicleControl byte at 0x%08X (initial value %u)",
                            reinterpret_cast<uintptr_t>(g_pHasControl), ctlByte);
                g_LogCapturedControl = true;
            }

            // The byte is a bool, so anything other than 0 or 1 means the pointer
            // has gone stale: it aims into an object that has since been freed and
            // its memory handed to something else. Treating a stale read as truth
            // is worse than ignoring it — a garbage nonzero byte reads as "driving"
            // and would release the sim-rate clamp in the middle of a cutscene,
            // which is exactly what the clamp exists to prevent. The last known
            // good state is held until the hook fires again and re-captures.
            if (ctlByte > 1) {
                if (!g_WarnedStaleControl) {
                    Logger::Log("Vehicle control: byte at 0x%08X read %u, which is not a bool. "
                                "The object it points at was freed and reused. Holding the last "
                                "known state until the hook re-captures.",
                                reinterpret_cast<uintptr_t>(g_pHasControl), ctlByte);
                    g_WarnedStaleControl = true;
                }
            } else {
                int hasControl = (ctlByte != 0) ? 1 : 0;
                if (hasControl != g_LastControlState) {
                    Logger::Log("Vehicle control changed: PlayerHasVehicleControl=%u -> %s sim rate",
                                ctlByte, hasControl ? "restoring target" : "clamping to 30");
                    g_LastControlState = hasControl;
                }
                g_WarnedStaleControl = false;
            }
        }

        // Unknown state (-1) leaves the target framerate alone rather than guessing.
        if (g_Config.ClampSimRateWhenNoControl && g_LastControlState == 0) {
            targetFps = kBaseSimRate;
        }

        if (targetFps > 0.0f && *pMaxVariableFps != targetFps) {
            *pMaxVariableFps = targetFps;
        }

        // Cutscene unlock: force the variable sim tick on so cutscenes and menus
        // are not held to 30.
        if (g_Config.UnlockCutsceneFPS) {
            if (*g_pSimTickEnable != 1) {
                *g_pSimTickEnable = 1;
            }
        }


        // Log changes to FPS or tick state
        if (*pMaxVariableFps != g_LastLoggedFps || *g_pSimTickEnable != g_LastLoggedTickEnable) {
            g_LastLoggedFps = *pMaxVariableFps;
            g_LastLoggedTickEnable = *g_pSimTickEnable;
            Logger::Log("GameTime state updated: MaxVariableFps = %.1f, VariableSimTickEnable = %u", g_LastLoggedFps, g_LastLoggedTickEnable);
        }
    }
}
