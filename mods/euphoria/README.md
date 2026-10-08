# euphoria - zombies with a physical body (NOT part of the original game)

A mod folder, kept apart from the base game. The zombies keep their own walk, run and attack animations and get an
**active ragdoll** on top: a body of 14 rigid segments on their own bones (pelvis, two torso segments, head, upper arms,
forearms, thighs, shins, feet), joined by ball joints with swing limits, driven by motors that pull every joint towards
the animation's pose frame by frame, with a balance controller (capture point against the support between the feet:
lean, arms out, a recovery step, a bent knee) and impulses from every bullet, melee hit and blast at the segment that
was hit. A hit knocks the torso back and the shoulders round, a leg shot buckles the knee, hits add up, and when the
balance is gone the body goes down physically and gets up again. The base game never reads this folder: without
`+set fs_mods euphoria` every map is the unchanged original.

It is inspired by GTA IV's NaturalMotion Euphoria, and it is not that middleware: the engine never had it. Everything
here is this repo's own code (`src/euphoria`, engine glue in `src/game_sp/actor_sp_euphoria.cpp` and
`src/cgame_mp/cg_euphoria.cpp`), and the sections below say what is simulated, what is approximated and what is not
verified.

## Start it

`Play-Euphoria.cmd` at the repo root (needs the build in `build\Release`, see the repo README): Kino der Toten with
physics 1. `Play-Euphoria.cmd zombie_pentagon`, `Play-Euphoria.cmd zombie_theater 1.5 0.5` (harder hits, plus the
drunk weave). Or the launcher (`tools\mod-launcher.hta`, "Euphoria zombies"), on any map, with or without horde and
nightmare. By hand, from the build folder:

```powershell
cd build\Release
$R = (Get-Location).Path
.\BO1Zombies.exe +set fs_b $R +set fs_h $R +set r_fullscreen 0 +set bo1_zombies 1 +set fs_game mods/euphoria +set fs_mods euphoria +set euphoria_physics 1 +devmap zombie_theater
```

## Dvars (set on the command line)

| dvar | default | what |
| --- | --- | --- |
| `euphoria_physics` | 1 | the active ragdoll and its hit reactions, 0 = off; above 1 = harder hits (per zombie `self.euphoria`) |
| `euphoria_level` | 0 | the first version's procedural drunk weave / sway (an approximation; `self.drunk`), 0..1 |
| `euphoria_variance` | 0.25 | each zombie's amounts are value × (1 ± variance) |
| `euphoria_specials` | 0 | 1 also applies to napalm / shrieker zombies, George, the astronaut, the thief, the dogs |
| `euphoria_selftest` | 0 | 1: the headless check (`Check-Euphoria.cmd`) |

Engine tuning, server (`src/game_sp/actor_sp_euphoria.cpp`):

| dvar | default | what |
| --- | --- | --- |
| `bo1_mod_euphoria` | 1 | 0 turns the engine part off |
| `bo1_mod_euphoria_hit` | 1.0 | body velocity (units/s) per point of damage; × hit location (legs 1.25-1.35, torso 1, head 0.6, arms 0.35-0.45) × weapon class (shotgun 0.55 per pellet, SMG 0.8, MG 1.05) |
| `bo1_mod_euphoria_hitmax` | 140 | cap per hit; one frame's hits (a shotgun blast) add up to 2.5× |
| `bo1_mod_euphoria_step` | 9 | capture point this far from the support = a stumble (a recovery step) |
| `bo1_mod_euphoria_fall` | 26 | capture point this far = a fall (mid-step 60% of it: hits during a stumble topple it) |
| `bo1_mod_euphoria_getup` | 1.2 | seconds on the ground before the 1.1 s get-up |
| `bo1_mod_euphoria_push` | 3 | how fast the actor moves under its displaced centre of mass |
| `bo1_mod_euphoria_tilt` | 75 | hit-box tilt of a fallen actor, degrees (server approximation) |
| `bo1_mod_euphoria_debug` | 0 | an entity number: its balance per frame in the console |

Client (`src/cgame_mp/cg_euphoria.cpp`):

