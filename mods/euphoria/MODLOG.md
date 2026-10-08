# MODLOG - euphoria (drunk, staggering zombies)

Journal of the mod, per the universal-modder loop (intake → recon → route → lab → source of truth → vertical slice →
verify → showcase → publish → field note). The field note is in the universal-modder knowledge base
(`knowledge/games/call-of-duty-black-ops-zombies/`).

## Intake (2026-10-08)
- Game: Call of Duty: Black Ops (Steam), Zombies. Idea: "Euphoria physics" for the zombies: they keep their normal
  animation but stumble around like the drunk companions in GTA IV (or the player when drunk).
- Done means: a mod folder in this repo (`mods/euphoria`), loadable with `+set fs_mods euphoria`, zombies visibly
  weaving / lurching / leaning with their own animations, a headless self-test that proves it from the log, launcher
  option, docs. A clip needs the Windows build (not available from this session).
- Online: Zombies co-op is online but this build (`BO1Zombies.exe`) runs its own listen server from a copy of the
  install (`setup.ps1`); the Steam client / BlackOps.exe / BlackOpsMP.exe are never touched or read for play. No
  anti-cheat involved.

## Recon
- `um kb search "black ops"` / "zombies" / "euphoria": nothing in the knowledge base (first note).
- The repo IS the engine: decompiled BO1 Zombies engine source (`src/`), CMake / MSVC x86, DirectX SDK June 2010,
  mods in `mods/<name>` copied next to the exe on build, loaded with `fs_game` / `fs_mods` (`mods/_stack/maps/_modstack.gsc`
  hooks: `maps/<name>/_hooks.gsc` register()). Existing mods: sandbox, horde, nightmare, noperks, zinfo, mapkit.
- Retail zombie scripts (`maps\_zombiemode_spawner`, `animscripts\zombie_*`) live in the game's fastfiles, not in the
  repo: a mod can shadow them but cannot read them here. Anim names cannot be guessed (`%anim` not in the animtree =
  compile error).
- Real Euphoria (NaturalMotion) = behaviour-driven active ragdoll. The engine's ragdoll (`src/ragdoll`) is a passive
  corpse ragdoll on the client only. A live AI is a server actor (`actor_s`) animated by XAnim with root-motion deltas.

## Route (chosen): engine + script
- Engine: a procedural stagger layer in the actor's own move code (server), driven by one actor field `self.drunk`.
  Reason: it keeps the retail animations, needs no anim names, no client code, no netcode (entity pitch / roll are
  already networked), and is a dvar-tunable ~250-line module (`src/game_sp/actor_sp_stagger.cpp`).
- Script: `mods/euphoria/maps/euphoria/_euphoria.gsc` gives every zombie its amount (and sobers crawlers), plus a
  self-test. Launcher: `tools/euphoria_flags.js` + a "Drunk zombies" select in `tools/mod-launcher.hta`.
- Rejected: script-only (no way to perturb the heading / add a lateral push from GSC without fighting the AI every
  frame; flinch anims would stop the walk); active ragdoll (weeks, and ragdolls are client-side corpses here).

## Source of truth (what the engine really does), `src/game_mp/actor_mp.cpp` unless noted
- Actors think every 50 ms (`Actor_Think` → `nextthink = level.time + 50`). Order per frame:
  `Actor_UpdateOriginAndAngles` → `Actor_UpdateAnglesAndDelta` (move delta + orientation) → `Actor_DoMove` (physics).
- In `AI_ANIM_MOVE_CODE` the stride length comes from the animation (`XAnimCalcDelta`), but the DIRECTION comes from
  the path lookahead: `Path_UpdateMovementDelta` sets `Physics.vWishDelta = fMoveDist * perp` (perp = lookaheadDir) and
  records `vLookDir` into `moveHistory`, which `Actor_FaceMotion` averages to turn the body. Turning both `perp` and
  `vLookDir` makes the zombie walk AND face off the path; the lookahead pulls it back next frames = weaving.
