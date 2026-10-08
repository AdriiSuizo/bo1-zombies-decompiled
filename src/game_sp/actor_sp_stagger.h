#pragma once
// mod: euphoria (mods/euphoria) - NOT part of the original game. A procedural "drunk" stagger for actors, driven by
// one script field per actor (self.drunk, 0 = retail, 1 = full). The animations are untouched: the actor keeps its own
// walk / run, and the engine bends what the animation produces, the way a drunk walks in GTA IV:
//   - heading wander: the path direction the run animation is applied along turns left and right with slow noise;
//   - stumbles: every ~1-4 s a sideways lurch (a push across the path), a yaw kick, a lean into it, a dip forward and
//     a short stride, then a smaller counter-lean (the catch step); one in four is a near-fall;
//   - body sway: the whole model pitches and rolls a few degrees around its feet (the entity angles, pitch and roll
//     that retail keeps at 0 for actors; they are networked and the client renders them).
// Server only: everything is in the actor's origin and angles, so hit detection and the client see the same thing.
// Only while the actor's own code moves it along a path (AI_ANIM_MOVE_CODE): scripted animations (window climbs,
// attacks, traversals) are left alone, and the lean fades out for them.

struct actor_s;

struct actor_stagger_t
{
    float amount;           // script "drunk": 0 = off (retail), 1 = full (clamped)
    int stumbles;           // script "drunkstumbles" (read-only): stumbles so far
    int seeded;             // phases and the first stumble time set
    float phase[3];         // per-actor noise phases
    int lastStepTime;       // level.time of the last update (one per actor frame)
    float yawOffset;        // smoothed heading offset, degrees (applied to the move direction)
    float roll;             // smoothed body roll, degrees (entity angles[2])
    float pitch;            // smoothed body pitch, degrees (entity angles[0])
    float strideScale;      // 1 = the animation's stride, lower during a stumble
    float pushDelta;        // sideways move this frame, units (signed: + = the actor's right)
    int nextStumbleTime;    // level.time of the next stumble
    int stumbleStartTime;   // the current / last stumble
    int stumbleEndTime;
    float stumbleSide;      // -1 left, +1 right
    float stumblePush;      // units/s sideways at the peak
    float stumbleYaw;       // degrees of heading kick at the peak (signed)
    float stumbleTilt;      // degrees of roll at the peak (signed)
    float stumbleDip;       // degrees of pitch (forward) at the peak
    float stumbleStride;    // stride scale at the peak (0.3 = a near-fall)
};

void Actor_Stagger_RegisterDvars();
// Path_UpdateMovementDelta, before the wish delta is built: turns the move direction (dir, 3D, and the 2D look
// direction that the body faces) by the heading offset and shortens the stride during a stumble.
void Actor_Stagger_Move(actor_s *self, float *dir, float *lookDir, float *moveDist);
// Path_UpdateMovementDelta, after the wish delta is built: the sideways lurch of a stumble.
void Actor_Stagger_Push(actor_s *self);
// Actor_UpdateAnglesAndDelta, after the body yaw: the pitch and roll of the sway (retail: 0).
void Actor_Stagger_Tilt(actor_s *self);
