<!--
Imported from the wefalltomorrow fork of this project, which did the work.

STATUS: UNRESOLVED. This narrows the cause and rules several things out, but it
does not contain a fix, and neither does that fork -- its own conclusion asks for
the offending startup transition to be identified first. The accompanying
diagnostics ASI lives on the fork's controller-mapping-diagnostic branch and was
deliberately not imported: it is research tooling, not a feature.
-->

# Controller mapping persistence research

## Symptom
Custom controller mappings usually reset to the game's defaults after restarting Need for Speed: The Run. They occasionally persist, but the reset happens on most launches.

## Environment / ruled out
- Game is launched directly from `Need For Speed The Run.exe`.
- Offline; no EA App / Origin client is running.
- Reproduced both with and without the PCGamingWiki x360ce controller fix, so x360ce is not the cause.

## Save data
The relevant files are:
- `Documents\NFSTR\settings\PROF_SAVE_body`
- `Documents\NFSTR\settings\PROF_SAVE_header`

Two snapshots from the same affected setup have now been captured:
1. mappings already reset to the game's defaults;
2. desired custom mappings, captured immediately after changing them and exiting normally, before another launch.

The `PROF_SAVE_body` contains a dedicated binary property named `Vehicle Inputs`.

### Reset/default snapshot
- `PROF_SAVE_header`: 8 bytes
- `Vehicle Inputs`: 1312 bytes
- `Vehicle Inputs` magic: `0xABCDEF01`
- entry count: 43
- `Vehicle Inputs` SHA-256: `44e37a2efad30ca4a887f8d8a75f1146f7b7592f3601270e1649316ebc03669e`

### Desired/custom snapshot
- `PROF_SAVE_header`: byte-for-byte identical to the reset snapshot
- `Vehicle Inputs`: 1776 bytes
- `Vehicle Inputs` magic: `0xABCDEF01`
- entry count: 51
- `Vehicle Inputs` SHA-256: `46117a1b3df484d7f207e58b4c47f07f388b8063700348072194a6f1dbea9e09`

### Binary diff result
The two 1,560,576-byte `PROF_SAVE_body` files are identical everywhere except:
- bytes `0x00000000..0x00000003` (the body checksum/hash), and
- the `Vehicle Inputs` property/value region beginning at file offset `0x120F41` (decimal 1,183,553).

The body data before `Vehicle Inputs` is identical, and everything after the end of the larger custom block is also identical. The reset save's unused space where the larger custom block would extend is zero padding.

This is decisive: **the game successfully writes the custom controller mappings to disk when they are changed.** The persistence bug is therefore not "custom mappings never save". A later startup is replacing the stored `Vehicle Inputs` data with a different/default block and subsequently saving that replacement.

The property format observed in both snapshots is:

```text
uint32 valueSize
uint32 magic = 0xABCDEF01
uint32 entryCount

repeat entryCount times:
    uint32 actionId
    uint32 bindingCount
    repeat bindingCount times:
        uint32 field0
        uint32 field1
        uint32 field2
        uint32 field3
```

The custom and reset blocks differ structurally as well as in individual bindings: 51 actions are serialized in the desired mapping versus 43 after the reset.

## Executable targets
The v1.1 executable contains both `OnInputDeviceChanged` and `Vehicle Inputs` in the same input/profile subsystem.

Known code in the tested executable:
- `OnInputDeviceChanged` is dispatched at `0x00927133`.
- `Vehicle Inputs` wrapper A is `0x00927140` and is also the profile object's vtable slot `+0x28`.
- wrapper B is `0x00927150`.
- wrapper C is `0x00927160` and begins by setting byte `[ecx+0x4E] = 1`.
- the surrounding input-state updater is `0x009270A0`; it tracks a state field at object offset `+0x3C` and a global device flag at `0x0289227E`.
- when state changes between 2 and 3 it dispatches event/hash `0x4BB4E56D` at `0x009270DC`.
- when the updater resets state to 0 it reaches the literal `OnInputDeviceChanged` dispatch at `0x00927133`.

## First diagnostic run: mappings stayed correct
A first logging-only ASI run was made with the custom save present. The mappings **remained correct** on that launch.

Observed wrapper activity during the run:
- wrapper A (`0x00927140`): 3 entries;
- wrapper B (`0x00927150`): 1 entry;
- wrapper C (`0x00927160`): 0 entries.

The initial diagnostic also hooked `0x009270A0` as a nearby input-device-change candidate. That site fired about 2,900 times in roughly 49 seconds, proving the function itself is a hot updater rather than a useful one-shot signal. Logging that hot path was removed after this run.

## Second diagnostic run: mappings reset
Diagnostic v2 removed the hot-path hook and instrumented only A/B/C, including caller return addresses. The mappings reset on this launch.

Observed sequence:
1. wrapper A, caller `0x00866727`;
2. wrapper B, caller `0x00882C63` (direct call at `0x00882C5E`);
3. wrapper A, caller `0x00882C95` (vtable call returning at that address);
4. wrapper A again, caller `0x00866727`.

All four calls used the same profile object during the run. Wrapper C again never ran.

This rules out the simple theory that wrapper C is the operation that resets the mappings. A and B participate in the lifecycle, but their presence/count alone is not enough to distinguish a good launch from a reset launch: the first good run showed the same aggregate A=3, B=1, C=0 pattern.

The call sites around `0x00882C40`, `0x00882C70`, and `0x00882CA0` form three small profile/event handlers. The B path calls wrapper B directly; the middle path invokes wrapper A through the profile object's vtable slot `+0x28`; the C path would call wrapper C but did not execute in either captured run.

## Current conclusion
The strongest working theory remains a **startup/device-initialization race/overwrite**:

1. profile load restores the desired `Vehicle Inputs` data;
2. controller/device initialization can rebuild the live vehicle bindings from defaults;
3. on bad launches the default state wins and is later serialized back through the normal profile lifecycle;
4. on good launches the custom state survives.

Because the reset run does not uniquely differ in A/B/C call counts, the next useful discriminator is the actual device-state transition path rather than another wrapper-only count.

## Diagnostic v3
Diagnostic v3 keeps A/B/C instrumentation and adds hooks only at two cold event sites inside the otherwise-hot input updater:
- `0x009270DC`: state-2/3 transition event; logs resulting `+0x3C` state and global device flag;
- `0x00927133`: actual `OnInputDeviceChanged` dispatch; logs state and device flag.

It deliberately does **not** log every call to `0x009270A0`.

The preferred permanent fix remains generic: preserve the profile-loaded `Vehicle Inputs` state across the incorrect startup device transition/rebuild once that transition is identified. Do not hardcode one user's button mappings and do not continuously rewrite the save file.

Keep this fix separate from the framerate code.
