#include "features.h"
#include "event_table.h"
#include "run_modes.h"
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
// THE PLAYER'S CURRENT CAR IS NOT TOUCHED. A cached copy of it sits near the
// available-car list and can be written, but the game restores it every tick: a
// test that skipped through three events kept the same Porsche throughout while
// the log reported the write landing each time. The real source was never found,
// and mRally2 did not find it either -- their mods change what a gas station
// OFFERS and let the player swap there, which is what this does too.
//
// WHAT A PUMP OFFERS IS A RESIZABLE LIST, and both its contents and its LENGTH
// can be set. The list is the player's available-car catalogue, and the three
// words in front of it are a begin/end/capacity triple:
//
//     gas - 0x10   begin      == the array address itself
//     gas - 0x0C   end        count = (end - begin) / 4
//     gas - 0x08   capacity   256 entries
//
// Confirmed against two captured dumps without further testing: in one, begin ==
// end and the diagnostic had reported 0 entries; in the other the arithmetic gave
// exactly the 57 entries it had counted.
//
// This is why an earlier attempt produced a pump showing an endless row of
// identical Golfs. It rewrote the contents of a 57-entry list without touching
// its length. Writing a short list AND its end pointer gives a pump that offers
// exactly those cars, once each.
//
// THE STORY'S SCRIPTED CAR SELECTS ARE NOT COVERED, and cannot be. The opening
// garage, the Vegas and Audi dealerships and Uri's garage use a separate per-event
// list that only accepts cars the event already loads: a foreign ID keeps the old
// badge, freezes the camera and hands over the original car. docs/RESEARCH.md
// section 48 has the full investigation -- read it before trying again.
//
// THE THREE RUN MODES fill an event from a curated pool instead of forcing one
// car, which is what All American Run, The Classics Run and The Supercar Run do.
// The pools and the per-event stage mapping are in run_modes.h; the only logic
// here is drawing from them. Picks are WITHOUT REPLACEMENT, matching the source
// tables -- a nine-car grid of nine different cars, not nine of one.
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

    // The player's available-car list -- what a gas station offers.
    //
    //     [[[[[exe+0x2482500]+0x64]+0x1A8]+0x18]+0x1D8]+0
    //
    // Four-byte stride, unlike the opponent array's eight, and preceded by a
    // begin/end/capacity triple. Only `end` is written, and only to SHRINK: moving
    // it past `capacity` would hand the game a list longer than its own buffer.
    const uintptr_t kGasBase = 0x2482500;
    const uintptr_t kGasOffsets[] = { 0x64, 0x1A8, 0x18, 0x1D8 };
    const size_t    kGasLength = sizeof(kGasOffsets) / sizeof(kGasOffsets[0]);
    const uintptr_t kGasStride = 0x4;

    const uintptr_t kGasBeginOffset = 0x10;   // all subtracted from the array
    const uintptr_t kGasEndOffset   = 0x0C;
    const uintptr_t kGasCapOffset   = 0x08;

    // What a pump offers in DOUBLE CROSS, once each. Named in the INI so players
    // know what the mode hands them.
    const uint32_t kDoubleCrossGasCars[] = {
         487380342u,   // bmw_m3_gts_10_presale_1    Most Wanted Edition M3 GTS
        1147473296u,   // aud_r8_v10_10_presale_1    Darius's Audi R8
         242635328u,   // dod_chr_taxi_player        Charger Taxi
        3675475195u,   // for_vic_taxi_player        Crown Vic Taxi
        3753218205u,   // cop_car_int_11_oos         Decommissioned Cruiser
        3720220186u,   // vol_gti_mk1_76_pp_rare_1   Kuru Tactics Golf
    };
    const int kDoubleCrossGasCount =
        sizeof(kDoubleCrossGasCars) / sizeof(kDoubleCrossGasCars[0]);

    // Only Double Cross sets a gas station list. The difficulty never does and
    // the three Run modes leave the pumps alone, so Double Cross on DEADLY gets
    // these cars plus DEADLY's rules with nothing to arbitrate. A forced vehicle
    // override turns the mode's car behaviour off, this included.
    const uint32_t* PlayerCarList(int* count) {
        if (g_Config.GameMode == 1 && g_Config.ForcedVehicleId == 0) {
            *count = kDoubleCrossGasCount;
            return kDoubleCrossGasCars;
        }
        *count = 0;
        return 0;
    }

    uintptr_t g_GasArray = 0;
    int       g_GasLogged = -1;
    int       g_GasLoggedBefore = -1;   // the count the game had, so a same-size
                                        // write in a new context still reports

    // How many cars the game had in the list before anything was written, for the
    // log. Deliberately NOT used to decide anything: the main menu's car browser
    // reads the same list, and its size changes with progress and unlock cheats.
    int g_LastSeenCount = 0;
    int g_ReportedCount = -1;

    // xorshift32. The grid should differ between runs of the same event, which is
    // the whole appeal of the Run mods, and nothing here needs a good distribution
    // beyond that. Seeded once from the tick count.
    uint32_t g_Rng = 0;
    uint32_t NextRandom() {
        if (g_Rng == 0) g_Rng = GetTickCount() | 1u;
        g_Rng ^= g_Rng << 13;
        g_Rng ^= g_Rng >> 17;
        g_Rng ^= g_Rng << 5;
        return g_Rng;
    }

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

    // The grid drawn for the current event, held so every tick writes the same
    // cars rather than reshuffling.
    uint32_t g_Draw[kMaxSlotsWritten] = {0};
    bool     g_DrawValid = false;

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

    uintptr_t ResolveGasArray() {
        uintptr_t p = Memory::GetGameBase() + kGasBase;
        if (!Readable(p, sizeof(uintptr_t))) return 0;
        p = *reinterpret_cast<uintptr_t*>(p);
        for (size_t i = 0; i < kGasLength; ++i) {
            uintptr_t next = p + kGasOffsets[i];
            if (!Readable(next, sizeof(uintptr_t))) return 0;
            p = *reinterpret_cast<uintptr_t*>(next);
        }
        if (!Readable(p, sizeof(uint32_t))) return 0;
        return p;
    }

    // Replaces the offered cars AND the list's length, so a pump shows exactly
    // these and nothing else. Returns the count written, or -1 if it did nothing.
    //
    // Rewriting contents without the length is what produced a pump offering an
    // endless row of identical Golfs: fifty-seven slots, all set to one car.
    int WriteGasList(uintptr_t gas, const uint32_t* cars, int count) {
        const uintptr_t beginAt = gas - kGasBeginOffset;
        const uintptr_t endAt   = gas - kGasEndOffset;
        const uintptr_t capAt   = gas - kGasCapOffset;
        if (!Readable(beginAt, sizeof(uint32_t)) || !Readable(endAt, sizeof(uint32_t))
            || !Readable(capAt, sizeof(uint32_t))) return -1;

        const uintptr_t begin = *reinterpret_cast<uintptr_t*>(beginAt);
        const uintptr_t cap   = *reinterpret_cast<uintptr_t*>(capAt);

        // begin must actually point at the array we resolved, or this is not the
        // triple it looks like and nothing should be written.
        if (begin != gas) return -1;

        // Only act on a list the game has actually populated. This is what scopes
        // the feature: the catalogue is EMPTY except while a selection screen is
        // up -- a captured log read 0 entries at event start and 57 at the pump --
        // so "non-empty" covers gas stations and the story's forced car-select
        // screens alike, and touches nothing the rest of the time.
        const uintptr_t curEnd = *reinterpret_cast<uintptr_t*>(endAt);
        if (curEnd <= begin) return -2;
        g_LastSeenCount = static_cast<int>((curEnd - begin) / kGasStride);
        // Never grow past the game's own buffer.
        if (begin + static_cast<uintptr_t>(count) * kGasStride > cap) return -1;

        for (int i = 0; i < count; ++i) {
            uintptr_t slot = gas + static_cast<uintptr_t>(i) * kGasStride;
            if (!Readable(slot, sizeof(uint32_t))) return -1;
            uint32_t* p = reinterpret_cast<uint32_t*>(slot);
            if (*p != cars[i]) *p = cars[i];
        }

        const uintptr_t wantEnd = begin + static_cast<uintptr_t>(count) * kGasStride;
        uintptr_t* endp = reinterpret_cast<uintptr_t*>(endAt);
        if (*endp != wantEnd) *endp = wantEnd;
        return count;
    }

    uint32_t ReadPreset(uintptr_t owner) {
        uintptr_t a = owner - kPresetOffset;
        if (!Readable(a, sizeof(uint32_t))) return 0;
        return *reinterpret_cast<uint32_t*>(a);
    }

    // Which single car the active mode puts in every slot, or 0 if this mode draws
    // from a pool instead.
    uint32_t WantedVehicle() {
        if (g_Config.ForcedVehicleId != 0) return g_Config.ForcedVehicleId;
        if (g_Config.GameMode == 1) return kCrossCorvette;
        return 0;
    }

    // Fills out[0..count) with distinct entries drawn from a pool.
    //
    // Rejection sampling against what has already been chosen. With counts of at
    // most nine against pools of fifty to four hundred, collisions are rare and the
    // retry cap keeps a pathologically small pool from spinning -- if it gives up,
    // the slot simply repeats a car, which is a duller grid rather than a hang.
    void DrawDistinct(const uint32_t* pool, uint16_t poolSize, uint32_t* out, int count) {
        for (int i = 0; i < count; ++i) {
            uint32_t pick = 0;
            for (int attempt = 0; attempt < 32; ++attempt) {
                pick = pool[NextRandom() % poolSize];
                bool clash = false;
                for (int j = 0; j < i; ++j) {
                    if (out[j] == pick) { clash = true; break; }
                }
                if (!clash) break;
            }
            out[i] = pick;
        }
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
        const RunModes::Mode* mode =
            (g_Config.ForcedVehicleId != 0) ? 0 : RunModes::ForGameMode(g_Config.GameMode);
        const bool active = (want != 0);
        if (!active && !mode && !g_Config.LogVehicleArray) return;

        uintptr_t walk[8] = {0};
        int depth = 0;
        uintptr_t owner = 0;
        uintptr_t arr = ResolveArray(walk, &depth, &owner);

        // WHAT THE PLAYER IS OFFERED: gas stations, and the story's forced car
        // changes -- the opening garage, the Vegas dealership, the Audi dealership
        // after the mob, Uri's garage.
        //
        // Gated on an EVENT being set up, not on the list being populated. The main
        // menu's browse-all-cars screen reads the same list at the same address, so
        // the list itself cannot say which screen is up; what separates them is that
        // the browse screen runs with no event while the forced selects are numbered
        // events in their own right (1.3, 3.53, 11.C, 12.C) and should resolve here.
        //
        // NOT gated on the car count. An unlock-everything cheat changes how many
        // cars the browse screen holds, so any threshold would fail on exactly the
        // setups most likely to have one.
        int carCount = 0;
        const uint32_t* cars = PlayerCarList(&carCount);
        if (cars && carCount > 0 && arr) {
            uintptr_t gas = ResolveGasArray();
            if (!gas) {
                g_GasArray = 0;
                g_GasLogged = -1;
            } else {
                const int n = WriteGasList(gas, cars, carCount);
                if (g_Config.LogVehicleArray && n != -2
                    && (gas != g_GasArray || n != g_GasLogged
                        || g_LastSeenCount != g_GasLoggedBefore)) {
                    g_GasArray = gas;
                    g_GasLogged = n;
                    g_GasLoggedBefore = g_LastSeenCount;
                    if (n < 0) {
                        Logger::Log("Car list at 0x%08X: the begin/end/capacity triple "
                                    "did not check out, so nothing was written.", gas);
                    } else {
                        Logger::Log("Car list at 0x%08X: event active, game had %d car(s), "
                                    "now offering %d.", gas, g_LastSeenCount, n);
                    }
                }
            }
        } else if (cars && carCount > 0 && !arr && g_Config.LogVehicleArray) {
            // Visible on purpose: if a forced car-select lands here, the gate is
            // wrong and this is the line that says so.
            uintptr_t gas = ResolveGasArray();
            if (gas) {
                uintptr_t b = gas - kGasBeginOffset, e = gas - kGasEndOffset;
                if (Readable(b, 4) && Readable(e, 4)) {
                    uintptr_t bv = *reinterpret_cast<uintptr_t*>(b);
                    uintptr_t ev = *reinterpret_cast<uintptr_t*>(e);
                    int have = (bv == gas && ev > bv) ? static_cast<int>((ev - bv) / kGasStride) : 0;
                    if (have > 0 && have != g_ReportedCount) {
                        g_ReportedCount = have;
                        Logger::Log("Car list at 0x%08X: %d car(s) on screen with NO event "
                                    "set up, so it was left alone. Expected for the main "
                                    "menu's car browser.", gas, have);
                    }
                }
            }
        }

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
        if (g_EventSlots > 0 && (active || mode)) {
            int slots = g_EventSlots;
            if (slots > kMaxSlotsWritten) slots = kMaxSlotsWritten;

            // A pooled mode redraws only when the event changes. Redrawing every
            // tick would reshuffle the grid underneath the game while it is
            // spawning, and would make the log meaningless.
            if (mode && isNewEvent) {
                size_t ei = EventTable::IndexOf(slot0);
                uint8_t stage = (ei < EventTable::kEventCount) ? mode->eventStage[ei] : 0xFF;
                if (stage < mode->poolCount && mode->poolSize[stage] > 0) {
                    DrawDistinct(mode->pools[stage], mode->poolSize[stage], g_Draw, slots);
                    g_DrawValid = true;
                    if (g_Config.LogVehicleArray) {
                        Logger::Log("%s: drawing %d car(s) from pool %u of %u (%u entries).",
                                    mode->name, slots, stage, mode->poolCount,
                                    mode->poolSize[stage]);
                    }
                } else {
                    g_DrawValid = false;
                    if (g_Config.LogVehicleArray) {
                        Logger::Log("%s: no pool for this event, leaving it alone.",
                                    mode->name);
                    }
                }
            }

            for (int i = 0; i < slots; ++i) {
                uintptr_t slot = arr + static_cast<uintptr_t>(i) * kSlotStride;
                if (!Readable(slot, sizeof(uint32_t))) break;
                uint32_t* p = reinterpret_cast<uint32_t*>(slot);
                uint32_t v = mode ? (g_DrawValid ? g_Draw[i] : 0) : want;
                if (v != 0 && *p != v) *p = v;
            }
        }

        if (isNewEvent && g_Config.LogVehicleArray) {
            DumpArray(arr, preset, g_EventSlots, slot0);
        }
    }
}