- Other anim modes (`USE_POS_DELTAS`, `USE_BOTH_DELTAS`...) rotate the wish delta by `fDesiredBodyYaw` in
  `Actor_DoMove` (scripted anims: window climbs, traversals). Perturbing yaw there would misalign them, so the stagger
  only runs in `MOVE_CODE` and fades its lean out otherwise.
- `Actor_SetBodyAngle` (`src/game/actor_orientation.cpp`) writes `currentAngles[0] = 0` and `[2] = 0` every frame:
  actors never pitch or roll in retail. Writing them after `Actor_UpdateBodyAngle` survives to `Actor_Think`'s copy
  into `s.lerp.apos.trBase`. All three apos components are netfields (`src/qcommon/msg_mp.cpp` lerp.apos.trBase[0..2])
  and the client builds the axis with `AnglesToAxis(cent->pose.angles)` (`cg_ents_mp.cpp`). The client's actor
  controller (`CG_Actor_DoControllers`, `cg_pose_mp.cpp`) only bends the spine by `pose.actor.pitch`, which the actor
  path never sets (it aliases the turret barrel pitch zeroed in `CG_Actor_PreControllers`), so no double pitch.
- `fLeanAmount` (`s.animState`) is networked for actors but the client only uses it for dog run blends
  (`bg_dog_animations_mp.cpp`): not usable for a human sway.
- Per-actor extra state goes in the side table `actor_sp_ext_t` (`src/game_sp/actor_sp_ext.h`): `actor_s` keeps its
  decompiled size (0x2780, strides hard-coded). Script fields on it: a row in `aifields` (`src/game/actor_fields.cpp`,
  array size is explicit: 92 → 94) with `AF_SP_EXT` + a name → offset row in `g_actorSpExtFields`.
- `actor_sp_ext_t` is zeroed on `Actor_SetDefaults` (every spawn), so `drunk` defaults to 0 = retail.

## Vertical slice (this session)
- Engine module `src/game_sp/actor_sp_stagger.{h,cpp}`: weave (two-sine noise on the move direction), stumbles
  (lateral push + yaw kick + lean + dip + short stride, half-sine envelope, 450 ms catch step, 1 in 4 a near-fall),
  sway (pitch / roll noise). Three hooks in `actor_mp.cpp` (two in `Path_UpdateMovementDelta`, one at the end of
  `Actor_UpdateAnglesAndDelta`). Dvars `bo1_mod_stagger*` registered from `Actor_SP_RegisterDvars`.
- Script: `_hooks.gsc` (both ffotd hooks, main_start runs once), `_euphoria.gsc` (watch loop, per-zombie thread,
  self-test with PASS / FAIL lines).
- NOT BUILT OR RUN HERE: this session is a Linux container without MSVC / DirectX SDK / the game. The C++ was written
  against the real declarations (every call checked in the headers) and re-read for the compiler, but the first
  Windows build may still find a typo. Next step on Windows: build, then the headless self-test (README "Check it").

## Verify (to do on Windows)
1. `cmake --build build --config Release`, then `tools\headless.ps1 -Zombies -Commands "+set fs_game mods/euphoria +set fs_mods euphoria +set euphoria_selftest 1 +devmap zombie_theater" -AutoQuitMs 150000`
   → `euphoria PASS` in `build\Release\mods\euphoria\games_mp.log`.
2. Play Kino at Drunk: zombies weave and lurch on open floor, climb windows normally, attack normally.
3. Tune: `bo1_mod_stagger_push` / `_stride` if the feet slide too much; `_weave` if they miss doors; `_rate`.
4. Clip: `um win record` 30 s on Kino's stage, `um video compile`.

## Next
- Real falls + get-ups from the retail animtree (BO1 has knock-down / get-up clips for the Thundergun; names to read
  from the fastfile's `animscripts\zombie_utility.gsc` / `_zombiemode_spawner.gsc` with the game at hand), triggered
  by the engine's near-fall through a notify.
- A "drunk" sound / grunt on the near-fall (`do_zombies_playvocals`).
- Foot IK (`src/ik`) to pin the feet during a lurch.
