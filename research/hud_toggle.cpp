#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// A key that hides the HUD, for clean screenshots.
//
// Photo mode draws a row of control hints along the bottom of the screen, and any
// external capture (Print Screen, GeForce Experience, Steam) grabs them along with
// the shot. The widget itself offers no way out: _c4/UI/Assets/Widget_photo_mode
// is a bare UIWidgetAsset with no WidgetEvents, no WidgetFunctions and no
// visibility binding, exactly like the nitrous HUD widget, so there is no
// data-driven flag to turn it off.
//
// What does exist is the game's own HUD visibility byte, which mRally2's Master
// Table exposes as "In-Game HUD" with 1/0 hotkeys:
//
//     [[exe+0x248B55C] + 0xC] + 0x5B8
//
// This binds a key to flip it. Whether it also clears the photo mode hint row is
// the open question: it is the racing HUD's flag, and the hint row may be drawn by
// a different screen. The log reports the byte it found and every flip, so one
// press in photo mode settles it either way.
//
// The chain is re-resolved on every press rather than cached. The UI objects
// behind it are rebuilt between screens, and a pointer captured on one screen is
// not valid on the next; that is the same failure that produced garbage reads from
// the cached vehicle-control pointer.

namespace {
    const uintptr_t kHudBase   = 0x248B55C;
    const uintptr_t kHudOffset1 = 0x0C;    // applied first
    const uintptr_t kHudOffset2 = 0x5B8;   // then this, to reach the byte

    bool g_KeyWasDown = false;
    bool g_LoggedResolve = false;
    bool g_LoggedFailure = false;

    // Returns the address of the HUD visibility byte, or 0 if the chain is not
    // currently valid.
    uintptr_t ResolveHudByte() {
        uintptr_t slot = Memory::GetGameBase() + kHudBase;
        if (!Memory::IsReadable(slot, sizeof(uintptr_t))) return 0;

        uintptr_t p = *reinterpret_cast<uintptr_t*>(slot);
        if (p < 0x10000) return 0;
        if (!Memory::IsReadable(p + kHudOffset1, sizeof(uintptr_t))) return 0;

        p = *reinterpret_cast<uintptr_t*>(p + kHudOffset1);
        if (p < 0x10000) return 0;
        if (!Memory::IsReadable(p + kHudOffset2, sizeof(uint8_t))) return 0;

        return p + kHudOffset2;
    }
}

namespace Features {
    void UpdateHudToggle() {
        if (g_Config.HudToggleKey <= 0) return;

        // Edge-triggered, so holding the key does not flip it every tick.
        bool down = (GetAsyncKeyState(g_Config.HudToggleKey) & 0x8000) != 0;
        bool pressed = down && !g_KeyWasDown;
        g_KeyWasDown = down;
        if (!pressed) return;

        uintptr_t addr = ResolveHudByte();
        if (!addr) {
            if (!g_LoggedFailure) {
                Logger::Log("HUD toggle: the pointer chain from [exe+0x%X] is not valid right now, "
                            "so there is nothing to flip. This is expected outside a race.",
                            static_cast<unsigned>(kHudBase));
                g_LoggedFailure = true;
            }
            return;
        }
        g_LoggedFailure = false;

        uint8_t* hud = reinterpret_cast<uint8_t*>(addr);
        if (!g_LoggedResolve) {
            Logger::Log("HUD toggle: visibility byte at 0x%08X, currently %u.", addr, *hud);
            g_LoggedResolve = true;
        }

        *hud = (*hud != 0) ? 0 : 1;
        Logger::Log("HUD toggle: %s.", *hud ? "shown" : "hidden");
    }
}
