#ifndef EVENT_TABLE_H
#define EVENT_TABLE_H

#include <cstdint>
#include <cstddef>

// How many opponent slots each event uses.
//
// The key is the value found in slot 0 of the opponent array in a STOCK game, so
// it identifies an event only until something overwrites that slot. Callers must
// look up once per event and remember the answer.
//
// Extracted from the Stage scripts of All American Run, The Classics Run and The
// Supercar Run. All three cover the same 40 events with the same detection values
// and the same slot counts, with zero disagreements between them, so this is one
// table rather than three merged.
//
// An event NOT in this table is left alone. Guessing a count is the one thing that
// can do real damage here: writing nine slots into a one-slot Battle Race walks
// eight entries past the end of the array into whatever follows it.

namespace EventTable {
    struct Entry { uint32_t detect; uint8_t slots; };

    static const Entry kEvents[] = {
        { 96674846u, 8 },
        { 246197794u, 9 },
        { 272462180u, 3 },
        { 502950628u, 1 },
        { 570367103u, 3 },
        { 703857006u, 8 },
        { 746773576u, 9 },
        { 757287967u, 7 },
        { 1014047533u, 9 },
        { 1161287153u, 3 },
        { 1214321547u, 8 },
        { 1390273791u, 3 },
        { 1663746137u, 9 },
        { 1732090001u, 8 },
        { 1770516894u, 4 },
        { 1844033689u, 1 },
        { 2360253972u, 8 },
        { 2463598039u, 6 },
        { 2911404618u, 4 },
        { 2940168599u, 9 },
        { 2952613532u, 6 },
        { 3083222042u, 3 },
        { 3084366756u, 6 },
        { 3149572385u, 1 },
        { 3229489595u, 3 },
        { 3268712543u, 6 },
        { 3289858687u, 8 },
        { 3319259724u, 1 },
        { 3393955391u, 1 },
        { 3418726804u, 1 },
        { 3471684760u, 9 },
        { 3481124691u, 6 },
        { 3489511903u, 8 },
        { 3508062179u, 6 },
        { 3589993730u, 9 },
        { 3908753765u, 6 },
        { 3914415322u, 4 },
        { 4045851596u, 1 },
        { 4214528836u, 1 },
        { 4225104048u, 2 },
    };

    static const size_t kEventCount = sizeof(kEvents) / sizeof(kEvents[0]);

    // Returns 0 for an event that is not in the table, which means "do nothing".
    inline uint8_t SlotsFor(uint32_t detect) {
        for (size_t i = 0; i < kEventCount; ++i) {
            if (kEvents[i].detect == detect) return kEvents[i].slots;
        }
        return 0;
    }
}

#endif // EVENT_TABLE_H
