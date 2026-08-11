# NFS The Run — reverse engineering notes

Working notes for NFSTR-DE. Target is **Need for Speed The Run v1.1.0.0** (32-bit,
Frostbite 2, ImageBase `0x00400000`). Every address below is written as an absolute VA;
subtract `0x400000` for the module offset used with `Memory::GetGameBase()`.

Tools: `IDA_Dump/Need For Speed The Run_dump.i64` (Brawltendo's database, with the MAP
and full Hex-Rays export beside it), `ReClass Setup/NFSTR.rcnet` (262 reversed classes),
and the community Cheat Engine tables in the parent folder.

---

## 1. Reading addresses out of the CT tables

Two different address forms appear in the Cheat Engine tables and mixing them up costs
hours. This bit us once already.

| Form in the .CT | Meaning | Conversion |
|---|---|---|
| `"Need for Speed The Run.exe"+E5EEF6` | already a module offset | use `0xE5EEF6` as-is |
| `00E4EB60:` (bare) | absolute VA | subtract `0x400000` |

The exe's `.text` maps file offset 1:1 to RVA (`VA = raw = 0x1000`), so a byte
signature's file offset in the exe equals its module offset.

---

## 2. The Frostbite settings system

This is the most productive thing found so far. The engine keeps one settings object per
settings class, and they are reachable at runtime.

**Do not bother with `Game.cfg`.** The game does read one (`"Game.Level is not set in
Game.cfg"`), but the retail build ignores a hand-made file next to the exe. Tested and
confirmed dead.

### Universal accessor

```c
container = fb::SettingsManager::getContainer(fb::g_settingsManager,
                                              &fb::XxxSettings::c_TypeInfo);
```

| Symbol | VA |
|---|---|
| `fb::SettingsManager::getContainer` | `0x004E72D0` |
| `fb::g_settingsManager` (pointer variable) | `0x02846C74` |

Calling convention is `__thiscall(this, const TypeInfo*)`. From MinGW, declare it
`__fastcall(self, edx_ignored, typeInfo)` — `ecx` carries `this`, the callee ignores
`edx`, and both conventions are callee-clean.

`sub_E68920` is a good reference: it fetches six different containers and prints
`WorldRender.Enable`, `ShaderSystem.FlushEnable`, `VisualTerrain.Enable`,
`TimingView.Enable`, `DebrisSystem.Enable`, `EmitterSystem.Enable`.

### Cached pointers

Some classes also have a cached global, filled the first time the accessor runs. Handy
because they can be pasted straight into ReClass with brackets to dereference.

| Class | Cached pointer | ReClass address |
|---|---|---|
| `GameTimeSettings` | `0x02753F20` | `[02753F20]` |
| `GameRenderSettings` | `0x02753F28` | `[02753F28]` |

They sit 8 bytes apart in the same table, so other classes' cached slots are likely
nearby. `sub_418010` shows the pattern for `GameRenderSettings`.

### Naming convention

The config prefix is the class name minus the trailing `Settings`:
`GameRenderSettings` → `GameRender.`, `VisualTerrainSettings` → `VisualTerrain.`

### Finding any other class

1. Look up `?c_TypeInfo@<Class>@fb@@` in the `typeinfo` segment.
2. `xrefs_to` that address; the small callers are the accessors.
3. Decompile one to see its cached global, or just call `getContainer` yourself.

Or simply set `LogSettingsContainers = 1` in the mod's INI — it calls the accessor for a
curated list and logs every container address for pasting into ReClass.

**Timing matters.** Roughly two thirds of the containers exist from early startup, but the
world subsystems — `WorldRender`, `VisualEnvironment`, `GlobalPostProcess`, `Vegetation`,
`EmitterSystem`, `Debris` — do not register until a level is loaded and the player has
control of the car. The probe therefore samples on vehicle-control transitions rather than
on a timer. Probing at the main menu finds 17 of 24; probing once driving finds 23 of 24.
Only `OcclusionSettings` has never appeared.

---

## 3. `fb::GameRenderSettings` (verified live)

Container at `[0x02753F28]`, size 244 bytes. Offsets confirmed against the running game.

| Offset | Type | Field | Stock | Notes |
|---|---|---|---|---|
| `0x0C` | float | `ForceRoll` | 0 | rolls the camera; independent of the shift gate |
| `0x14` | float | `ForceRenderShiftX` | 0 | horizontal viewport shift |
| `0x18` | float | `ForceWorldFadeAmount` | -1 | -1 = disabled |
| `0x1C` | float | `ForceBlurAmount` | -1 | **not** motion blur, no visible effect |
| `0x20` | float | `ForceFov` | -1 | -1 = engine default, which is **48** |
| `0x28` | u32 | `DrawFpsMethod` | 1 | |
| `0x34` | float | `ForceRenderShiftY` | 0 | engine's own value is **-0.017** |
| `0x3C` | float | `NearPlane` | 0.1 | |
| `0x48` | float | `ViewDistance` | 20000 | **no visible effect** |
| `0x77` | bool | `InitialClearEnable` | false | **fixes the glitchy/invisible minimap** |
| `0x7B` | bool | `DrawFps` | false | **no visible effect** |
| `0x7C` | bool | `PerfOverlayVisible` | false | |
| `0x81` | bool | `EmittersEnable` | true | |
| `0x8B` | bool | `ForceRenderShiftEnabled` | false | gates `ForceRenderShiftX/Y` |

Shipped in the mod's `[RENDER]` section: `ForceFov`, `ForceRenderShiftEnabled`,
`ForceRenderShiftX/Y`, `ForceRoll`, `FixMinimapRendering`. The three marked "no visible
effect" were tested and dropped, likely debug-build leftovers.

---

## 4. `fb::GameTimeSettings` (verified live)

Container at `[0x02753F20]`. The active single-player instance has `Name = 'GameTime'`;
a second instance named `'Online'` exists with junk framerate values — **do not write to
that one**.

| Offset | Type | Field |
|---|---|---|
| `0x0C` | char* | `Name` |
| `0x24` | float | `forceSimRate` |
| `0x28` | float | `maxVariableFps` |
| `0x30` | float | `forceDeltaTime` (-1 = off; setting it broke the camera) |
| `0x38` | float | `timeScale` |
| `0x40` | bool | `variableSimTickTimeEnable` |

The mod's GameTime hook at `exe+0xA607F7` captures `variableSimTickTimeEnable`
(`base + 0x40`), so `maxVariableFps` is reachable at `captured - 0x18`.

`fb::GameTimer` itself is static at `0x0288AD00`: `sleepMode` `+0x28`,
`gameTimeSettings` `+0x2C`, `simRate` `+0x30`. `SleepMode` enum:
`BusyWait=0, VSync=1, Sleep=2, NoBlock=3, StableTicksFastFrames=4`.

---

## 5. Engine audio (Ginsu)

The engine sound is an `EA::Audio::Core::GinsuPlayer` granular synth. The voice render
function is `0xD9D2D0`–`0xD9DB3A` — **it is not defined as a function in the IDA
database**, which is why F5 fails on it; run `define_func` at `0xD9D2D0` first.

`GinsuPlayer` layout (derived from code):

| Offset | Field |
|---|---|
| `0x30` | attribute: requested frequency |
| `0x58` | attribute: slew time in ms |
| `0x60` | `mPlaying` |
| `0x64` | `mOutputSamplesRequested` |
| `0x68` | `mSynthData` |
| `0x1C0` | `mfPreviousFrequency` |
| `0x1C4` | `mfTargetFrequency` |
| `0x1C8` | `mfCurrentFrequency` |
| `0x1CC` | `mfSlewSamplesRemaining` |
| `0x1D0` | `mSampleRate` |
| `0x1E0` | `mPlaybackPos` |
| `0x1E8` | `mCurrentMixTime` (double) |

**The high-FPS bug.** Pitch glides from previous to target over
`slewTimeMs * sampleRate / 1000` samples, and the glide is re-armed on every frequency
change (`0xD9D424`–`0xD9D438`). Right after a re-arm the interpolation ratio is 0, so
`current = previous`, unchanged. At 30 FPS enough render calls elapse between RPM changes
for the glide to progress; above 30 a re-arm lands on nearly every pass and
`mfCurrentFrequency` never leaves its 1000 Hz starting value. Confirmed by logging: at
144 FPS `target` tracked RPM perfectly while `current` sat frozen at 1000.

Note this is independent of the glide *length*, which is why an earlier patch that
resized it at `0xD9D408` did nothing.

