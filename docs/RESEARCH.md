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
