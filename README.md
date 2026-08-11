<p align="center">
  <img src="assets/the-run-de.png" alt="NFS The Run Definitive Edition" width="640">
</p>

# NFS The Run Definitive Edition

An ASI plugin for **Need for Speed: The Run** (PC, v1.1.0.0) that unlocks the
framerate, repairs what unlocking it breaks, and adds a difficulty worth the name.

The Run ships locked to 30 FPS. Raising the cap the usual way raises the physics
simulation rate with it, and Frostbite 2 does not survive that: the engine note
stops following the RPM, tyre spray flies off sideways, and quick-time prompts
expire before you can press anything. This mod unlocks the framerate and fixes
each of those separately, so the game still behaves the way it was tuned at 30.

It runs alongside [ThirteenAG's FusionFix](https://github.com/ThirteenAG/WidescreenFixesPack)
and duplicates none of it. Both hook the same vehicle-control check; this one
detects the existing hook, reads its jump target, and calls into it so both run.

---

## What it fixes

**Engine audio above 30 FPS.** The synth glides from the old pitch to a new one
over roughly 33 ms, and re-arms that glide every time the requested pitch changes.
A glide that just restarted has not moved yet, so past 30 FPS the changes arrive
faster than the glide can travel and the pitch never leaves the 1000 Hz it starts
at. Logging the synth showed it directly: at 144 FPS the requested pitch tracked
the RPM while the actual pitch sat frozen. The fix stops the glide re-arming.

**Tyre spray, drift smoke and dirt dust.** These effects have each particle
inherit part of the wheel's velocity. Above 30 FPS the inherited velocity comes
out too large, so spray spawns in the right place and then streaks sideways or
straight up. It repeats at the same corner every lap because the direction comes
from the wheel's motion and the ground there. The fix scales the inherited
velocity by 30/fps and touches nothing else.

**Quick-time prompts.** The countdown assumes a 30 FPS frame time, so at 144 the
prompts expire almost five times too quickly. The mod drops the simulation to 30
whenever you have no control of the car, which is exactly when QTEs and cutscenes
play, and restores your target framerate the moment you are driving.

`UnlockCutsceneFPS` trades that fix for smooth cutscenes, and is off by default.
The engine's only lever here is the variable sim tick, which is what breaks
physics, audio and input at high framerates — so rather than switching it on, the
mod enables it *only* while you have no vehicle control and clears it the instant
you do. A variable step can only corrupt a car being simulated under your
control, and during a cutscene or the car select there is not one. Driving keeps
its fixed 30 Hz step either way; QTEs are the one casualty, since they share the
no-control window.

**Minimap rendering.** On some events it comes up glitchy, invisible, or missing
road segments. Clearing the render target each frame fixes it.

## Run For Your Life

Extreme difficulty in The Run mostly means quicker AI. Pick Extreme with this on
and the mod recognises it and changes the rules: the difficulty is renamed to
**DEADLY** in the menus, your car is capped to half its health, nitrous stops
filling on its own, drafting builds at half rate, the driving assists are
stripped from your car, and the AI is scaled up. The one thing it hands back is a little more nitrous
punch when you do earn a bar.

Every one of those changes is **player-only**. AI cars keep their full
slipstream, their assists, and get twice the nitrous recharge. That distinction is the whole
design, and it is not decorative: the community cheat-table assist patches turned
out to degrade AI cars more than the player's, so switching them on made races
easier. Anything this mode takes away, it takes from you alone.

It also turns the out-of-bounds reset off, which is the one rule it relaxes
rather than tightens. With the AI quicker and your car fragile, some events are
close enough that the racing line alone will not win them, and the answer has to
be a better line — a cut corner, a crossed median, a route the event never
anticipated. The out-of-bounds volume punishes exactly that, so leaving it on
would close off the only option the mode leaves open. It comes back on every
other difficulty.

It also doubles each event's traffic density ceiling, without touching the density
the game picks per event. A fixed value was tried first and was wrong:
forcing every event to the same ceiling made sparse stretches as congested as city
ones, and traffic that thick takes the speed out of the game. Multiplying keeps
each event's own character and makes the busy ones busier.

**Drafting is slowed, not removed.** It was disabled outright first, which was
the nitrous mistake in miniature: drafting is a core mechanic, and deleting it
removes a way to show skill instead of demanding one. The meter now builds at
half rate while the slingshot it pays out is untouched, so the same reward costs
twice as long held square behind a car at speed. The *nitrous* a draft earns is
halved along with it — that reward is computed from the same field — which is
consistent enough to leave alone: twice as long in the slipstream, same total.
Near misses and the oncoming lane are unaffected. The two halves can be separated
because they are separate fields — the per-frame draft contribution the game
integrates into the meter is what zeroing removed, and the payout is elsewhere.

**Nitrous is earned, not removed.** An earlier version disabled it outright and
that was a mistake, because several timed and chase events are close to
unwinnable without it. Instead the passive refill runs at a tenth of stock while
the reward for a near miss, an oncoming pass or a draft pays what it always did.
The bar stops filling while you drive carefully and fills fast when you take
risks. The reward scale is derived as `1 / recharge` rather than tuned by hand,
because the game computes recharge as `(base + bonus) * scalar` — turning the
scalar down turns the reward down with it, and that one ratio cancels it back out
exactly. That split works because the game keeps the reward in a separate value
from the refill rate, which the field diagnostic in `docs/RESEARCH.md` pinned down
by watching it pulse to 3.0 on every near miss.

The mode's numbers are fixed in code and cannot be changed from the INI, because
a difficulty every player can soften to taste is not a difficulty. Each one has an
equivalent under `[VEHICLE]` and `[TRAFFIC]` for playing with the mode switched
off; the mode overrides those while engaged and hands them back when it is not.

A fifth difficulty is not possible from an ASI, which is worth stating plainly
since it was the first thing tried. The menu is a fixed list of four items in an
EBX asset, `RaceAIDifficulty` has exactly four values, and the AI tuning data is a
struct with four named blocks rather than an array, so a fifth value indexes
nothing. Recognising Extreme and changing what it means is the version that works.

## Time of day

Every event is authored at one fixed time of day, so the twentieth run down a
stretch of road looks like the first. `RandomizeTimeOfDay` changes that:

- `1` picks a different time each time a level loads
- `2` is Night Run, night everywhere it exists

Levels do not all implement every time of day, and asking for one that was never
built gives a broken or black scene. Mode 1 therefore carries a per-level table of
128 events converted from _mRally2's TOD Randomizer, and any level not in it is
left as the developers set it. Mode 2 skips San Francisco Escape, which has no
night preset at all.

Turning either on also disables the out-of-bounds reset and the wrong-way
respawn. Some presets swap map assets while those volumes are authored against
the daytime layout, so they fire where nothing is wrong. The tool this was ported
from does the same and lists it as a known issue.

## Everything else

| Setting | Notes |
|---|---|
| Field of view | The game runs 48, which is why the chase camera sits on the bumper. Ships at 60, and only while driving so the garage is untouched. |
| Shadow map resolution | 2048 at the game's highest preset. Ships at 4096. |
| Shadow filtering and draw distance | Filtering at 0 or 2 turns the softening off. Both are read when a level loads, so they apply from the next event. |
| Anisotropic filtering | The game ships at 4 with no menu option. Forced to 16. The engine resets it to 4 on every level load, so the mod reapplies it. |
| Driving assists | Strips the racing-line and road-alignment assists from your car only, keyed on the game's own human-player flag. Replaces thirteen cheat-table byte patches that made races easier. |
| Vehicle health | Holds health high so damage never reaches the wreck screen. A cheat, so it ships off, and Run For Your Life ignores it. |
| AI skill and rubber-banding | Two scalars the game already applies to AI performance. Being AI-side, neither can leak onto the player's car. |
| Nitrous economy | Separate scalars for your refill rate, your reward for risky driving, your boost strength, and the AI's refill rate. Recharge is `(base + bonus) x scalar`, so the first two are set as a pair. |
| Traffic density, max density and car count | |
| Viewport shift and camera roll | Conflicts with FusionFix's camera; see the INI. |
| Checkpoint timer, out-of-bounds reset, wrong-way respawn | For free roam and experimenting. |
| Photo mode | Missing from the pause menu because its visibility was tied to Autolog, which EA shut down. Restored on its own, without the debug entries. **On by default.** Saving still wants an Autolog sign-in, so use an external screenshot key. |
| QA debug menu | Unhides every hidden UI entry, photo mode included. Broader than the row above and only worth it if you want the debug menus. |

Everything above ships off unless the text says otherwise. Every setting is
documented in the INI itself, including what it costs and what it was measured
against.

## Install

Download it from **[NFSMods](https://nfsmods.xyz/mod/5373)**. This repository holds
the source; the built `.asi` is not committed to it, so cloning will not give you
one. Build it yourself with the instructions below if you would rather.

1. Install an ASI loader if you do not have one. FusionFix ships with one.
2. Copy `NFSTR_DefinitiveEdition.asi` and `NFSTR_DefinitiveEdition.ini` into the
   game's `plugins` folder.
3. Set `FPSLimit` to match your monitor. It ships at 144.

## Build

MinGW-w64 targeting 32-bit, C++17:

```
build.bat
```

or with CMake:

```
cmake -B build -A Win32
cmake --build build --config Release
```

Output is `NFSTR_DefinitiveEdition.asi`. Nothing is needed beyond the Win32 API;
the hooks are hand-written x86 assembly.

## How the patches work

Every patch checks the bytes it is about to overwrite against a known signature
first, and logs and skips on a mismatch. A wrong address produces a line in the
log instead of a crash. Addresses resolve relative to the module base, so ASLR
does not break them.

Code caves reproduce the instructions they replaced byte for byte, and each one
is checked against the disassembled output rather than trusted from source. Two
constraints came up often enough to be worth recording: one hook site sits inside
a 37-byte window where a comparison's flags must survive to a later jump, and
another straddles a live x87 value, so those caves stay off the FPU and off the
flags register entirely.

Turn on `DebugLog` and read `NFSTR_DefinitiveEdition.log` if something looks
wrong. It records every patch, every address, and the bytes that were there.

The log is written next to the `.asi`, and if that file cannot be opened at all
every line goes to the debugger output instead, where DebugView will capture it.
**No log either way means the `.asi` was never loaded**, which is a loader or
install problem rather than a mod one. Antivirus quarantining the file is the
usual cause, and it does it silently.

## Known limitations

Several engine settings exist and do nothing in the retail build, so they are not
offered: `ViewDistance`, `DrawFps` and `ForceBlurAmount` in the render settings,
and motion blur, MSAA and the shadow cascade slice count in the world render
settings. Each was wired up, tested, and removed rather than shipped as an option
that quietly fails.

The redline crackle is quieter than at 30 FPS. Diagnostics found a fourth
engine-sound voice rendering at 3 calls per second against 315 for the other
three, so it is starved rather than mistuned. Not yet chased down.

Zeroing the Extreme checkpoint-reset allowance works, but the HUD still shows a
reset count and the reset button does nothing, because the counter it reads is a
separate value that has not been located. The code is in `research/rewinds.cpp`
rather than the build.

Las Vegas Rival Race, Las Vegas Alley Escape and Chicago Downtown Escape do not
change time of day. Overlapping lighting volumes override the setting, which the
original TOD tool documents too.

## Documentation

`docs/RESEARCH.md` is the reverse engineering log: verified addresses, struct
layouts, the Frostbite settings system and how to reach it, what each fix does,
and the things that were tried and did not work. That last part is most of its
value. Two features shipped before anyone noticed they were writing to a field
the AI reads too, so a difficulty setting was quietly handing the player an
advantage in both cases. The assists were caught by play-testing, nitrous by
asking whether AI cars could still use it.

`docs/SETTINGS_FIELDS.md` lists field names, offsets and types for twenty
Frostbite settings classes, extracted from the game's own reflection data rather
than guessed. `docs/dump_fields.py` is the extractor and works on any class in
the binary.

`research/` holds code that is not in the build: the checkpoint-reset work above,
nitrous suppression from before it was replaced by the recharge economy, crash
workarounds from the community tables that turned out to scatter race AI, and an
unfinished unreleased-events feature.

## Credits

Brawltendo, for the IDA database and for the
[NFS Rivals framerate unlocker](https://github.com/Brawltendo/NFS-Rivals-Framerate-Unlocker),
which showed how the same class of bug was solved in a later Frostbite game.

_mRally2, for The Run Master Table and the TOD Randomizer. The traffic, assist,
nitrous and time-of-day addresses came from that work, along with the per-level
time-of-day table.

ThirteenAG, for FusionFix and the ASI loader.

## Compatibility

Built and tested against Need for Speed: The Run v1.1.0.0 on Windows. The
signature checks mean other versions refuse to patch rather than corrupt
themselves, but nothing else is tested.
