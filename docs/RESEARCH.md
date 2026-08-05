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
