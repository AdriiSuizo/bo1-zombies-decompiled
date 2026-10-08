# euphoria - drunk, staggering zombies (NOT part of the original game)

A mod folder, kept apart from the base game. On any zombie map the zombies keep their own walk, run and sprint
animations but move like the drunk companions in GTA IV: they weave off their path, lurch sideways, lean into the
lurch and catch themselves with a counter-lean, and about one stumble in four is a near-fall (a long lurch, a deep
dip forward and a short stride). The base game never reads this folder: without `+set fs_mods euphoria` every map is
the unchanged original.

It is not NaturalMotion's Euphoria (a licensed behaviour-physics middleware the Black Ops engine never had). It is a
procedural layer in this repo's engine that bends what the retail animation produces, so it costs nothing per zombie
and works with every map's animations. Hit boxes, pathing and the client see the same thing, because the whole effect
is in the actor's origin and angles.

## Start it

Launcher (`tools\mod-launcher.hta`): "Drunk zombies" Off / Tipsy / Drunk / Wasted, on any map, with or without horde
and nightmare. By hand, from the build folder:

```powershell
cd build\Release
$R = (Get-Location).Path
.\BO1Zombies.exe +set fs_b $R +set fs_h $R +set r_fullscreen 0 +set bo1_zombies 1 +set fs_game mods/euphoria +set fs_mods euphoria +set euphoria_level 0.75 +devmap zombie_theater
```

Stacked with other mods: `+set fs_mods horde euphoria zinfo` (`tools/euphoria_flags.js` puts it after horde and
nightmare, before zinfo / noperks).

## Dvars (set on the command line)

| dvar | default | what |
| --- | --- | --- |
| `euphoria_level` | 0.75 | how drunk, 0..1 (0 = the mod only adds files; the launcher's Tipsy 0.4, Drunk 0.75, Wasted 1) |
| `euphoria_variance` | 0.25 | each zombie's amount is level × (1 ± variance), so a crowd staggers out of step |
| `euphoria_specials` | 0 | 1 also staggers napalm / shrieker zombies, George, the astronaut, the thief and the dogs |
| `euphoria_selftest` | 0 | 1: the headless check below |

Engine tuning (the same build, any mod that sets `self.drunk`; `src/game_sp/actor_sp_stagger.cpp`):

| dvar | default | what |
| --- | --- | --- |
| `bo1_mod_stagger` | 1 | 0 turns the engine part off (every `self.drunk` is ignored) |
| `bo1_mod_stagger_weave` | 20 | heading wander, degrees left / right of the path at drunk 1 |
| `bo1_mod_stagger_sway` | 8 | body sway, degrees of roll at drunk 1 (pitch is half) |
| `bo1_mod_stagger_rate` | 0.45 | stumbles per second at drunk 1 (less drunk = fewer) |
| `bo1_mod_stagger_push` | 120 | the sideways lurch of a stumble, units/s at its peak |
| `bo1_mod_stagger_stride` | 1 | how much a stumble shortens the stride (0 = never: no foot sliding, less of a hitch) |
| `bo1_mod_stagger_debug` | 0 | an entity number: one console line per frame with its stagger state, one per stumble |

## How it works

- `maps/euphoria/_euphoria.gsc` (hooked through `mods/_stack`, so it stacks with the other mods) polls the AI list
  four times a second and gives every new zombie `self.drunk = euphoria_level × (1 ± variance)`. Crawlers get
  `self.drunk = 0` (nothing to stumble on). Specials stay retail unless `euphoria_specials 1`.
- `self.drunk` is a new actor field (`src/game/actor_fields.cpp`, storage in the SP side table
  `src/game_sp/actor_sp_ext.h`). `self.drunkstumbles` (read-only) counts the stumbles.
- The engine (`src/game_sp/actor_sp_stagger.cpp`), only while the actor's own code moves it along a path
  (`AI_ANIM_MOVE_CODE`; scripted animations such as window climbs, attacks and traversals are left alone):
  - `Path_UpdateMovementDelta` (`src/game_mp/actor_mp.cpp`): the path direction the animation's stride is applied
    along is turned by a slow two-sine noise (the weave), and the direction the body faces with it, so the zombie
    veers off and the path lookahead keeps pulling it back; during a stumble the stride is shortened and a sideways
    push (across the path, towards the stumble side, a half-sine envelope over 300-900 ms) is added to the wish delta.
    Collision stays the actor's own (`Actor_PhysicsAndDodge`), so a lurch into a wall slides along it.
  - `Actor_UpdateAnglesAndDelta`: after the retail body yaw (`Actor_SetBodyAngle` zeroes pitch and roll every frame)
    the sway and the stumble's lean / dip are written to the entity's pitch and roll. Both are networked
    (`lerp.apos.trBase[0]` and `[2]`) and the client builds the model axis from all three angles
    (`CG_CalcEntityLerpPositions` → `AnglesToAxis(cent->pose.angles)`), so no client code was needed.
  - A stumble: side ±1, 300-550 ms (near-fall 600-900 ms), a heading kick of 25-55°, a lean of 12-22° into the side,
    a dip of 6-14° (near-fall 18-28°), stride 0.5-0.8 (near-fall 0.3), then 450 ms of counter-lean. The next one
    comes 0.5-1.5 / (rate × drunk) seconds later.
- Everything is smoothed per 50 ms actor frame (factor 0.3), so the client's interpolation of the angles stays clean.

## Check it (headless, no window)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\headless.ps1 -Zombies -Commands "+set fs_game mods/euphoria +set fs_mods euphoria +set euphoria_selftest 1 +devmap zombie_theater" -AutoQuitMs 150000
Select-String "euphoria" build\Release\mods\euphoria\games_mp.log
```

Every 5 s a line `euphoria: t <ms> alive N drunk N max roll <deg> max pitch <deg> stumbles N`; `euphoria PASS ...` once
a zombie has stumbled and leaned more than 2°, `euphoria FAIL ...` after 180 s without. For one zombie's stagger per
frame in console_mp.log: `+set bo1_mod_stagger_debug <entnum>` (the entity numbers are in `games_mp.log`'s spawn lines
and in `ai_debugAnimDeltas`-style tooling).

## Known limits

- The feet slide during a lurch and when the stride is shortened: the run animation does not know about the push.
  `bo1_mod_stagger_stride 0` keeps the stride, `bo1_mod_stagger_push` lower keeps the feet closer to the animation.
- No actual falls: a zombie never goes down and gets up (the retail zombie animtree's knock-down / get-up clips were not
  verified by name from here, and a `%anim` that is not in the tree fails the script compile). The near-fall is the
  deep dip + short stride. See MODLOG.md "Next".
- A zombie mid-lurch that is also dodging another zombie can take the lurch's push on top of the dodge; it looked fine
  in the design but is not measured.