**Fix (shipped):** stop the glide re-arming. `FixEngineAudioSlew = 1` NOPs the 8-byte
`movss [ebx+0x1CC], xmm1` at `0xD9D42A`. Mode 2 replaces the 28-byte block at `0xD9D424`
with a cave that only re-arms once the glide finished — tested and worse, since `current`
then trails `target` through the rev range.

Related dead ends: throttling `fb::NFSCombustionEngine::Update` (`0x00683310`) to 30 Hz
barely changed anything; `GinsuPlayer::Something` (`0xD9DD20`) is grain setup, not the
per-frame advance.

---

## 6. The backfire flame, and why the sim-tick approach was dropped

Forcing `variableSimTickTimeEnable` on while driving (a setting this mod used to
ship as `FixSimTickWhenDriving`) made particles behave, but that flag is a single
global bit on the timer: it changes the simulation timestep for everything.
Camera springs went rigid, collisions glued the car to guardrails and traffic,
and inputs were dropped. It also stopped the exhaust backfire flame from drawing,
because burst particles are given a lifetime counted in simulation frames but
aged once per rendered frame, so at 144 FPS against a 30 Hz simulation they
expired before they were drawn.

The setting was removed. Kickup particles are fixed properly in section 9, which
touches nothing but the inherited emitter velocity, and with the sim rate left
alone the backfire flame draws correctly again. There is no per-emitter or
per-system equivalent of that flag, so scoping it to particles was never possible.


## 6b. `fb::WorldRenderSettings` (tested)

Resolved through the settings manager, not a cached pointer, and only after a level
has loaded. Offsets from the game's reflection data; see `SETTINGS_FIELDS.md` for the
full class.

| Offset | Field | Result |
|---|---|---|
| `0x044` | `ShadowmapResolution` | **works.** 2048 at the highest in-game preset; 4096 is visibly sharper |
| `0x048` | `ShadowmapQuality` | **works.** 1 gives soft edges; 0 or 2 appear to switch filtering off, leaving hard edges |
| `0x058` | `ShadowmapViewDistance` | **works.** stock 200 |
| `0x04C` | `ShadowmapSliceCount` | no effect |
| `0x0B8` | `MultisampleCount` | no effect |
| `0x098` | `MotionBlurScale` | no effect |
| `0x0A0` | `MotionBlurQuality` | no effect |
| `0x0AC` | `MotionBlurMaxSampleCount` | no effect |
| `0x1AE` | `MotionBlurEnable` | no effect, does not toggle motion blur |

These are read once when the renderer sets up a level, so they take effect from the
next event rather than mid-race. Only the three that work are shipped.

---

## 7. Other verified addresses

| What | Address | Notes |
|---|---|---|
| GameTime hook (sim tick / max fps) | `exe+0xA607F7` | 6 bytes `8A 48 40 8B 43 08` |
| Vehicle-control check | `exe+0x3F6C73` | 5 bytes `80 7E 04 00 57`; **FusionFix hooks this too** — chain onto its stub rather than replacing it |
| Traffic density scale | `exe+0xE5EEF6` | `F3 0F 10 01 83 E0 FC` |
| Traffic max density | `exe+0xE5EEE9` | `F3 0F 10 50 1C` |
| Traffic vehicle limit | `exe+0xE5A9A3` | `8B 40 60 83 F8 19` |
| Hidden UI options | `0x00968F50` | `JNE` → `JMP` |
| Vehicle assists | 13 sites | see `src/features/vehicle.cpp` |
| `fb::NFSCombustionEngine::Update` | `0x00683310` | |
| `fb::CinebotCamera::commitShot` | pattern `E8 ? ? ? ? 8B 44 24 ? 0F 57 D2` | shot struct: eye `+0`, target `+16` |

`ERaceLineAssistStatus`: `Off=0, AlignToRoad=1, Grip=2, MiniDrift=3, Drift=4,
DriftExit=5, LinkDrift=6`.

---

## 8. Cheat-table crash "fixes" — do not ship

The Chicago Interstate and Tunnel of Pain crash bypasses from mRally2's table only exist
to stop crashes caused by *other* cheats (debug menu, unreleased events). In a normal
race they blank live game code and the AI opponents pull to the side of the road and
stop. Verified by bisection: with everything else off and the fixes disabled, the game
boots and the AI is fine. The code is parked in `research/crash_fixes.cpp`.

---

## 9. Kickup particles

Kickup emitters (snow spray, drift smoke, wet spray, dirt dust) set, in their EBX
data:

    InheritSpeedAndDirectionFromEmitter = True
    InheritSpeedScaleAmount             = ~0.759

so each particle inherits part of the emitter's velocity. The emitter is the
wheel. Above 30 FPS that inherited velocity is over-scaled, so particles spawn in
the right place and then streak sideways or straight up. It repeats at the same
point on a track because the direction comes from the wheel's motion and the
terrain there.

Of 897 emitters in the game data, 124 set the inherit flag. Only kickup looks
broken because the rest are cutscene effects, which run at a clamped 30 FPS, or
effects on near-stationary emitters with no meaningful velocity to over-scale.

`SpawnDirectionData` owns the fields: `DirectionFromEmitterOrigin` `0x30`,
`InheritSpeedScaleAmount` `0x34`, `InheritSpeedAndDirectionFromEmitter` `0x38`,
`UseProcessorForSpeedScale` `0x39`.

Two sites apply it, same shape with different base registers:

| Site | Instruction | Guard |
|---|---|---|
| `0x01385D3B` | `movss xmm0,[esi+0x34]` | `cmp byte [esi+0x38],0` @ `0x01385D14` |
| `0x013895FB` | `movss xmm0,[eax+0x34]` | `cmp byte [eax+0x38],0` @ `0x013895D4` |

The contribution is `InheritSpeedScaleAmount * emitterVelocity`, so scaling the
first term cancels the over-scale. Each site is exactly 5 bytes and is replaced
with a jump to a cave that loads the field and multiplies it by `30 / FPSLimit`.

## 10. Anisotropic filtering (fb::ShaderSystemSettings)

`MaxAnisotropy` is an int32 at offset 0x94, confirmed both in the game's own
reflection data and read live in ReClass. It ships at 4. The engine rewrites it
back to 4 whenever a level loads, so it has to be reapplied rather than set once.

The container is resolved with the standard `getContainer` call using
`fb::ShaderSystemSettings::c_TypeInfo` at 0x2AA3428.

Neighbouring fields, for reference: `ZOnlyMaxAnisotropy` 0x84, `MipmapBias` 0x9C
(float), `DxMaxInstructionCount` 0xA4, `DatabaseLoadingEnable` 0xB0.

`fb::DxDisplaySettings` was checked against ReClass at the same time and matches
exactly: `FullscreenWidth` 0x18, `FullscreenHeight` 0x20, `FullscreenRefreshRate`
0x2C, `VSyncEnable` 0x40, `FullscreenModeEnable` 0x42, `Fullscreen` 0x48.

### Shader and streaming flags that do nothing

`DxImmutableUsageEnable`, `InstantUnloadingEnable`, `PushBasedLoadingEnable`,
`PriorityThreshold`, `OnDemandBuildingEnable` and `DxMaxInstructionCount` were all
tested against load times and visuals. None changed anything, and two of them are
written back to their stock values on the next level load. Load times in this game
are not gated by anything reachable from these settings.

An earlier note in this file gave `DxParallelShaderLoadingEnable` at 0x10C and
`DxDelayedShaderLoadingEnable` at 0x10B. Those came from a heuristic that assumed a
field record follows each string, and strict containment showed no class array holds
them. Treat them as unverified; they are not used anywhere in the build.

## 11. Player vehicle (fb::NFSVehicle::m_health)

`m_health` is a float at 0x1878, read live in ReClass. It sits at 100 and falls as
the car takes damage; the wreck screen fires once it drops far enough.

NFSVehicle is a heap object with no settings container, so it is reached through the
pointer chain _mRally2's Master Table uses for its vehicle lights, wrecked trigger
and explosion request entries:

    [[[[[[exe+0x26858B0] + 0x88] + 0x38] + 0xD0] + 0x14C] + 0x8]

Relative to that final object the table reads a lights byte at 0xAE3, a wrecked
trigger at 0xAEA and an explosion request at 0xAEC, which is consistent with a
multi-kilobyte vehicle object that also carries m_health at 0x1878.

Every link is VirtualQuery-checked before it is followed and the health value is
range-checked before it is written, so a chain that lands somewhere else on another
build logs and skips instead of corrupting memory. The write repeats each tick,
because damage is applied continuously and a one-shot write would be undone by the
next collision.

