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
static bool g_WarnedCutsceneConflict = false;

// True while the control byte is unreadable garbage. The clamp can safely hold
// its last known state through this; the cutscene unlock CANNOT, and must fail
// closed instead. See the note at the tick write.
static bool g_ControlReadStale = false;

// When the no-control window began, for the cutscene-unlock dwell below.
static DWORD g_NoControlSince = 0;

// How long no-control has to persist before the variable sim tick is allowed.
//
// "No control" is NOT the same as "no car is being simulated", which is what the
// unlock actually requires. A head-to-head wreck takes control away and then
// simulates your car tumbling through the air — that is physics, and running it
// on a variable step at 144 flattens it into a gentle slide instead of the
// dramatic crash the game authored at 30. It was reported exactly that way.
//
// Crash and takedown windows are short and often flicker (a real log showed
// control going 1 -> 0 -> 1 inside two seconds). Cutscenes, menus, the garage and
// the car select last far longer. So the tick waits for the window to prove
// itself before switching, and a crash never survives the wait.
//
// This is a heuristic and worth naming as one. The principled version keys on
// whether the player's vehicle is actually being simulated rather than on how
// long control has been gone; that needs a signal we do not have yet. The cost of
// this one is that a genuine cutscene spends its first second and a half at 30
// before unlocking.
static const DWORD kCutsceneDwellMs = 1500;
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

        // Say what this flag is actually being used for. Three different settings
        // want it and they are not interchangeable, so a message that always
        // claims "sim-rate clamp" is wrong two thirds of the time — and wrong
        // exactly when someone is reading the log to find out why the clamp is
        // not behaving.
        const char* purpose = g_Config.ClampSimRateWhenNoControl
                            ? (g_Config.UnlockCutsceneFPS ? "sim-rate clamp + cutscene unlock"
                                                          : "sim-rate clamp")
                            : (g_Config.UnlockCutsceneFPS ? "cutscene unlock"
                                                          : "driving-only FOV");

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
                            "(coexisting with another mod, for: %s).",
                            ctlAddr, g_pControlChainTarget, purpose);
            } else {
                Logger::Log("Control-check chain hook failed at 0x%08X", ctlAddr);
            }
        } else if (Memory::VerifyBytes(ctlAddr, ctlExpected, sizeof(ctlExpected))) {
            // Still clean after the delay: no other mod present, safe to hook directly.
            g_pControlReturn = ctlAddr + 5;
            if (Memory::InjectJMP(ctlAddr, reinterpret_cast<uintptr_t>(ControlCheckHookAsm), 5)) {
                Logger::Log("Control-check hook injected at 0x%08X (clean site, for: %s).",
                            ctlAddr, purpose);
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
                            || g_Config.UnlockCutsceneFPS
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
                g_ControlReadStale = true;
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
                    // Report what will actually happen, not what the clamp would
                    // do if it were on. These three settings produce three
                    // different responses to the same transition.
                    const char* effect;
                    if (hasControl) {
                        effect = "driving, fixed sim step at the target framerate";
                    } else if (g_Config.UnlockCutsceneFPS) {
                        effect = "no control, unlocking (variable sim tick on)";
                    } else if (g_Config.ClampSimRateWhenNoControl) {
                        effect = "no control, clamping to 30";
                    } else {
                        effect = "no control, sim rate left alone";
                    }
                    Logger::Log("Vehicle control changed: PlayerHasVehicleControl=%u -> %s.",
                                ctlByte, effect);
                    g_LastControlState = hasControl;
                    // Restart the dwell on every entry into no-control, so a crash
                    // that briefly drops control cannot inherit a cutscene's credit.
                    if (!hasControl) g_NoControlSince = GetTickCount();
                }
                // Coming back from a stale run restarts the dwell. The state may
                // have read "no control" the whole time it was garbage, and that
                // stretch should not count as a cutscene that has proven itself.
                if (g_ControlReadStale) {
                    g_NoControlSince = GetTickCount();
                    g_ControlReadStale = false;
                }
                g_WarnedStaleControl = false;
            }
        }

        // Unknown state (-1) counts as "driving" everywhere below, which is the
        // safe reading: it keeps the fixed sim step rather than guessing that
        // nothing is being simulated.
        const bool noControl = (g_LastControlState == 0);

        // Cutscene unlock, scoped to the no-control window.
        //
        // The variable sim tick is only destructive when there is a car under the
        // player's control to corrupt. During a cutscene, the car select or the
        // garage there is not one, so the engine note, the tyre spray and the
        // handling cannot be affected by a variable step — there is no driving
        // happening to affect. Confining it to those moments is what makes this
        // safe, and it is why the option is gated on the control flag rather than
        // set once and left on. Every frame of actual driving keeps the fixed 30 Hz
        // step the game was tuned against.
        //
        // It REPLACES the clamp during that window rather than running alongside
        // it. Both target exactly the same moments and want opposite things — the
        // clamp pulls the rate to 30, this leaves it at the target — so with both
        // applied the clamp would simply win and cutscenes would stay at 30.
        //
        // THE COST is the QTE fix, which is the clamp. Prompts count down against
        // a 30 FPS frame time, so at the target framerate they expire faster and
        // the timing is tighter. Play-testing found them still playable, just less
        // forgiving. That is a real trade rather than a free win, which is why this
        // ships off.
        // FAIL CLOSED ON A STALE READ. The clamp holds its last known state when
        // the control byte turns to garbage, because clamping to 30 during
        // gameplay is merely slow. The tick is the opposite: holding "no control"
        // while the player is actually driving leaves the variable step ON through
        // live gameplay, which is the one outcome this whole design exists to
        // prevent. So an untrustworthy read forces the fixed step, and the unlock
        // resumes only once the byte reads as a bool again.
        const bool dwellMet = noControl
                           && !g_ControlReadStale
                           && (GetTickCount() - g_NoControlSince) >= kCutsceneDwellMs;
        const bool cutsceneUnlock = g_Config.UnlockCutsceneFPS && dwellMet;

        if (g_Config.ClampSimRateWhenNoControl && noControl && !cutsceneUnlock) {
            targetFps = kBaseSimRate;
        }

        // The unlock clamps the SHORT no-control windows itself, whatever the
        // clamp setting says.
        //
        // Turning the tick off for crashes was not enough: wrecks still came out
        // limp. The tick was never the whole story — crash drama also needs the
        // rate at 30, and using the unlock means running with the clamp off, which
        // left crashes at the target framerate. So the two halves are handled
        // together. A window that has not yet earned the unlock gets the full
        // fixed-30 treatment, exactly as if the clamp were on, and only a window
        // that outlasts the dwell is released to the target rate.
        //
        // This makes the unlock self-contained: driving fast and fixed, crashes
        // and takedowns at 30, cutscenes fast and smooth.
        if (g_Config.UnlockCutsceneFPS && noControl && !cutsceneUnlock) {
            targetFps = kBaseSimRate;
        }

        if (targetFps > 0.0f && *pMaxVariableFps != targetFps) {
            *pMaxVariableFps = targetFps;
        }

        // The field is only ever written when the option is on, and it is driven
        // back to 0 the moment control returns rather than left set. An earlier
        // version set it once and never cleared it, which is why enabling this
        // used to break driving: the whole game ran on a variable step from the
        // first cutscene onwards. Writing it unconditionally is also avoided, so
        // with the option off the game owns the field exactly as it always did.
        if (g_Config.UnlockCutsceneFPS) {
            const uint32_t want = cutsceneUnlock ? 1u : 0u;
            if (*g_pSimTickEnable != want) {
                *g_pSimTickEnable = want;
            }
            if (!g_WarnedCutsceneConflict) {
                Logger::Log("UnlockCutsceneFPS is ON: the variable sim tick is enabled only after "
                            "%lu ms without vehicle control, so driving and crash physics keep "
                            "their fixed 30 Hz step. QTE prompts run at the target framerate and "
                            "time out faster.", static_cast<unsigned long>(kCutsceneDwellMs));
                g_WarnedCutsceneConflict = true;
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
