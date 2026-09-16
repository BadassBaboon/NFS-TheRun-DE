#include "features.h"
#include "event_table.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>

// Game modes that replace the cars the AI races in.
//
// The game keeps an event's opponents in a flat array of 32-bit vehicle IDs, eight
// bytes apart, reached through
//
//     [[[[exe+0x23A4F60] + 0x14] + 0x68] + 0x10] + 0x4
//
// This is _mRally2's chain, given in their tables as the virtual address 027A4F60;
// the module offset is that minus the 0x400000 image base.
//
// ONE ARRAY COVERS EVERY OPPONENT. Across the Stage scripts of All American Run,
// The Classics Run and The Supercar Run, event branches write between one and nine
// slots, and the single-slot branches are the one-on-one Battle Races. Confirmed on
// real runs at all three shapes: a 1-slot Battle Race against a boss, a 3-slot pack
// and a 9-slot pack. Pack AI, small-group AI, rivals and bosses are not three
// systems, they are three slot counts in the same array.
//
// The "SWAP" scripts named after rivals in those tables are a red herring here.
// They rewrite a rival's bodykit, rims, livery and performance package, not which
// car they drive.
//
// TIERS NEED NO WORK. Every vehicle entry already carries its own performance
// package -- 340 of them across 2272 vehicles in the Vehicle Attributes Table -- so
// a tier is chosen by picking the right ENTRY. Nothing is written into any
// customization template. docs/vehicles.tsv is the full list.
//
// THE PLAYER'S OWN CAR IS NOT TOUCHED, and that is settled rather than pending. A
// cached copy of it sits near the available-car list and can be written, but the
// game restores it every tick: a test that skipped through three events kept the
// same Porsche throughout while the log reported the write landing each time. The
// real source was never found, and mRally2 did not find it either -- their mods
// change what a gas station OFFERS and let the player swap there. Forcing every
// entry of the available-car list does work, but it makes a pump show fifty-seven
// identical cars, which is worse than leaving the player's choice alone.
//
// ON SLOT COUNTS. The count cannot be guessed. In a nine-slot event slot 9 held
// aud_ur_qua_91_ai_162 -- a valid vehicle ID belonging to something else -- with an
// asset-path string only starting at slot 10, so neither "keep going while it looks
// like a vehicle" nor a fixed maximum is safe. It comes from event_table.h, keyed
// on the value the game itself put in slot 0. That key stops working the moment
// this writes to slot 0, so the lookup happens once per event and the answer is
// remembered. An event missing from the table is left completely alone.

namespace {
    const uintptr_t kChainBase = 0x23A4F60;
    const uintptr_t kChainOffsets[] = { 0x14, 0x68, 0x10 };
    const size_t    kChainLength = sizeof(kChainOffsets) / sizeof(kChainOffsets[0]);
    const uintptr_t kArrayOffset  = 0x4;
    const uintptr_t kSlotStride   = 0x8;

    // A stable per-event identifier sitting just below the array on the same
    // object. mRally2's Gas Stations script calls it vehicleAIPreset and uses it to
    // tell which chapter a gas station belongs to. Nothing overwrites it, which
    // makes it a better event identity than slot 0 -- slot 0 stops being usable the
    // moment this feature writes to it.
    const uintptr_t kPresetOffset = 0x2C;   // subtracted, not added

    const int kMaxSlotsLogged = 12;

    // Nothing may write past the largest event the game actually has.
    const int kMaxSlotsWritten = 9;

    // DOUBLE CROSS: Sergeant Cross's Corvette, che_vet_cbn_10_presale_1.
    const uint32_t kCrossCorvette = 1464376845u;

    uintptr_t g_EventArray  = 0;   // array address for the event we resolved
    uint32_t  g_EventPreset = 0;   // and its preset, together the event's identity
    uint8_t   g_EventSlots  = 0;   // slots to write, 0 = leave this event alone
    bool      g_EventKnown  = false;

    // Chain-failure reporting is LATCHED, not throttled. An unresolvable chain is
    // the normal state in menus and between events, so it must cost at most one
    // line per episode. A time-throttled version produced two hundred lines in one
    // session at roughly half the tick rate, which the code as written should not
    // have allowed -- so this does not rely on getting the arithmetic right.
    bool  g_FailLatched = false;
    DWORD g_FailTicks   = 0;

    inline bool Readable(uintptr_t addr, size_t size) {
        return addr >= 0x10000 && Memory::IsReadable(addr, size);
    }

    // Returns 0 until every link is populated, which is the normal state in menus.
    // walk records what each link held so a failure can report WHERE it broke.
    uintptr_t ResolveArray(uintptr_t* walk, int* depth, uintptr_t* owner) {
        *depth = 0; *owner = 0;
        uintptr_t p = Memory::GetGameBase() + kChainBase;
        if (!Readable(p, sizeof(uintptr_t))) return 0;
        p = *reinterpret_cast<uintptr_t*>(p);
        walk[(*depth)++] = p;

        for (size_t i = 0; i < kChainLength; ++i) {
            uintptr_t next = p + kChainOffsets[i];
            if (!Readable(next, sizeof(uintptr_t))) return 0;
            p = *reinterpret_cast<uintptr_t*>(next);
            walk[(*depth)++] = p;
        }

        uintptr_t arr = p + kArrayOffset;
        if (!Readable(arr, sizeof(uint32_t))) return 0;
        *owner = p;
        return arr;
    }