The control-check site at exe+0x3F6C73 was ruled out as a route to the vehicle: its
`esi` has fields at 0x10, 0x30 and 0xD0 only, so it is a camera or input controller.

## 12. Renaming Extreme in the menus

The difficulty label and its description are localised strings loaded from the loc
bundles onto the heap, so they have no fixed address — they were found by hand at
0xF5E56A2C and 0xF5E569A5 in one session and will be elsewhere in the next. They are
narrow ASCII, not UTF-16: the two buffers sat 0x87 bytes apart, and an odd gap rules
out a two-byte encoding.

They are therefore located by content at runtime, with two rules that keep that from
being reckless:

- The description is found by the fragment `forewarned` rather than its full text, so
  a difference in wording or punctuation does not make the search silently fail.
- `EXTREME` on its own matches 28 places in memory. It is only accepted when it is
  null-terminated on both sides, making it a whole string rather than a fragment, and
  only within 0x2000 bytes of the description buffer already found.

There is more than one copy, and every one in the window has to be rewritten. Two sit
back to back at 0xF5E56A24 and 0xF5E56A2C, exactly the eight bytes of `EXTREME `
apart, and the menu draws the *second*. Renaming only the first changed a string
nothing was displaying, which looked exactly like the rename failing. That adjacency
is also why the label has no padding to grow into: the byte after one terminator is
the `E` of the next copy, so seven characters is the hard limit.

Note that `Memory::FindAllPatternsProcess` cannot be used for this: it stops at
0x20000000, and these buffers live above 0xF0000000 because the game is large-address
aware. The scan in difficulty_text.cpp walks the whole user address space instead.

Replacements are written into the game's own buffer, so they cannot be longer than
what is already there. A buffer's capacity is its text plus any run of zero bytes
after its terminator, capped at 16 — allocators commonly pad, but a long run of zeroes
is more likely to be a different allocation than slack. Anything longer is truncated
and the log reports the exact number of characters that fit.

`EXTREME` is seven characters and the copies sit back to back with no padding, so
seven is the hard ceiling for the replacement label. The shipped `DEADLY` is six.

## 13. Nitrous (fb::NFSVehicle::collectRaceCarInputState)

IDA names this function, so there is no ambiguity about the site:

    0x0069B064  mov [esi+0B5h], bl            <- NOS input flag
    0x0069B06A  test byte ptr [edi+1882h], 8
    0x0069B08C  mov [esi+0B6h], al            <- second "extra NOS" flag

`esi` is the `EA::VehiclePhysics::RaceCar::InputState` being filled in and `edi` is
the NFSVehicle — 0x1882 sits just past m_health at 0x1878, which corroborates both.
Two independent cheat tables hook this same instruction to kill nitrous: mRally2's
Master Table ("Nos Enabled") and the All American Run table ("NOS Disable").

The useful property is that this runs every frame during input collection, not once
at vehicle spawn. Zeroing the flag means nitrous never registers as pressed, and it
can be toggled live — no race reload is needed for the change to take or to stop.

Only 0xB5 is touched. 0xB6 is derived from vehicle state rather than input, and
suppressing the input flag already prevents the boost from being requested.

Original bytes: `88 9E B5 00 00 00` (6). The obvious replacement,
`mov byte ptr [esi+0B5h], 0`, assembles to 7 and does not fit, which is why both
cheat tables use a code cave. This mod does too, with one addition: the cave tests a
flag and either passes the real input through or writes zero, so engaging and
disengaging the difficulty never rewrites executable bytes. The cave clobbers only
the flags register, which is safe because the instruction it returns to is a `test`.

## 14. Vehicle assists are mostly an AI handicap

Play-testing found the `DisableAllVehicleAssists` patches made no perceptible
difference to the player's car while AI cars slid, crashed and dropped off the pace.
Decompiling `fb::NFSVehicle::collectRaceCarInputState` explains both halves:

- `racelineAssistForceScalar` and `racelineAssistTorqueScalar` are written into
  every vehicle's `RaceCar::InputState` with no `isHumanPlayer` gate. They are
  shared vehicle physics, and the AI depends on the racing line far more than a
  human does.
- Two of the patches invert an `isHumanPlayer` test rather than disabling anything.
  At 0x69B167 the original is `if (isHumanPlayer && ...) alignToRoad = 1`, so
  flipping the branch grants the assist to AI cars and removes it from the player.
  0x69B5E2 has the same shape, and despite the "OverrideDriftIntent" name it
  inherited from the cheat table it actually gates `nosRechargeScalar`.

Consequence: enabling them makes a race EASIER. They are kept as an option but
documented for what they are, and Run For Your Life does not use them.

## 15. Checkpoint resets are called "rewinds"

`_c4/gameplay/TheRun/TheRunInfo.xml` (`StoryModeInfo`) holds the career config:

    Difficulties          = [Easy, Normal, Hard, Extreme]
    RewindsPerDifficulty  = [ 10,    5,     3,     1    ]
    UnlimitedRewindsValue = 0x63 (99)

This independently confirms the RaceAIDifficulty ordering used in difficulty.cpp,
and matches the single reset observed on Extreme. Individual events can override
the count through `OverrideRewindsPerDifficulty`, and `HideNumRewindsRemaining`
controls whether the counter is drawn.

To force zero rewinds the array has to be found in memory. It is an EBX asset
instance rather than a settings container, so getContainer cannot reach it, but
the four values are a distinctive 16-byte signature:
`0A 00 00 00  05 00 00 00  03 00 00 00  01 00 00 00`, optionally followed by
`63 00 00 00` if UnlimitedRewindsValue is stored inline after it. Same approach as
the menu-string search in difficulty_text.cpp. Not yet implemented.

## 16. The nitrous HUD widget

`_c4/UI/Assets/WidgetHudNitrous` is a bare `UIWidgetAsset` — no `WidgetEvents`,
no `WidgetFunctions`, and no `UIBoolDataSourceInfo` visibility binding of the kind
the difficulty menu items use. There is therefore no data-driven flag to flip to
hide it; suppressing it would mean intercepting the widget where the HUD composes
it, which is a much larger job than the nitrous suppression itself. Left alone.

## 17. Drafting and rewinds, implemented

### Drafting (player only)

Written at the tail of `collectRaceCarInputState`:

    0x0069B65C  movss [esi+2B0h], xmm0     drafting
    0x0069B680  movss [esi+2B4h], xmm0     draftingSpeed
    0x0069AAD3  mov   [esi+102h], al       isHumanPlayer

The All American Run table writes [esi+2B0h] for its own "Drafting Disable", which
corroborates the offset. Both fields are filled for every car, so blanking them
outright would strip the AI's slipstream too and hand the player an advantage — the
exact trap the driving assists fall into. The cave keys on `isHumanPlayer` instead.

The hook sits on the second write (8 bytes, room for a 5-byte jump); by then both
values exist, so the cave clears drafting and substitutes zero for the draftingSpeed
about to be stored. It stays off the x87 stack, which has a live value pushed at
0x0069B67A and popped at 0x0069B688, and only clobbers flags, which nothing reads
before the function returns.

### Rewinds

RewindsPerDifficulty is found by its contents — four int32s of 10, 5, 3, 1. Since a
signature can collide, every candidate is logged with the dword after it, and a
match followed by 99 (UnlimitedRewindsValue) is treated as confirmed. A lone
unconfirmed match is accepted; several are not, and nothing is written. Only index 3
is touched, so it is safe to apply regardless of the difficulty selected.

### Player-only assists, not implemented

The same isHumanPlayer trick would work for the driving assists, which is the honest
version of what the cheat tables were reaching for. The fields are:

    [esi+0x290]  racelineAssistForceScalar   (fstp at 0x0069B48E)
    [esi+0x294]  racelineAssistTorqueScalar  (fstp at 0x0069B4C4)
    [esi+0x299]  alignToRoad                 (mov  at 0x0069B1A9, byte)

## 18. Scanning live game memory safely

The first version of the rewind search crashed the game a few seconds after the
menu strings were renamed. Three separate problems, all worth remembering:

**Reads must not fault.** The scan called `VirtualQuery`, saw `MEM_COMMIT`, then
compared the pages directly. The game allocates and frees constantly while
loading, so a region can be freed between the query and the comparison, and
touching it then is an access violation. The scan now copies each region in 64 KB
chunks through `ReadProcessMemory` on our own process, which fails cleanly on a
region that has gone away instead of taking the game down. Verified against a
deliberately decommitted allocation.

**Chunking must overlap.** Consecutive chunks overlap by `len - 1` bytes so a
match lying across a boundary is still found. Verified with a signature planted
three bytes before a 64 KB boundary.

