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

## Session 2 (2026-10-08): active ragdoll, hit reactions, falls and get-ups
- The user's correction: no whole-model tilting as the answer; real full-body physics over the animations, reactions
  per body part to every bullet, falls and get-ups, GTA IV as the reference. Also: reuse any existing physics code,
  work locally (no push), a `.cmd` launcher, say what is verified.
- Looked for an existing Euphoria-style controller in the available repos (iw4L, LibertyRecomp, GTA-IV-RECOMP): none
  (LibertyRecomp only names a `m_physics` pointer "Euphoria/Bullet"). The core was written here so it can be the shared one.
- Engine facts that shaped the design (all read in the source):
  - the retail ragdoll (`src/ragdoll`) is a client-only passive corpse ragdoll built from `ragdoll.cfg` (14 bones, 28
    joints) on the game's physics (`src/physics`); it writes its bodies into the DObj skeleton in model space through
    `DObjSetSkelRotTransIndex` + `skel[bone].quat/trans/transWeight` in `Ragdoll_DoControllers`, run from
    `CG_DoControllers` before `DObjCalcSkel`; bones not set are computed from their parents. That is the write path the
    mod uses for a LIVE actor.
  - the animated pose of the frame is not available at controller time (controllers run before `DObjCalcAnim`), and a
    bone cannot be skel-set after its animation was calculated (`DObjSetSkelRotTransIndex` returns 0 on the anim bit). So
    the client computes the animated pose into a scratch `DSkel` (swap `obj->skel.mat`, zero part bits, `DObjCalcSkel`,
    restore): one extra skeleton calculation per euphoria zombie per frame.
  - bullet hits on actors reach the client as `EV_BULLET_HIT` with the target entity (`groundEntityNum`), the hit bone
    (`index.bone`), the weapon and the bullet start (`lerp.u.turret.gunAngles`) (`src/game/bullet.cpp`,
    `CG_BulletHitEvent`): everything a client-side impulse needs.
  - the server damage path for a surviving actor is `finishactordamage` -> `Actor_Pain(damage, point, mod, dir, hitLoc,
    weapon)`: the server impulse hook.
  - `animState.fAimUpDown / fAimLeftRight / fLeanAmount` are networked for actors and unused by zombies (the client only
    reads lean for dogs; the aim fields are the MP player path): three free floats for the balance state.
  - `DObjGetBoneIndex(obj, SL_FindString(name), &idx = 254, -1)` resolves a bone by name across the DObj's models.
- Built `src/euphoria` (engine-free): XPBD rigid segments, joints + cone limits, motors to the animation's relative
  rotations, pelvis authority, foot planting, ground plane, balance controller (lean / arms / step / knee), fall and
  get-up states, impulses, a balance budget for accumulation; and the server's capture-point pendulum. Tuning done
  against the tests (`tests/euphoria`): XPBD compliances had to be ~1e-5..1e-3 (inverse inertias 0.3-70), the
  capture point must be judged relative to the animation's root velocity or a walking body "stumbles" forever, hits
  needed a budget that weakens the animation's hold or they could never add up against it.
- Verified: the core (13 g++ tests, all passing, 22 µs per body per 60 Hz frame). NOT verified: the MSVC build and
  everything in-game (no Windows / game here). The `Check-Euphoria.cmd` self-test and the debug dvars are the oracles.
- Open: `ragdoll.cfg` from the user's install (exact radii / limits of the retail ragdoll) to replace the estimated
  segment radii and cone limits; world collision for fallen bodies; hit segment by bone name; server-side hit boxes
  from the client's pose (or a server full body) if gameplay needs exact shots at a fallen zombie; melee / blast events
  to the client.

## Session 2b (2026-10-08): the shot behaviour, GTA IV's structure
- The user uploaded GTAIV.exe / PlayGTAIV.exe / gtaEncoder.exe / playerped.rpf / BlackOps.exe. No code or data was taken
  from any of them (the rpf is encrypted, the NM behaviours are compiled code). Their STRINGS were useful:
  GTAIV.exe names the behaviours (NmRsCBUShot, BodyBalance, CatchFall, DynamicBalancer: BalanceSolve / FootPlacement /
  PelvisControl, BraceForImpact, Flinch, HeadLook, Pedal, ArmsWindmill, BodyWrithe, HighFall, RollDownStairs...) and
  their parameters (addShockSpin, shockSpinMin/Max, spinePainMultiplier, reachForWound, timeBeforeReachForWound,
  timeBeforeCollapseWoundLeg, stiffnessDecayTarget/Time, armsOutOnPush, useArmToSlowDown...). BlackOps.exe confirms the
  rig's bone names (j_mainroot is the pelvis-level root, no "pelvis" bone) and `exec ragdoll.cfg`.
- Implemented in the core with those names: shock spin, spine pain + twist, reach for wound, wounded-leg collapse,
  head look, flinch, shot relax, catch fall. Five new tests (18 in all), all passing.
- Bugs found by the tests on the way (worth remembering):
  - bone-local axes: offsets like "rotate the arm about the bone's y" are wrong on a rig whose bones point down their
    x towards the child; every behaviour axis now comes from world geometry (forward / left from the hips' positions);
  - the head look used the head bone's x as its facing: near-vertical, so the yaw was noise and the head spun at 20 rad/s;
  - a bullet's lever arm on a light segment gave ~100 rad/s of spin: per-segment caps, with the remainder of the
    momentum handed to the whole body (a 320 hit must still knock the body down);
  - the reach pulled the arm so hard the arm's velocity swamped the capture point and the balance controller stepped
    into an oscillation: the capture point now ignores the arms, the reach is softer, and legs are not reached for;
  - the pelvis springs (authority, upright) rang without damping: damped towards the animation's root velocity.
