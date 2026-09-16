# NFS The Run — vehicle database

`vehicles.tsv` is the full table: 2272 vehicles with hash ID, database entry, display
name and performance package. Converted from the Vehicle Attributes Table published
with All American Run, which is the authoritative source — an earlier version of this
file was scraped out of the cheat tables and was missing whole models, the Scirocco
among them.

    hash_dec   the ID to write into an opponent or gas-station slot
    hash_le    the same value as little-endian bytes, the form the tables use
    db_entry   vol_gti_mk1_76_pp_rare_1
    name       Volkswagen Golf GTI (Mk1) - Signature Edition
    perf_le    performance package, from the PERFORMANCES sheet

45 manufacturer prefixes, 340 distinct performance packages.

## Tiers are chosen by vehicle ID, not by writing a package

Every vehicle entry already carries its performance package. Cars that share a tier
share a package, so picking the right ENTRY is enough — there is no need to write
anything into the customization template to retier a car. That is what the three Run
mods do, but they do it to give cars custom bodykits and liveries at the same time.
For a single-car mode it is unnecessary.

## Volkswagen Golf GTI (Mk1)

34 entries across 5 performance packages:

### `00 00 00 00` — stock — Tier 1  (9 entries)

    1784507788  8C 6D 5D 6A  vol_gti_mk1_76
     470621649  D1 1D 0D 1C  vol_gti_mk1_76_pp_stock_1
    3721885843  93 78 D7 DD  vol_gti_mk1_76_pp_stock_2
     561262262  B6 2E 74 21  vol_gti_mk1_76_pp_stock_3
    1446568855  97 E3 38 56  vol_gti_mk1_76_pp_stock_4
    3365712787  93 B3 9C C8  vol_gti_mk1_76_pp_stock_5
      77381397  15 BF 9C 04  vol_gti_mk1_76_pp_stock_6
    2733609224  08 91 EF A2  vol_gti_mk1_76_pp_stock_7
    3338495730  F2 66 FD C6  vol_gti_mk1_76_pp_stock_8

### `8C DA 59 71` — NFS Edition tier  (8 entries)

    1840926221  0D 4E BA 6D  vol_gti_mk1_76_ai_002
    2539886783  BF 98 63 97  vol_gti_mk1_76_ai_074
     986928949  35 57 D3 3A  vol_gti_mk1_76_ai_075
    2613494165  95 C1 C6 9B  vol_gti_mk1_76_ai_120
    2089443610  1A 61 8A 7C  vol_gti_mk1_76_ai_121
    1339518974  FE 6F D7 4F  vol_gti_mk1_76_ai_172
    3069328390  06 3C F2 B6  vol_gti_mk1_76_ai_177
    1901714060  8C DA 59 71  vol_gti_mk1_76_nfs_e

### `A1 19 57 B7` — Style Pack 1  (8 entries)

    4006488197  85 28 CE EE  vol_gti_mk1_76_pp1v1
    1150888169  E9 28 99 44  vol_gti_mk1_76_pp1v2
     156286591  7F BE 50 09  vol_gti_mk1_76_pp1v3
     150273431  97 FD F4 08  vol_gti_mk1_76_pp1v4
    2429525574  46 9E CF 90  vol_gti_mk1_76_pp1v5
    1366735751  87 BB 76 51  vol_gti_mk1_76_pp1v6
    2212008718  0E 93 D8 83  vol_gti_mk1_76_pp1v7
     223606344  48 F6 53 0D  vol_gti_mk1_76_pp1v8

### `24 EB 0F CB` — Euro Look Pack  (8 entries)

    4277519707  5B C5 F5 FE  vol_gti_mk1_76_pp2v1
    2420091869  DD AB 3F 90  vol_gti_mk1_76_pp2v2
     393386074  5A 98 72 17  vol_gti_mk1_76_pp2v3
    1122998064  30 97 EF 42  vol_gti_mk1_76_pp2v4
    1958685965  0D 2D BF 74  vol_gti_mk1_76_pp2v5
    2997481304  58 EF A9 B2  vol_gti_mk1_76_pp2v6
    1614707860  94 7C 3E 60  vol_gti_mk1_76_pp2v7
    2768649334  76 3C 06 A5  vol_gti_mk1_76_pp2v8

### `17 EB E4 E6` — Signature Edition — Tier 5  (1 entries)

    3720220186  1A 0E BE DD  vol_gti_mk1_76_pp_rare_1

The Signature Edition, `vol_gti_mk1_76_pp_rare_1` = **3720220186**, is the Tier 5
Golf. The seven `_ai_NNN` entries share the NFS Edition package and are the builds
the game itself uses for Golf opponents, so they are the natural contents of an
opponent array. The eight `_pp_stock_N` entries are Tier 1.

There is no Tier-5 Golf opponent build. A Tier 5 field means putting `pp_rare_1`
into every slot, which is legitimate but means eight identical Signature Editions.

## Volkswagen Scirocco R

Also present (68 entries) and absent from the old cheat-table extraction entirely.
Not wanted for the Golf mode, but its absence is why that extraction is no longer
trusted for anything.