**The scanner must skip its own module.** The needle being searched for is, by
definition, also sitting in our own memory, so every search finds our copy of it.
For the rewind array that phantom candidate would have made the count ambiguous
on every run, and the "refuse when ambiguous" rule would then have meant the
feature never applied. `ScanWritableAll` now skips regions whose `AllocationBase`
is this .asi.

Why the string search survived all of this and the rewind search did not: the
string search asks for one hit and stops, and its needles are string literals in
read-only `.rdata`, which is never scanned. The rewind search asks for up to
sixteen, so it always walked the entire address space.

The rewind search now looks for the twenty-byte `10, 5, 3, 1, 99` first and only
falls back to the sixteen-byte array, and gives up after 24 passes rather than
rescanning forever.

## 19. Player-only assists, and what the rewind UI does not do

### Assists, done properly

Section 14 established that the cheat-table assist patches degraded the AI more
than the player. The fix is to stop patching shared physics and instead blank the
player's own fields, keyed on the flag the game already computes:

    [esi+0x102]  isHumanPlayer               mov  at 0x0069AAD3
    [esi+0x290]  racelineAssistForceScalar   fstp at 0x0069B48E
    [esi+0x294]  racelineAssistTorqueScalar  fstp at 0x0069B4C4
    [esi+0x299]  alignToRoad                 mov  at 0x0069B1A9 (byte)

One cave at 0x0069B680 handles this and drafting together, because that write is
the last of the group and eight bytes wide. The thirteen byte patches are gone.

### The rewind HUD

Zeroing `RewindsPerDifficulty[3]` changes the gameplay correctly — a crash goes
straight to game over — but the HUD is not driven from that array. It still shows
a reset count, the reset button does nothing, and the reset message appears with
an infinity glyph before the game-over screen.

The count shown was 2 where Extreme grants 1, and the candidate accepted in
testing had no trailing `99`, so it is not confirmed that the array found is the
career's own. The remaining-rewinds counter the HUD reads is a separate runtime
value that has not been located; `UIRewindDataBinding` in
`_c4/UI/Flow/Screen/LoadScreens/RewindLoop.xml` binds it to `NFSUIRaceInfoComp`
with DataKey 0x43b317e6, which is the thread to pull next.

The feature was removed from the build and parked in `research/rewinds.cpp`. The
memory-scanning approach in it is sound and reusable; what is missing is the HUD
counter, not the search.

## 20. Nitrous was not player-only either

The first nitrous cave zeroed `nosEnabled` for every vehicle. The decompiler shows
AI cars get the flag too:

    v25 = a4 == 0 && playerSpawnType;        // playerSpawnType != 0 means AI
    v26 = (this->dword187C & 0x20000) != 0 || v25;
    raceInputState->nosEnabled = v26;        // 0x0069B064

so the difficulty was quietly removing nitrous from the whole field, which makes a
race easier. Same failure as the cheat-table assist patches, found the same way —
by asking whether a field is written for AI cars as well before touching it.

Now gated on `isHumanPlayer` at `[esi+0x102]`, like drafting and the assists. The
rule this establishes for anything added to the mode: before suppressing a field
in `collectRaceCarInputState`, check whether the AI reads it too.

## 21. AI difficulty scalars — the next lever

`sub_1261E40` copies an AI performance struct and multiplies two of its fields by
per-difficulty scalars:

    0x01261EB8  a2+8  *= glueScalar        (cheat-table site exe+0xE61EA1)
    0x01261EF6  a2+12 *= difficultyScalar  (cheat-table site exe+0xE61ED9)

Both scalars are read from `[eax]` at those sites, which is what mRally2's
"Difficulty Scalar" and "Glue Scalar" scripts overwrite — with 5.0 and 0.7
respectively. Being AI-side, these cannot leak onto the player's car, which makes
them a much safer lever than anything in `collectRaceCarInputState`.

Implemented in ai_difficulty.cpp. Verified bytes at both sites:

    0x01261EA1  F3 0F 10 00  F3 0F 10 4E 08   movss xmm0,[eax] / movss xmm1,[esi+8]
    0x01261ED9  F3 0F 10 00  F3 0F 10 4E 0C   movss xmm0,[eax] / movss xmm1,[esi+0Ch]

Nine bytes each, which is the 5-byte jump plus padding, and the same span the
cheat table replaces. Each cave reproduces both instructions with a `mulss`
between them, so the scalar the game read is scaled before it is applied. `mulss`
does not write the flags register, so unlike the input-state caves there is
nothing to reason about on the return path.

`DeadlyAiSkillScale` and `DeadlyAiGlueScale` are temporary INI knobs at 1.0, which
is a no-op. They exist only to find the right values by feel; once known, the
numbers get hardcoded into `Difficulty::` and the settings are removed, the same
path the kickup particle scale is on.

## 22. Nitrous suppression withdrawn

The nitrous hook worked exactly as intended and was removed from the build anyway.
The Run is designed around nitrous: several timed events and chase sequences are
close to unwinnable without it, so suppressing it did not make the game harder, it
made parts of it impossible. The code is preserved in `research/nos.cpp`, correct
and player-only, in case a mode ever wants it.

Worth recording as a design rule, because it is not a reverse-engineering problem
at all: a difficulty option has to leave every event completable. Removing a
mechanic the game's own encounters are balanced around fails that test even when
the implementation is perfect.

## 23. AI scalar values

Play-testing settled on `DeadlyAiSkillScale = 5.0` and `DeadlyAiGlueScale = 2.0` —
losable races that remain winnable. Both stay in the INI rather than being
hardcoded like the rest of the mode's rules: they raise the challenge rather than
soften it, so exposing them cannot be used to water the difficulty down.

The skill default matches the value mRally2's table forces as an outright cheat,
which is a reasonable sanity check that the site does what it appears to. Glue
turned out to be "higher keeps the pack on you" rather than the reverse, which was
the open question when the hooks went in.

## 24. Nitrous tuning, and the flags window at 0x0069B5BD

Nitrous suppression was withdrawn in section 22 for breaking events. Tuning it
achieves the intent without that risk, and the game makes it easy by splitting
player and AI itself:

    0x0069B5BD  cmp   byte [esp+13h], 0     the isHumanPlayer test
    0x0069B5D6  fld   dword [edi+1330h]
    0x0069B5DC  fstp  dword [esi+2A8h]      nosStrengthScalar, EVERY car
    0x0069B5E2  jz    <AI branch>
    0x0069B601  fld   dword [edi+132Ch]     PLAYER branch
    0x0069B607  fstp  dword [esi+2A4h]      nosRechargeScalar, from car tuning
    0x0069B60F  movss xmm0, [023DE148h]     AI branch: the constant 1.0
    0x0069B617  movss [esi+2A4h], xmm0      nosRechargeScalar = 1.0 for AI

The AI's recharge rate being a hardcoded constant is what makes "give the AI more
nitrous" a two-instruction change: substitute our own float for the 1.0 and leave
the store alone.

Field offsets in RaceCar::InputState, decoded from the bytes:

    [esi+0x29C]  nosRechargeOverride
    [esi+0x2A0]  nosRechargeBonus
    [esi+0x2A4]  nosRechargeScalar
    [esi+0x2A8]  nosStrengthScalar

`nosStrengthScalar` is stored for every car at 0x0069B5DC, so it is scaled inside
the player hook at 0x0069B601 instead of at its own site — that address is only
reached when the driver is human.

### The flags window

Worth recording because it constrains anything added here later. The `cmp` at
0x0069B5BD sets the flags that the `jz` at 0x0069B5E2 consumes **thirty-seven
bytes later**. That is only safe because every instruction in between is x87, and
x87 does not write EFLAGS. A cave placed anywhere in that span must preserve flags
or the human/AI branch breaks. Both hooks here sit past the `jz`, so they are
clear of it.

Both caves use x87 with matched fld/fstp pairs, so the FPU stack is balanced and
no flags are touched either way.

## 25. Time-of-day randomizer

Ported from _mRally2's TOD Randomizer table. Every event is authored at one fixed
time of day, so repeated runs down a stretch look identical; randomizing inside
what each level supports makes the game feel less rehearsed without altering play.

### The site

`sub_99BEB0` is a state-transition handler. On the transition into a level:

    0x0099BF25  mov ecx, [eax+64h]     the time-of-day preset
    0x0099BF28  push 1
    0x0099BF31  call sub_E650D0        applies it