| dvar | default | what |
| --- | --- | --- |
| `bo1_mod_euphoria_client` | 1 | 0: no full body on the client (only the server's tilt) |
| `bo1_mod_euphoria_dist` | 1800 | full body only within this distance of the camera |
| `bo1_mod_euphoria_max` | 24 | at most this many full bodies at once (the rest show the retail animation + the server's tilt) |
| `bo1_mod_euphoria_muscle` | 1 | motor strength (lower = floppier, more reaction per hit) |
| `bo1_mod_euphoria_hit_client` / `_hitmax_client` | 1 / 140 | the client's impulse per bullet (damage × class × segment) |
| `bo1_mod_euphoria_bias` | 12 | how hard the server's balance offset pulls the client body |
| `bo1_mod_euphoria_debug_client` | 0 | an entity number: its body per frame in the console |

## What is real, what is approximated, what is not verified

**Simulated (real rigid-body physics by joints), on the client, per zombie within `bo1_mod_euphoria_dist`:**
- 14 segments with mass and inertia, gravity, ball joints with swing limits, ground contact with friction
  (position-based dynamics, 120 Hz substeps; `src/euphoria/euphoria_body.cpp`).
- Motors at every joint tracking the retail animation's relative rotations (the body follows the walk / run / attack
  while physics acts on it).
- Bullet impulses at the hit point on the segment that was hit (direction from the bullet's start, magnitude from the
  weapon's damage and class), from the engine's own bullet-hit events.
- A balance controller: capture point vs the support; it leans the torso against the fall, raises the arms, bends the
  knee of the stepping leg and moves that foot under the capture point; falls when the capture point is out of reach or
  the pelvis too low; weak-muscle ragdoll on the ground; a get-up that ramps the motors and the animation's hold back.
- Accumulation: every hit spends a balance budget the animation's hold on the body scales with; it recovers in ~1.5 s.

**Authority (server), real but reduced:** one inverted pendulum per zombie (`src/euphoria/euphoria_balance.cpp`) fed by
the real damage path (`Actor_Pain`: damage, hit location, bullet direction, weapon, means of death). It decides stumbles,
falls (the actor stops moving) and get-ups, moves the actor under its displaced centre of mass, and goes to the client
in three netfields zombies never use, so the client body falls when the server says.

**Approximations, stated plainly:**
- The server has no full body: its hit boxes are the animated skeleton tilted as a whole (lean while stumbling, 75°
  towards the fall direction when down). Shots at a zombie on the ground hit that tilted volume, not the client's exact
  pose.
- Segment inertia is isotropic; no self-collision between segments; the ground is a plane at the actor's height (a
  zombie falling against a wall or down a step is not resolved against the world).
- The "ankle strategy" is an upright spring on the pelvis, not ankle torque.
- The run animation keeps playing on the server while the zombie is down; the client body is on the ground, the far-LOD
  (beyond the distance limit or past the slot limit) shows the tilted running animation.
- The client picks the hit segment as the nearest one to the bullet's impact point (not by the hit bone's name).
- Melee and blast impulses reach the server model only; the client body gets them through the server's offset bias.
- The optional drunk layer (`euphoria_level`) is the procedural weave / sway of the first version, not physics.

**Not verified here:** the whole engine integration. This repo's session was a Linux container with the source only (no
MSVC, no DirectX SDK, no game): the core was built and tested with g++ (`tests/euphoria/run.sh`, 13 checks: standing,
hits that stumble and recover, a hit that knocks down and gets up, walking that follows the animation's root, an arm
hit that moves the arm alone, a leg hit that bends the knee, hits that add up, no NaN under random hits, determinism,
~22 µs per body per frame), and the glue was written against the engine's declarations and read twice, but it has not
been compiled by MSVC nor run in the game. Treat the first Windows build and the checks below as the first real test.

## Check it

- `tests\euphoria\run.sh` (any g++ / WSL) or on Windows `cl /EHsc /O2 /I src tests\euphoria\euphoria_tests.cpp src\euphoria\euphoria_body.cpp src\euphoria\euphoria_balance.cpp`: the core, no game.
- `Check-Euphoria.cmd`: headless server (no window): the self-test script hurts zombies through the real damage path
  (three light hits, then a heavy one, on one zombie at a time, their health raised first) and logs the engine's
  notifies and fields; `euphoria PASS stumbles N falls N getups N` in `build\Release\mods\euphoria\games_mp.log`.
- The client body: play (`Play-Euphoria.cmd`), `+set bo1_mod_euphoria_debug_client <entnum>` for one zombie's body per
  frame in `console_mp.log` (its state, stability, pelvis height, tracking error, stumbles, falls), or
  `tools\headless.ps1 -Client -ShotsAtMs "60000 90000"` for back-buffer screenshots on the private desktop.

## How it works (files)

- `maps/euphoria/_euphoria.gsc`: per zombie `self.euphoria` (and `self.drunk`), crawlers off, the self-test.
- `src/euphoria/`: the engine-independent core (`euphoria_body` the full body, `euphoria_balance` the server model,
  `euphoria_math`), the piece to share with the standalone C++ / Rust controller.
- `src/game_sp/actor_sp_euphoria.cpp`: server; hooked from `Actor_Pain` (impulses) and the stagger hooks in
  `Path_UpdateMovementDelta` / `Actor_UpdateAnglesAndDelta` (push, stride, tilt); fields `euphoria`, `euphoriafalls`,
  `euphoriastate`; notifies `euphoria_stumble` / `euphoria_fall` / `euphoria_getup`.
- `src/cgame_mp/cg_euphoria.cpp`: client; hooked from `CG_Actor_DoControllers` (the animated pose is computed into a
  scratch skeleton, the body steps, its segments are written back with `DObjSetSkelRotTransIndex` as the death ragdoll
  does; bones the body does not own, fingers and toes, follow their parents) and from `CG_BulletHitEvent`. When the
  zombie dies the retail ragdoll snapshots whatever pose the body left, so the hand-over should be seamless (unverified).