    uint32_t ReadPreset(uintptr_t owner) {
        uintptr_t a = owner - kPresetOffset;
        if (!Readable(a, sizeof(uint32_t))) return 0;
        return *reinterpret_cast<uint32_t*>(a);
    }

    // Which car the active mode puts on the grid. The INI can override it.
    uint32_t WantedVehicle() {
        if (g_Config.ForcedVehicleId != 0) return g_Config.ForcedVehicleId;
        if (g_Config.GameMode == 1) return kCrossCorvette;
        return 0;
    }

    void DumpArray(uintptr_t arr, uint32_t preset, uint8_t slots, uint32_t slot0) {
        char line[512];
        int n = 0;
        for (int i = 0; i < kMaxSlotsLogged; ++i) {
            uintptr_t slot = arr + static_cast<uintptr_t>(i) * kSlotStride;
            if (!Readable(slot, sizeof(uint32_t))) break;
            int w = _snprintf(line + n, sizeof(line) - n, " %u",
                              *reinterpret_cast<uint32_t*>(slot));
            if (w < 0 || n + w >= static_cast<int>(sizeof(line)) - 16) break;
            n += w;
        }
        line[n] = 0;
        if (slots) {
            Logger::Log("Vehicle array at 0x%08X, preset %u, was %u, %u slots:%s",
                        arr, preset, slot0, slots, line);
        } else {
            Logger::Log("Vehicle array at 0x%08X, preset %u, was %u, NOT IN THE EVENT "
                        "TABLE so nothing was written. Slots:%s", arr, preset, slot0, line);
        }
    }
}

namespace Features {
    void UpdateVehicleSwap() {
        const uint32_t want = WantedVehicle();
        const bool active = (want != 0);
        if (!active && !g_Config.LogVehicleArray) return;

        uintptr_t walk[8] = {0};
        int depth = 0;
        uintptr_t owner = 0;
        uintptr_t arr = ResolveArray(walk, &depth, &owner);

        if (!arr) {
            g_EventKnown = false;
            ++g_FailTicks;
            if (g_Config.LogVehicleArray && !g_FailLatched) {
                g_FailLatched = true;
                char line[256];
                int n = _snprintf(line, sizeof(line), "base=0x%08X",
                                  static_cast<unsigned>(Memory::GetGameBase() + kChainBase));
                static const char* kName[] = { "[base]", "+0x14", "+0x68", "+0x10" };
                for (int i = 0; i < depth && i < 4; ++i) {
                    int w = _snprintf(line + n, sizeof(line) - n, "  %s=0x%08X",
                                      kName[i], static_cast<unsigned>(walk[i]));
                    if (w < 0) break;
                    n += w;
                }
                Logger::Log("Vehicle array: no event set up -- chain stopped after "
                            "%d of %d links. %s. This is normal in menus and between "
                            "events; nothing more will be logged until it resolves.",
                            depth, static_cast<int>(kChainLength) + 1, line);
            }
            return;
        }
        if (g_FailLatched) {
            if (g_Config.LogVehicleArray) {
                Logger::Log("Vehicle array: chain resolved after %lu unresolved ticks.",
                            static_cast<unsigned long>(g_FailTicks));
            }
            g_FailLatched = false;
        }
        g_FailTicks = 0;

        const uint32_t preset = ReadPreset(owner);

        // Identify the event ONCE. Slot 0 is the table's key and this feature
        // destroys it on the first write, so the lookup has to happen while it is
        // still the game's own value and the answer has to outlive it. The preset
        // and the array address together say whether we are still in the same
        // event; neither alone is enough, since a preset repeats across a reload
        // and an address can be reused by a later allocation.
        bool isNewEvent = (!g_EventKnown || arr != g_EventArray || preset != g_EventPreset);
        uint32_t slot0 = 0;
        if (isNewEvent) {
            slot0 = *reinterpret_cast<uint32_t*>(arr);
            g_EventSlots = EventTable::SlotsFor(slot0);
            g_EventArray = arr;
            g_EventPreset = preset;
            g_EventKnown = true;
        }

        // WRITE FIRST, LOG AFTERWARDS. The window between this array appearing and
        // the game reading it to spawn cars is short, and anything slow in front of
        // the write misses it. A rival-hunting diagnostic once sat here and blocked
        // the ticker for ten seconds scanning the address space; the cars were long
        // since on track by the time the write landed, and the mode looked broken
        // while the log claimed it had done the work. Diagnostics go after.
        if (active && g_EventSlots > 0) {
            int slots = g_EventSlots;
            if (slots > kMaxSlotsWritten) slots = kMaxSlotsWritten;
            for (int i = 0; i < slots; ++i) {
                uintptr_t slot = arr + static_cast<uintptr_t>(i) * kSlotStride;
                if (!Readable(slot, sizeof(uint32_t))) break;
                uint32_t* p = reinterpret_cast<uint32_t*>(slot);
                if (*p != want) *p = want;
            }
        }

        if (isNewEvent && g_Config.LogVehicleArray) {
            DumpArray(arr, preset, g_EventSlots, slot0);
        }
    }
}