Bytes `8B 48 64 6A 01` — five exactly, a jump with nothing left over. The level is
identified by a GUID four bytes below the object: the cheat table reads it as
`[timeofday] - 0x68`, and since `timeofday` is `eax+0x64` that resolves to
`eax-4`.

The cheat table hooks this instruction only to capture the address, then writes
from a Lua timer. This mod does the work inside the hook instead, so the value is
already randomized when the original instruction loads it — nothing has to win a
race against a ticker.

### Why a per-level table

Levels do not all implement every preset. The source table notes "NIGHT TOD IS NOT
IMPLEMENTED" against Get Outta San Francisco, and picking a preset a level cannot
render gives a broken or black scene. So each level carries its own legal set,
generated into `tod_table.h`: 128 events, values 0-8.

Conversion was checked rather than trusted. Each arm of the source's Lua
if/elseif chain declares its own `math.random(1,N)`, and every one of the 128
converted arms has exactly N presets in its set. No GUID appears twice.

A GUID absent from the table is left alone. An unknown level is not a licence to
guess, and the failure mode for guessing wrong is a scene that will not render.

Car Crusher is excluded deliberately: the source reaches it through a different
pointer chain and writes a second value for New Jersey Junkyard, which is not
replicated here.

### The cave

`pushal`/`pushfl` bracket a plain cdecl call, so the C side can do as it likes
without disturbing the game's registers or flags. The randomizer is integer-only
and touches no floating point, keeping it clear of the x87 and SSE state the
surrounding code depends on. It uses its own xorshift rather than `rand()`, since
it runs on the game's thread from inside a detour and should not share CRT state.

### Night Run (mode 2)

The source table's ForceNightVisEnv arm is far simpler than its randomizer: write
preset 4 and be done, with two exceptions.

    default                            preset 4
    Las Vegas East A (2494877324)      preset 3
    Get Outta San Francisco (2702358496)  preset 0 + a hand-built VisEnv

San Francisco has no night preset at all, so the source fakes it by forcing nine
VisEnv fields (light intensity, exposure, sky brightness, sun rotation and colour,
headlight condition) through nine separate code caves. Those are not ported, and
writing preset 0 without them would change the lighting without producing night,
so this skips that level entirely rather than half-applying the effect.

Modes 1 and 2 are alternatives rather than a scale, which matches the original —
its own notes say enabling Night Run disables the randomizer and vice versa.

### Out-of-bounds and wrong-way, forced off

The source tool's readme lists this as a known issue and so should this: some
presets swap map assets, and the OOB and wrong-way volumes are authored against
the daytime layout, so a level with a changed time of day trips them where nothing
is wrong. Both are therefore forced off whenever the time-of-day feature is on,
regardless of what [TRACK_RULES] says.

Levels that will not visibly change in either mode, inherited from the original:
Las Vegas Rival Race, Las Vegas Alley Escape and Chicago Downtown Escape have
overlapping lighting volumes that win, and the car-select cutscenes are not
covered.

### Section rename

`[DIFFICULTY]` became `[GAMEPLAY]`, since it now holds this alongside Run For Your
Life and the randomizer is not tied to any difficulty.

## 26. Audit: the stale control pointer

A log line reading

    Vehicle control changed: PlayerHasVehicleControl=176 -> restoring target sim rate

exposed a real bug that had been present since the control hook was written. The
hook captures `&[esi+4]` and that pointer is never revalidated, so once the object
behind it is freed and its memory reused, the byte reads as whatever landed there.

The consequence was not cosmetic. The test was `(*g_pHasControl != 0)`, so a
garbage nonzero byte reads as "the player is driving" and releases the sim-rate
clamp — in the middle of a cutscene, which is precisely what the clamp exists to
prevent. The FOV override gated on the same byte and would have applied the
driving FOV in menus.

The byte is a bool, so anything above 1 is proof the pointer is stale. It is now
treated as such: the last known good state is held until the hook fires again and
re-captures, and the condition warns once rather than silently acting. Consumers
read a validated accessor, `PlayerControlState()` (-1 unknown, 0, 1), instead of
dereferencing the raw pointer.

The general lesson, and the third time a variant of it has come up in this
project: a captured pointer into a game object is only valid while that object
is. If a field has a known range, check it before trusting a read.

### Other audit results

- Removed `Memory::FindPatternProcess`, `FindPatternRange` and
  `FindAllPatternsProcess`. All three were unreachable — the first two had no
  callers and the third was only reached through them.
- `PatchUtil::VerifiedNop` had no callers either. Rather than delete it, the two
  track-rule patches whose signatures were captured from a live run
  (`D9 41 68` for the OOB reset, `D9 86 24 29 00 00` for wrong-way) were promoted
  from `CaptureNop` to `VerifiedNop`, so they now refuse on a mismatched build
  instead of NOPing whatever is there. The checkpoint-timer sites have not been
  observed yet and still use `CaptureNop`.
- `Difficulty::GetCurrent` was exported but used only inside its own file; it is
  now file-local, leaving `RunForYourLifeActive()` as the module's public surface.
- `build.bat` did not pass `-Wall -Wextra`, so warnings were never being seen. It
  does now, and the tree is clean under them.

## 27. Where the risky-driving nitrous reward actually lives

Two wrong guesses and one live measurement.

The obvious candidate was `requestNosBoostAmount`, since
`CollectPowerTrainDynamicState` adds it straight to the bar:

    remainingNOSCapacity += requestNosBoostAmount * NOSstageCapacity

It is hooked at 0x0069AB40 and works, but a full run of near misses produced no
awards at all. That path carries scripted grants only — the same shape as the
`PlayerActionEntityData.NosBoostAmount = 1` seen in the EBX.

The answer came from watching the four recharge inputs on the player's own
InputState each tick instead of reading more code:

    NOS fields: override=0.0000  bonus=3.0000  scalar=1.0000  strength=1.0000
    NOS fields: override=0.0000  bonus=0.0000  scalar=1.0000  strength=1.0000

`nosRechargeBonus` at [esi+0x2A0] pulses to 3.0 and back to 0, in bursts matching
near misses. `nosRechargeOverride` is unused, staying at 0 throughout.

### Scaling it

The bonus is stored at 0x0069B5FB, inside the `if` that computes it. No new hook
was needed: the `jz` at 0x0069B5EB jumps to exactly 0x0069B601, which is already
the player hook's address, so that hook is the join point of the branch. It runs
whether or not a reward was paid, and scaling a zero is still zero.

### The formula

Dropping `nosRechargeScalar` to 0 killed reward nitrous as well as the passive
fill, which means the recharge is `(base + bonus) * scalar` rather than
`base * scalar + bonus`. Turning the scalar down therefore starves the rewards
too, and the way to get "low trickle, full reward" is to raise the bonus to
compensate: scalar 0.1 with bonus 10 leaves passive accumulation at a tenth while
a near miss pays roughly what it always did.

### Confirmed in play

`DeadlyPlayerNosRechargeScale = 0.1` with `DeadlyPlayerNosBonusScale = 10.0` gives
the intended result: the bar barely moves on its own and fills quickly on risky
driving. The field watch shows the multiply landing linearly with no clamp
downstream — `bonus` reads 30.0 where the game pays 3.0. Both are now the shipped
defaults.

## 28. The takedown cinematic, and why the camera is not the real target

When the player wrecks a cop or mob car the game plays a slow-motion cinematic and
drives the player's car during it, badly, which often ends the run on the corner
that follows. The data explains both halves at once.

`_c4/cameras/Core_Drive/crashes/coptakeout_treatment_prefab.xml` contains:

    AIControlSetterEntityData      Enabled = True
    CinebotStateLogicEntityData    Mode = CopTakeOut_ShotA_mode
    CinebotStateLogicEntityData    Mode = CopTakeOut_ShotA_bumper_mode
    CinebotStateLogicEntityData    Mode = CopTakeOut_ShotA_hood_mode

The autodrive is not a side effect of the camera. `AIControlSetterEntityData`
hands the car to the AI, and it sits in the same prefab as the three camera modes,
one per view (chase, bumper, hood). Suppressing the camera alone would leave the
car driving itself with no visual cue that it is happening, which is worse than
the current behaviour rather than better.

### The camera side, if it is ever wanted

`sub_11540A0` is the CinebotStateLogicEntity event handler and is what mRally2's
"Camera Type" script hooks at exe+0xD5416A:

    result = *(a2 + 4)                     the event id
    193438506  -> sub_1178840              pop the mode
    210993314  -> push: mode  = *(data + 0x10)
                        blend = *(data + 0x14)

