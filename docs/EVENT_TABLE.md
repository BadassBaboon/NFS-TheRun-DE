# NFS The Run — opponent event table

Extracted from the Stage scripts of All American Run, The Classics Run and The
Supercar Run. All three target the SAME 40 events with the SAME detection IDs and
the SAME slot counts — zero disagreements — so this is one shared table, not three.

`detect` is the value found in slot 0 of the opponent array in a stock game:

    [[[[exe+0x23A4F60]+0x14]+0x68]+0x10]+0x4      slots 8 bytes apart

`slots` is how many entries that event actually uses. Writing more than this
scribbles past the array.

    detect ID     slots
     246197794    9
     746773576    9
    1014047533    9
    1663746137    9
    2940168599    9
    3471684760    9
    3589993730    9
      96674846    8
     703857006    8
    1214321547    8
    1732090001    8
    2360253972    8
    3289858687    8
    3489511903    8
     757287967    7
    2463598039    6
    2952613532    6
    3084366756    6
    3268712543    6
    3481124691    6
    3508062179    6
    3908753765    6
    1770516894    4
    2911404618    4
    3914415322    4
     272462180    3
     570367103    3
    1161287153    3
    1390273791    3
    3083222042    3
    3229489595    3
    4225104048    2
     502950628    1
    1844033689    1
    3149572385    1
    3319259724    1
    3393955391    1
    3418726804    1
    4045851596    1
    4214528836    1

    40 events, slot counts 1-9.

## A better event key

mRally2 identifies events by reading slot 0, which is self-defeating for anything
that then WRITES slot 0 — after the first write the event can no longer be
recognised. The Gas Stations script uses a different field from the same base:

    [[[[exe+0x23A4F60]+0x14]+0x68]+0x10]-0x2C     vehicleAIPreset

which is a stable per-event identifier that nothing overwrites. Fifteen of these
are catalogued (the events with a gas station), tagged by chapter: 2_4, 3_1, 3_2,
3_5, 4_3A, 5_2, 6_1, 6_21, 9_0, 9_1, 9_3, 10_2 and three more.

Prefer the preset for identification and keep the slot-0 table for slot counts.

## Player car

    [[[[[exe+0x2482500]+0x64]+0x1A8]+0x18]+0x1D8]+0    entries 4 bytes apart

Despite mRally2 calling it the gas station array, a dump shows it is the player's
AVAILABLE-CAR LIST: sorted alphabetically by database entry, 57 entries at chapter
6, including presale, adsales, nfs_e and pp_rival variants. Writing one car into
all of it is what makes a pump offer only that car, and why it then shows a long
row of identical cars.

Its length is not stored nearby -- the eight words before it are pointers and
string fragments, none of them a count -- but the walk does not need one here. The
list is contiguous and terminates, so walking while each slot holds a known vehicle
ID finds the end by itself. That bound is NOT safe on the opponent array, where
slot 9 of a nine-slot event holds a valid ID belonging to something else.

    gas - 0x20    the player's CURRENT car

That word read 0 before the list populated and aud_ur_qua_91_pp_stock_1 after,
matching exactly what the player was driving -- not the ai_162 build of the same
model sitting in the opponent array, and not the highlighted carousel entry. One
write there changes the car; the 57-entry write only changes what can be picked.

Freezing the game's own fill is a NOP of `mov [eax],esi` at module offset 0x453549,
which is what the Run mods do. Not needed when overwriting the entries directly.
