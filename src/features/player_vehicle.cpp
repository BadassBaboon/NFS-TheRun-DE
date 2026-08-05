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

    bool g_LoggedVehicle = false;
    bool g_LoggedReject  = false;

    // Committed pages only; guard pages and no-access reservations would fault.
    bool Readable(uintptr_t addr, size_t size) {
        if (addr < 0x10000) return false;
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) == 0) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
        uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        return addr + size <= regionEnd;
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
        if (g_Config.VehicleHealth <= 0.0f) return;

        uintptr_t vehicle = ResolveVehicle();
        if (!vehicle) return;

        float* health = reinterpret_cast<float*>(vehicle + kOffHealth);
        float current = *health;

        if (!(current > kHealthMin && current < kHealthMax)) {
            if (!g_LoggedReject) {
                Logger::Log("Vehicle health: chain resolved to 0x%08X but m_health reads %f, "
                            "which is not a health value. Not writing.", vehicle, current);
                g_LoggedReject = true;
            }
            return;
        }

        if (!g_LoggedVehicle) {
            Logger::Log("Vehicle health: NFSVehicle at 0x%08X, m_health was %.1f, holding at %.1f.",
                        vehicle, current, g_Config.VehicleHealth);
            g_LoggedVehicle = true;
        }

        if (current != g_Config.VehicleHealth) *health = g_Config.VehicleHealth;
    }
}