So `[eax+0x10]` at that site is a pointer to the CinebotModeData being pushed, and
blocking specific modes means identifying that instance. EBX GUIDs are not what the
runtime compares: the TOD randomizer's level ids are 32-bit name hashes, so the
mode object is likely keyed the same way. Finding the right value means logging the
pointer during an actual takedown rather than deriving it from the asset GUID.

### Other levers seen while looking

`exe+0x11359`, `movss xmm0,[eax+38]`, is the global world-speed scalar; mRally2's
"World Speed" script forces 0.1 there. It would remove the slow motion and nothing
else, since it is global rather than takedown-specific.

## 29. Backlog

Open items, in the order they would be worth picking up. Each one has the work
already done recorded above, so none of them start from nothing.

**Takedown autodrive** (section 28). The camera is a red herring; the target is
`AIControlSetterEntityData`. Next step is one diagnostic run: the player-branch
hook at 0x0069B601 already captures the InputState, so logging `isAIControlled`,
`requestMatchSpeed`, `inputGas` and `inputSteering` during a takedown says whether
the car is handed to the AI or fed scripted input. The fix follows from which.

**Rewind HUD counter** (sections 15, 26). Zeroing `RewindsPerDifficulty[3]` works
but the HUD reads a separate counter that has not been located.
`UIRewindDataBinding` in `_c4/UI/Flow/Screen/LoadScreens/RewindLoop.xml` binds the
displayed value to `NFSUIRaceInfoComp` with DataKey 0x43b317e6, which is the
thread to pull. Code is in `research/rewinds.cpp`.

**Redline crackle.** Quieter above 30 FPS. Ginsu diagnostics found a fourth voice
rendering at 3 calls per second against 315 for the other three, so it is starved
rather than mistuned. Not chased down.

**Nitrous HUD widget** (section 16). `_c4/UI/Assets/WidgetHudNitrous` is a bare
`UIWidgetAsset` with no visibility binding, so hiding it means intercepting the
HUD where it composes the widget.

**AI base skill ranges.** `AIPerformanceDifficultySettingsData` holds SkillRange
and GlueRange per difficulty. The mode already scales the multipliers applied to
these, so touching the base ranges is a second knob on the same thing, and it
needs the EBX-instance hunt that the rewind array failed at.

**Chase camera position and pitch.** `CinebotCamera::commitShot` was identified as
the place to look and never followed up. `ForceRenderShiftY` covers part of the
same ground from the render settings side.

**GlobalPostProcessSettings.** Resolvable through `getContainer`, never tested.

## 30. Photo mode in the pause menu

Photo mode is hidden because its visibility binding depends on Autolog, which EA
shut down. `EnableExtraUIOptions` brought it back by flipping one branch so that
no menu item is ever hidden, which drags the QA debug entries along with it.

`menu_pause.xml` gives each `UILabelTreeMenuItem` three bindings in order:
IsLocked, IsEnabled, Visibility. The visibility DataKeys separate the entries:

    ID_MENU_PHOTOMODE   0x3FF1B819   InvertValue False
    ID_MENU_DEBUG       0xE4EE8972   InvertValue True

`sub_968EF0` is the menu-item builder and opens with the visibility test:

    0x00968F43  mov ecx, [esi+3Ch]        the DataKey
    0x00968F49  call edx                  resolve it
    0x00968F4B  cmp dword [esp+38h], 2    did the resolve succeed
    0x00968F50  jne  +29h                 not resolved -> item stays visible
    0x00968F52  cmp [esi+40h], bl         InvertValue
    ...                                   -> early return, item hidden

esi is the item data for the whole block, so the key is still readable at
0x00968F4B, and that instruction is five bytes. The cave compares the key and for
photo mode only clears ZF so the following jne takes the branch the game already
uses for an unresolved binding. Every other item runs the original compare.

ZF is cleared with `test %esi, %esi` rather than a compare against a constant:
esi is the item data and cannot be null here, and a test needs no scratch
register, so nothing is clobbered. esp is untouched, so the replayed
`cmp dword [esp+38h], 2` still addresses the same slot.

The patch sits five bytes below the byte EnableExtraUIOptions writes, so the two
do not overlap and can both be enabled.

### Saving is not solved

The in-game save still expects an Autolog sign-in. Writing the shot to disk would
mean capturing the backbuffer, which needs a D3D11 present hook, a staging
texture, a CPU readback and an image encoder. This mod has no rendering hooks at
all, so that is a new subsystem rather than an addition, and every overlay the
player is likely to already have does it better. Not attempted.

## 31. Hiding the HUD for screenshots

Photo mode draws a row of control hints along the bottom, and any external capture
takes them with the shot. The widget offers no way out: `Widget_photo_mode` is a
bare `UIWidgetAsset` with no WidgetEvents, no WidgetFunctions and no visibility
binding, the same shape as `WidgetHudNitrous` in section 16.

The game's own HUD visibility byte is reachable through the chain mRally2's table
exposes as "In-Game HUD", with 1/0 hotkeys:

    [[exe+0x248B55C] + 0xC] + 0x5B8

`HudToggleKey` binds a key to flip it, defaulting to F11. The chain is re-resolved
on every press rather than cached, because the UI objects behind it are rebuilt
between screens and a pointer captured on one is not valid on the next. That is
the same failure mode that produced the garbage `176` read from the cached
vehicle-control pointer in section 26.

### Tested and withdrawn

It does not do the job, and it is not inert. In photo mode the flag turns the
RACING HUD on rather than turning the hint row off, which is the opposite of what
was wanted. Worse, the log showed every flip paired with a vehicle-control
transition:

    HUD toggle: hidden.   ->  PlayerHasVehicleControl=1
    HUD toggle: shown.    ->  PlayerHasVehicleControl=0
    HUD toggle: hidden.   ->  PlayerHasVehicleControl=1

Perfectly anti-correlated across a dozen presses. Writing that byte disturbs state
the vehicle-control check reads, which drags the sim-rate clamp with it, so this
is not something to leave in behind a default-off switch. Moved to
research/hud_toggle.cpp.

The next thing to try is the "UI Objects Hook" the same table uses at
exe+0x1089270. It captures a UI object pointer in edi and reads a float at -0xC4
which the table calls the "UI Switch", so it looks like per-widget visibility or
alpha rather than a global flag. Identifying the photo mode widget among the
objects passing through that hook is the work.

## 32. There is no global UI-disable switch

Five routes to hiding the UI were tried. All are dead, and the negative is worth
recording so nobody repeats them.

**Per-widget visibility.** `Widget_photo_mode` is a bare `UIWidgetAsset` with no
WidgetEvents, no WidgetFunctions and no visibility binding. `WidgetHudNitrous` in
section 16 is identical. Frostbite widgets in this game carry no data-driven
visibility flag to flip.

**The HUD byte** (section 31). Wrong direction and not inert. Withdrawn.

**The cheat table's "UI Objects Hook"** at exe+0x1089270. Decompiled: `sub_1489210`
is a memory-pool allocator, growing an array and handing out 516-byte blocks. edi
is `this + 4`, an array member, not a UI object. The table's "UI Switch" float at
-0xC4 is reading into the containing structure from a convenient capture point,
not a per-widget visibility value.

**The twenty settings classes** already extracted in SETTINGS_FIELDS.md, plus
`GameSettings` and `NfsGameSettings` read field by field. Every Draw* field is a
terrain, texture or streaming debug flag. `GameSettings` is generic Frostbite and
still carries Battlefield fields (soldier weapon switching, unlimited ammo, aim
assist). No HUD or UI toggle in any of them.

**The UISettings reflection class**, found by locating the class-name string
"UISettings" at 0x25770A8 and the ClassInfoData that points at it, 0x2ABE3D0, with
its field array at 0x2B15110:

    0x10  System              0x24  RootUIGraph
    0x14  Bundles             0x28  ShowPlayerProfile          bool
    0x18  ProfileOptions      0x29  OneBundlePerGraph          bool
    0x1C  Language            0x2A  ShowOnlineRegistration     bool
    0x20  MaxPendingLoadCount

Nine fields, all about bundle loading and language. Nothing about drawing. (The
field count stored in the ClassInfoData reads 53 and is wrong in the usual way:
past the ninth entry the array bleeds into a neighbouring class, giving names like
AllowVehicleOutsideCombatAreas and ContactShadowEnable. Same over-read the
extractor had to be bounded against in section 7.)

Also checked: no `HudSettings` or `UISystemSettings` class name exists in the
binary, and no `DrawUI` string. Two `HudEnable` strings exist at 0x240459A and
0x2540D13 but nothing points at either, so they are not reflection field names.

### What is left

