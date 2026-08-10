#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// Player vehicle health.
//
// fb::NFSVehicle::m_health is a float at 0x1878, read live in ReClass: it sits at
// 100 and drops as the car takes damage. Once it reaches the wreck threshold the
// run ends with the "wrecked" screen. Holding it high stops that.
//
// The vehicle is a heap object, so unlike the settings classes there is no
// getContainer call that hands it over. It is reached through the pointer chain
// _mRally2's Master Table uses for the vehicle lights, wrecked trigger and
// explosion request bytes:
//
//     [[[[[[exe+0x26858B0] + 0x88] + 0x38] + 0xD0] + 0x14C] + 0x8]
//
// Every link is validated before it is followed, and the health value itself is
// range-checked before it is written, so a chain that resolves to something else
// on a different build is ignored rather than corrupting memory.
//
// The write repeats every tick rather than firing once when driving starts. Damage
// is applied continuously, so a single write would just be eaten by the next
// collision; reapplying holds the value where it was put.

namespace {
    const uintptr_t kChainBase = 0x26858B0;
    const uintptr_t kChainOffsets[] = { 0x88, 0x38, 0xD0, 0x14C, 0x8 };
    const size_t    kChainLength = sizeof(kChainOffsets) / sizeof(kChainOffsets[0]);

    const uintptr_t kOffHealth = 0x1878;

    // The stock value is 100. Anything outside this range means the chain landed
    // somewhere that is not a vehicle, so leave it alone.
    const float kHealthMin = 0.0f;
    const float kHealthMax = 100000.0f;

    // The car this last reported on. Logging is keyed on the pointer changing
    // rather than latching after the first one, so every event confirms the cap
    // was applied. A one-shot latch meant the log proved nothing past the first
    // race, which is exactly when a stale chain would go unnoticed.
    uintptr_t g_LastVehicle = 0;
    bool g_LoggedReject  = false;

    // The chain resolves to a stale object for a few seconds while a level loads,
    // long before the player's car exists. That is expected and the range check
    // handles it, so a rejection is only worth reporting once it has persisted:
    // by then it is a real failure rather than a load in progress. The ticker runs
    // every 16ms, so this is roughly ten seconds.
    const int  kRejectTicksBeforeLogging = 600;
    int  g_RejectTicks = 0;

    inline bool Readable(uintptr_t addr, size_t size) {
        return addr >= 0x10000 && Memory::IsReadable(addr, size);
    }

    // Returns 0 if any link in the chain is not yet populated or is not readable.
    uintptr_t ResolveVehicle() {
        uintptr_t p = Memory::GetGameBase() + kChainBase;
        if (!Readable(p, sizeof(uintptr_t))) return 0;
        p = *reinterpret_cast<uintptr_t*>(p);

        for (size_t i = 0; i < kChainLength; ++i) {
            uintptr_t next = p + kChainOffsets[i];
            if (!Readable(next, sizeof(uintptr_t))) return 0;
            p = *reinterpret_cast<uintptr_t*>(next);
        }

        if (!Readable(p + kOffHealth, sizeof(float))) return 0;
        return p;
    }
}

namespace Features {
    void UpdatePlayerVehicle() {
        // Run For Your Life owns the health while it is engaged, so whatever is in
        // [VEHICLE] is deliberately ignored rather than merged.
        const bool rfyl = Difficulty::RunForYourLifeActive();
        const float target = rfyl ? Difficulty::kHealthCap : g_Config.VehicleHealth;
        if (target <= 0.0f) return;

        uintptr_t vehicle = ResolveVehicle();
        if (!vehicle) return;

        float* health = reinterpret_cast<float*>(vehicle + kOffHealth);
        float current = *health;

        // Exactly zero is a real reading, not a bad pointer: a wrecked car sits at
        // zero, and so does one between events before the next is initialised.
        // There is nothing to do with it either way, so skip quietly rather than
        // reporting a chain that is working perfectly well.
        if (current == 0.0f) {
            g_RejectTicks = 0;
            return;
        }

        // Anything else outside the plausible range means the chain has landed on
        // something that is not a vehicle. NaN fails this too, since a comparison
        // against NaN is always false.
        if (!(current > kHealthMin && current < kHealthMax)) {
            if (++g_RejectTicks >= kRejectTicksBeforeLogging && !g_LoggedReject) {
                Logger::Log("Vehicle health: the pointer chain has resolved to 0x%08X for %d "
                            "seconds but m_health reads %f, which is not a plausible health "
                            "value. Nothing is being written.",
                            vehicle, kRejectTicksBeforeLogging / 60, current);
                g_LoggedReject = true;
            }
            return;
        }

        g_RejectTicks = 0;

        if (vehicle != g_LastVehicle) {
            Logger::Log("Vehicle health: NFSVehicle at 0x%08X, m_health was %.1f, %s at %.1f.",
                        vehicle, current, rfyl ? "capped" : "held", target);
            g_LastVehicle = vehicle;
            // A good read clears the rejection latch, so a genuine failure later
            // in the session is reported instead of being swallowed by an earlier
            // transient one.
            g_LoggedReject = false;
        }

        // Two different behaviours, because the same write means opposite things
        // depending on which side of stock the target sits.
        //
        // [VEHICLE] VehicleHealth is a cheat: you set it above 100 and it is held
        // there, so damage never accumulates and the wreck screen never fires.
        //
        // Run For Your Life sets a value below stock, and holding it there would
        // make the car invulnerable at 50 rather than fragile — exactly backwards.
        // So the mode applies a ceiling instead: health is pulled down to the cap
        // but never pushed back up, which halves the damage you can absorb while
        // leaving damage itself working normally. It also re-applies by itself
        // after a respawn, when the game resets health to 100.
        if (rfyl) {
            if (current > target) *health = target;
        } else if (current != target) {
            *health = target;
        }
    }
}