Hooking the UI render pass and skipping it. That means finding the pass, which is
real reverse engineering rather than a flag to flip, and it would have to be a
toggle since hiding the UI permanently would take the menus with it.

## 33. Pre-release audit

Two real defects, both in the same family as everything else this log records.

### The nitrous award hook was not player-only

`nos_tuning.cpp` has three hooks. Two sit inside branches the game has already
taken on `isHumanPlayer`, so they need no check of their own. The third does not:

    0x0069AAD3  isHumanPlayer stored
    0x0069AB46  requestNosBoostAmount stored   <- the hook, for EVERY vehicle
    0x0069B5E2  the isHumanPlayer branch

So `PlayerNosBoostScale` was scaling the AI's scripted nitrous grants as well as
the player's, despite the name. Harmless at the shipped 1.0 and invisible in
testing, which is exactly why it survived. Now gated on `[esi+0x102]`, which is
already stored by that point. Clobbering EFLAGS there is safe: everything between
the return address and the next compare at 0x0069AB5A is movss and mov.

That is the fourth time a field that looked player-specific turned out to be
written for AI cars too. The rule stands: before scaling or suppressing anything
in `collectRaceCarInputState`, find out whether the AI reads it.

### Run For Your Life stood aside for [TRAFFIC]

`UpdateTrafficControls` opened with `if (g_Config.EnableTrafficControls) return;`,
so with the traffic section enabled the mode's density ceiling never applied. A
player could set `TrafficMaxDensity = 0.05` and soften the difficulty, which is
the one thing the fixed values exist to prevent. The mode now wins, and hands the
ceiling back to the INI when it disengages.

### Everything else came back clean

Every Init/Update declared, defined and called. build.bat and CMakeLists source
lists identical. research/ excluded. INI and config.cpp key parity exact in both
directions, and every ConfigStruct field is read from the INI. All twelve hook
sites verify their bytes before patching. All eleven caves reproduce the
instructions they replaced and keep the x87 stack balanced. No unreferenced
helpers, no leftover markers, no unguarded logging in the ticker. Warning-free
under -Wall -Wextra, which build.bat now passes.

## 34. m_health reading zero is not a bad pointer

A release-candidate run produced:

    Vehicle health: the pointer chain has resolved to 0x... for 10 seconds but
    m_health reads 0.000000, which is not a health value. The chain is wrong.

The chain was fine. Zero is a real reading: a wrecked car sits at zero, and so
does one between events before the next is initialised. The timing in the log says
exactly that, with vehicle control dropping ten seconds earlier as the event ended
and returning one second after the warning as the next one started.

The range check tested `current > 0.0f`, so zero fell into the same bucket as the
genuine garbage that motivated the guard (56659397312512.0 from a stale pointer
during a level load). Zero is now skipped quietly and does not count toward the
rejection counter. Everything else outside the plausible range still warns, NaN
included, since a comparison against NaN is false.

Worth keeping in mind when writing this kind of guard: a plausibility check has to
distinguish "implausible" from "unremarkable". Treating a legitimate boundary value
as corruption produces a message that tells the user their setup is broken when it
is working.

## 35. Release values

Settled by playing, and fixed in Difficulty:: rather than the INI:

    health cap                50      a ceiling, damage still accumulates
    AI skill scale            1.12
    AI glue scale             0.95
    AI nitrous recharge       2.00    absolute; the game hardcodes 1.0 for AI
    your nitrous recharge     0.10
    your risky-driving bonus  10.0
    your nitrous strength     1.12
    traffic max density       0.25    the ceiling only

Recharge and bonus are a pair. The recharge is (base + bonus) x scalar, so the
0.10 and the 10.0 cancel out for earned nitrous while passive accumulation drops
to a tenth. Changing one without the other silently starves or floods the rewards.

Nitrous strength is the only value the mode raises in the player's favour, added
late to offset how scarce the bar becomes. Every other player-side change is a
removal, and every removal is player-only: AI cars keep their drafting, their
assists and their nitrous, and get twice the recharge.

PlayerNosBoostScale is the one nitrous setting the mode does not touch, because it
carries scripted grants rather than earned rewards.

## 36. Traffic max density: a multiplier, and why the memory write had to go

A fixed ceiling of 0.25 was wrong in play. Events are authored with different
densities, and forcing all of them to one number made sparse stretches as
congested as city ones. Traffic that thick removes the speed the game is about.
A multiplier keeps each event's own character.

Switching to a multiplier forced a change in how the hook works, for a reason that
is easy to miss. `sub_125EEE0` reads `[eax+0x1C]` TWICE and uses it for two
different things:

    0x0125EEE9  movss xmm2,[eax+1Ch]    a term:  density = scale * maxDensity
    0x0125EFDE  movss xmm2,[eax+1Ch]    the hard ceiling the result is clamped to

    v17 = v6 * v4;                  scale * maxDensity
    ...
    if (v13 <= v11) v13 = v11;
    if (v13 <= v14) return v13; else return v14;      v14 is the second read

Both reads must agree or the calculation and the clamp disagree, which is why the
original implementation wrote the value into `[eax+0x1C]` and let both reads pick
it up.

That write is fine for an absolute override and fatal for a multiplier. The site
is hit repeatedly, so each pass would scale the already-scaled value, compounding
0.15 to 0.225 to 0.34 and upward without bound.

Both sites are the same five-byte instruction, `F3 0F 10 50 1C`, so the fix is to
hook both and scale in the register instead. Nothing is written back, the event's
authored value stays intact, and every pass starts from the same number, so the
result is idempotent by construction. The absolute path from [TRAFFIC] now works
the same way, setting the register rather than the field, which also stops it
permanently modifying loaded game data.

Worth generalising: an in-place multiply at a hook site is only safe if the site
runs once per value. Check how many times the field is read and written before
choosing between scaling memory and scaling a register.

## 37. Health logging keyed on the car, not on first sight

A release-candidate run logged the health cap once at the first event and then
went silent for the rest of the session, including after a stale-chain rejection
two minutes later. The cap was almost certainly still applying, but the log could
not show it: the confirmation line was behind a one-shot latch.

That is the wrong shape for the one thing it exists to prove. The interesting
moment is not the first car of the session, it is every car after a load, which
is exactly when a stale pointer chain would go unnoticed.

The line is now keyed on the vehicle pointer changing, so each event produces one
confirmation and nothing repeats while a car is alive. A good read also clears the
rejection latch, so a genuine failure later in a session is reported rather than
being swallowed by an earlier transient one.

The rejection itself behaved correctly in that run: the chain resolved to
0xF3F09B30 while the real car was at 0xDE7ECE60, m_health read 1.09e27, and
nothing was written.

## 38. When there is no log, there is no diagnosis

A tester reported no log file and no menu rename. Those two together are the
signature of the .asi never running, not of a hook failing: a version mismatch
still produces a log full of ABORTED lines, and the rename runs regardless of
difficulty.

It turned out to be antivirus quarantining the .asi, which it did silently. Worth
recording because a fair amount of effort went into the wrong hypotheses first.

Ruled out along the way: path length. The reported install was about 85 characters
against a 260 limit, and spaces are irrelevant to fopen.

A fallback chain writing the log to %LOCALAPPDATA% / %APPDATA% / %TEMP% was built
and then removed. It worked, but it was solving a problem the mod did not have —
scattering logs into folders nobody would think to look in, to cover a permissions
case that was never the cause. What was kept is the OutputDebugStringA mirror: if
the log file cannot be opened, lines still go to the debugger channel where
DebugView can capture them. That costs a few lines and never moves the log
somewhere surprising.

## 39. Out-of-bounds reset as a difficulty RELAXATION

Run For Your Life disables the out-of-bounds reset, which runs against everything
else the mode does. The reasoning is that the mode's other changes narrow the
player's options — a fragile car, no assists, no drafting, scarce nitrous, faster
AI — to the point where some events cannot be won on the intended racing line.
The remaining answer is a creative route, and the OOB volume exists specifically
to prevent one. Tightening every rule at once does not produce a harder game, it
produces an unwinnable one; this is the same lesson the nitrous removal taught in
§21.

Implementation differs from the rest of track_rules.cpp, which patches once at
init. The difficulty is not known until the player picks one and can change
between events, so the three bytes at +0x3FAA8C (`fld [ecx+0x68]`) are written
and restored on transition instead. Cheap because it is a plain NOP with no code
cave: verify the current bytes, swap, log. Two guards matter — if [TRACK_RULES]
or the time-of-day feature already disabled the check permanently, the mode
stands aside rather than restoring it out from under them; and if the bytes ever
read as something unexpected, the toggle disables itself instead of fighting
whatever else owns that site every time the difficulty changes.

## 40. Drafting: scaled, after being removed

Drafting was disabled outright for the player in Run For Your Life and that was
wrong, for exactly the reason the nitrous removal in section 21 was wrong. A core
mechanic that gets deleted does not raise the skill floor, it lowers the ceiling:
slipstreaming well means holding a line directly behind a car at speed, and the
mode was removing the reward for doing it rather than making it harder to earn.

It is now scaled. [esi+2B0h] is multiplied by 0.5 for the player only, so the
meter builds at half rate, while [esi+2B4h] is left alone so the slingshot pays
out in full. Twice as long in the slipstream, same reward.

The split is available because the accumulation and the payout are separate
fields, which the earlier removal work is what established: zeroing 2B0h removed
drafting entirely, so 2B0h is the input side the game integrates.

Implementation note. The old code was `movl $0, 0x2B0(%esi)`, an immediate store
needing no register. Scaling needs a second SSE register, since xmm0 already
holds the draftingSpeed about to be stored, and xmm1 may be live across this site.
The cave therefore saves xmm1 to the stack with movups, does the multiply, and
restores it. The x87 and flags constraints already documented for this cave are
unchanged, and the SSE path touches neither.

## 41. The field watch was reading AI cars

A DEADLY run with LogNosAwards on produced mostly correct samples —
scalar=0.1000, strength=1.1200, exactly the mode's 0.10 and 1.12 against a base
of 1.0 — with occasional lines reading:

    scalar=1.0000  strength=1.0000
    scalar=2.0000  strength=1.0000

Neither is reachable for the player. The player hook multiplies strength by 1.12
unconditionally, so a player sample always reads 1.12; and 2.0 is exactly
kAiNosRechargeScale. The diagnostic was sampling AI cars.

The hooks themselves were fine, and the 2.0 is what proves it: the AI branch was
getting its constant and the player branch its 0.1, separately, which is the whole
design. The fault was in WatchRechargeFields. It reads g_pPlayerInputState from
the ticker thread while the game thread is running collectRaceCarInputState for
every vehicle in turn, and the game reuses that struct, so a pointer captured in
the player branch describes whatever car was processed most recently by the time
the ticker samples it. The scalar flapping between 0.1 and 0.0 in the same log is
the same race caught mid-write.

Fixed by re-checking [esi+102h] on every sample rather than trusting the capture
site. This is worth recording because it is the fourth appearance of the same bug
class in this project, and the first where it hit the diagnostic instead of the
gameplay: a value read without asking whose car it belongs to. Here it was only
misleading. The three before it were handing the player an advantage.

## 42. The draft/nitrous coupling, settled

Section 40 left open whether halving the draft rate also halves the nitrous a
draft earns. It does, and the evidence was already in hand: when the mode
disabled drafting outright, the player reported that drafting stopped paying any
nitrous reward. Zeroing [esi+2B0h] killed the reward, so the reward is computed
from that field. Scaling it to 0.5 halves it.

Left as is. Twice as long in the slipstream for the same total nitrous is
consistent with what the draft-rate change is for, and near misses and the
oncoming lane are unaffected and still pay in full.

Worth noting how this was answered. The frame ordering argument in section 40 —
bonus stored at 0x69B5FB, drafting at 0x69B65C, so the bonus cannot read this
frame's value — was correct and useless: it ruled out the same-frame path and
said nothing about the previous frame's, which is what the game actually uses. A
negative result from play-testing settled in one sentence what static reading
could not.

## 43. Recharge and reward are one dial, not two

The recharge is (base + bonus) * scalar. Turning the scalar down to make nitrous
scarce turns the REWARD down by the same factor, which is the opposite of what
the reward scale is for. The cancellation is exact rather than approximate:

    reward contribution = bonus_raw * bonusScale * scalar

so bonusScale = 1/scalar leaves it at bonus_raw, exactly stock, for any scalar.

kPlayerNosBonusScale is therefore derived as 1.0f / kPlayerNosRechargeScale in
features.h rather than written out as a number. The old pair (0.10 and 10.0)
happened to satisfy this, which hid the relationship and made the two look like
independent taste dials — a later change to one alone would silently have moved
the payout. At 0.12 the partner is 8.333, which nobody would have picked by hand.

## 44. UnlockCutsceneFPS, scoped instead of refused

UnlockCutsceneFPS wrote 1 to g_pSimTickEnable and never cleared it. That field is
VariableSimTickEnable — not something that behaves like it, the same field — so
turning the option on put the WHOLE GAME on a variable simulation step from the
first cutscene onward. A player reported it breaking input and the camera exactly
the way the variable tick does, which is because it was the variable tick.

The first fix was to refuse the option while ClampSimRateWhenNoControl was on,
since the two contradict each other. That was wrong, or at least lazy: it treated
a scoping bug as an incompatibility and threw the feature away.

The right shape came from the player. The variable step is only destructive when
there is a car under the player's control to corrupt; during a cutscene, the
garage or the car select there is not one. So the tick is now enabled only while
the control flag reads no-control and cleared the moment it returns. The control
hook that the sim-rate clamp already installs provides the signal, so this needs
no new hook — only the discipline of writing the field both ways instead of once.

Two consequences worth stating plainly.

It necessarily overrides the clamp for the same window, because both target
no-control moments and want opposite things. Applying both would just let the
clamp win and hold cutscenes at 30.

QTEs are no-control moments too, so they run unlocked and their prompts time out
faster. This is the QTE fix being traded away, not a bug. Play-testing called
them still playable, just less forgiving, which is what makes the trade offerable
at all. It ships off.

ANSWERED. The hypothesis was that cutscenes might be held at 30 only by our own
clamp, since driving already reaches 144 with the tick off — in which case the
tick write would be redundant. It is not. Measured on the same cutscene with the
clamp off both times:

    UnlockCutsceneFPS = 0   ->   30 FPS
    UnlockCutsceneFPS = 1   ->  144 FPS

The game caps cutscenes independently of anything the mod does, and the variable
sim tick is the only lever that lifts it.

## 45. "No control" is not "nothing is being simulated"

The scoped cutscene unlock in section 44 rested on the premise that a variable sim
step can only damage a car being simulated under the player's control, so a
no-control window is safe. Play-testing broke the premise.

Head-to-head wrecks came out wrong. At a fixed 30 the car launches into a violent
mid-air tumble; with the unlock active it merely got damaged and slid along the
ground, described as looking like a 20 mph knock. The log shows why:

    14:58:02  control=1 -> driving, tick 0
    14:58:03  control=0 -> no control, tick 1
    14:58:04  control=1 -> driving, tick 0

A wreck takes control away and then simulates the crash. That is physics, in a
no-control window — exactly the case the premise said could not exist. Takedowns
and reset sequences are the same shape.

Mitigated with a dwell: the tick is only enabled once no-control has persisted for
1500 ms, and the timer restarts on every entry into the state. Crash windows are
short and flicker (1 -> 0 -> 1 inside two seconds above), cutscenes and menus run
far longer, so the wait separates them without needing to identify either.

This is a heuristic and is labelled as one in the source. The principled version
keys on whether the player's vehicle is actively being simulated rather than on
elapsed time; that needs a signal not yet located. Cost of the heuristic: a real
cutscene spends its first 1.5 s at 30 before unlocking.

The general lesson is the one this project keeps relearning in new clothes. The
control flag answers "is the player driving", and it was used to answer "is the
car being simulated". Those coincide almost always, and the exception was a
visible, memorable moment of the game.

## 47. The draft ramp needed a grace period

The ramp from section 46 worked on first test — a clean climb from x0.50 to x1.00
over two seconds of held slipstream, then holding at full. One flaw showed up in
the same log:

    game computed 0.0198, car got 0.0192 (ramp x0.97)
    game computed 0.0352, car got 0.0177 (ramp x0.50)

A single frame under the 0.02 activity threshold discarded a nearly complete
ramp. Two causes: the value flickers around the threshold at the edges of a
draft, and — reported by the player — the game zeroes drafting outright whenever
the car catches air. A bump in the road is not a driving mistake and should not
cost two seconds of holding a hard line.

Fixed with a 400 ms grace. The hold timer is only abandoned once the draft has
stayed below the threshold for that long, so flicker and small jumps are absorbed
while genuinely pulling out of the slipstream still resets — leaving and rejoining
takes far longer than 400 ms.

Worth noting the ramp is applied to a value the game has already zeroed in the
air case, so the grace does not hand back draft that was not earned. It only
decides where the ramp resumes when the slipstream comes back.
