// mod: euphoria (mods/euphoria) - procedural stagger for actors, see actor_sp_stagger.h. NOT part of the original game.
#include "actor_sp_stagger.h"
#include "actor_sp_ext.h"
#include <game_mp/actor_mp.h>
#include <game_mp/g_main_mp.h>
#include <game_mp/g_utils_mp.h>
#include <qcommon/common.h>
#include <universal/dvar.h>
#include <universal/com_math.h>
#include <cmath>

static const dvar_s *bo1_mod_stagger;
static const dvar_s *bo1_mod_stagger_weave;
static const dvar_s *bo1_mod_stagger_sway;
static const dvar_s *bo1_mod_stagger_rate;
static const dvar_s *bo1_mod_stagger_push;
static const dvar_s *bo1_mod_stagger_stride;
static const dvar_s *bo1_mod_stagger_debug;

static const float STAGGER_TWO_PI = 6.2831855f;
static const float STAGGER_PI = 3.1415927f;
static const float STAGGER_DEG2RAD = 0.017453292f;
static const int STAGGER_RECOVER_MS = 450;  // the catch step after a stumble
static const float STAGGER_SMOOTH = 0.3f;   // per 50 ms actor frame

void Actor_Stagger_RegisterDvars()
{
    bo1_mod_stagger = _Dvar_RegisterBool("bo1_mod_stagger", true, 0, "mod: the drunk stagger of actors with self.drunk > 0 (mods/euphoria)");
    bo1_mod_stagger_weave = _Dvar_RegisterFloat("bo1_mod_stagger_weave", 20.0f, 0.0f, 90.0f, 0, "mod: stagger heading wander, degrees at drunk 1");
    bo1_mod_stagger_sway = _Dvar_RegisterFloat("bo1_mod_stagger_sway", 8.0f, 0.0f, 45.0f, 0, "mod: stagger body sway, degrees of roll at drunk 1");
    bo1_mod_stagger_rate = _Dvar_RegisterFloat("bo1_mod_stagger_rate", 0.45f, 0.01f, 5.0f, 0, "mod: stumbles per second at drunk 1");
    bo1_mod_stagger_push = _Dvar_RegisterFloat("bo1_mod_stagger_push", 120.0f, 0.0f, 600.0f, 0, "mod: sideways lurch of a stumble, units/s at drunk 1");
    bo1_mod_stagger_stride = _Dvar_RegisterFloat("bo1_mod_stagger_stride", 1.0f, 0.0f, 1.0f, 0, "mod: how much a stumble shortens the stride (0 = never, 1 = full)");
    bo1_mod_stagger_debug = _Dvar_RegisterInt("bo1_mod_stagger_debug", 0, 0, 2047, 0, "mod: print the stagger of this entity number to the console");
}

static actor_stagger_t *Stagger_Get(actor_s *self)
{
    return &Actor_SP_Ext(self)->stagger;
}

// 0 = off; otherwise the clamped amount, with the actor's noise seeded on first use
static float Stagger_Amount(actor_stagger_t *st)
{
    if ( !bo1_mod_stagger || !bo1_mod_stagger->current.enabled )
        return 0.0f;
    float a = st->amount;
    if ( !(a > 0.0f) ) // also NaN
        return 0.0f;
    if ( a > 1.0f )
        a = 1.0f;
    if ( !st->seeded )
    {
        st->seeded = 1;
        for ( int i = 0; i < 3; ++i )
            st->phase[i] = (float)G_flrand(0.0f, STAGGER_TWO_PI);
        st->nextStumbleTime = level.time + G_irand(500, 2500);
        st->strideScale = 1.0f;
        st->lastStepTime = 0;
    }
    return a;
}

static float Stagger_Noise(const actor_stagger_t *st, float t, float f1, int p1, float w1, float f2, int p2)
{
    return w1 * sinf(STAGGER_TWO_PI * f1 * t + st->phase[p1]) + (1.0f - w1) * sinf(STAGGER_TWO_PI * f2 * t + st->phase[p2]);
}

static bool Stagger_Debug(const actor_s *self)
{
    return bo1_mod_stagger_debug && bo1_mod_stagger_debug->current.integer == self->ent->s.number;
}

static void Stagger_Start(actor_s *self, actor_stagger_t *st, float amount)
{
    bool big = G_random() < 0.25f; // a near-fall
    int dur = big ? G_irand(600, 900) : G_irand(300, 550);
    st->stumbleStartTime = level.time;
    st->stumbleEndTime = level.time + dur;
    st->stumbleSide = G_random() < 0.5f ? -1.0f : 1.0f;
    st->stumblePush = (float)G_flrand(0.6f, 1.3f) * bo1_mod_stagger_push->current.value * amount * (big ? 1.4f : 1.0f);
    st->stumbleYaw = (float)G_flrand(25.0f, 55.0f) * st->stumbleSide * amount;
    st->stumbleTilt = (float)G_flrand(12.0f, 22.0f) * st->stumbleSide * amount;
    st->stumbleDip = (big ? (float)G_flrand(18.0f, 28.0f) : (float)G_flrand(6.0f, 14.0f)) * amount;
    st->stumbleStride = big ? 0.3f : (float)G_flrand(0.5f, 0.8f);
    // the mean gap is 1 / rate at drunk 1, longer when less drunk
    float rate = bo1_mod_stagger_rate->current.value * amount;
    if ( rate < 0.01f )
        rate = 0.01f;
    st->nextStumbleTime = st->stumbleEndTime + STAGGER_RECOVER_MS + (int)(1000.0f * (float)G_flrand(0.5f, 1.5f) / rate);
    st->stumbles++;
    if ( Stagger_Debug(self) )
        Com_Printf(18, "stagger: ent %i stumble %i %s side %+.0f %i ms push %.0f yaw %.0f tilt %.0f dip %.0f stride %.2f\n",
            self->ent->s.number, st->stumbles, big ? "FALL" : "lurch", st->stumbleSide, dur, st->stumblePush,
            st->stumbleYaw, st->stumbleTilt, st->stumbleDip, st->stumbleStride);
}

// One update per actor frame (50 ms): the stumble envelope, the noise and the smoothed targets.
static void Stagger_Step(actor_s *self, actor_stagger_t *st, float amount, bool moving)
{
    if ( st->lastStepTime == level.time )
        return;
    st->lastStepTime = level.time;

    if ( moving && level.time >= st->nextStumbleTime )
        Stagger_Start(self, st, amount);

    // the stumble envelope: a half sine over the stumble, then the catch step (a smaller lean the other way)
    float env = 0.0f;
    float recover = 0.0f;
    if ( st->stumbleEndTime > st->stumbleStartTime )
    {
        if ( level.time < st->stumbleEndTime )
        {
            float u = (float)(level.time - st->stumbleStartTime) / (float)(st->stumbleEndTime - st->stumbleStartTime);
            env = sinf(STAGGER_PI * u);
        }
        else if ( level.time < st->stumbleEndTime + STAGGER_RECOVER_MS )
        {
            float v = (float)(level.time - st->stumbleEndTime) / (float)STAGGER_RECOVER_MS;
            recover = sinf(STAGGER_PI * v);
        }
    }

    float t = (float)level.time * 0.001f;
    float weave = Stagger_Noise(st, t, 0.37f, 0, 0.6f, 0.83f, 1);
    float swayRoll = Stagger_Noise(st, t, 0.29f, 1, 0.7f, 1.1f, 2);
    float swayPitch = Stagger_Noise(st, t, 0.53f, 2, 0.5f, 0.17f, 0);
    float sway = bo1_mod_stagger_sway->current.value * amount * (moving ? 1.0f : 0.5f);

    float yawTarget = (moving ? weave * bo1_mod_stagger_weave->current.value * amount : 0.0f)
        + st->stumbleYaw * env - 0.5f * st->stumbleYaw * recover;
    float rollTarget = swayRoll * sway + st->stumbleTilt * env - 0.6f * st->stumbleTilt * recover;
    float pitchTarget = swayPitch * sway * 0.5f + st->stumbleDip * env - 0.3f * st->stumbleDip * recover;

    st->yawOffset += (yawTarget - st->yawOffset) * STAGGER_SMOOTH;
    st->roll += (rollTarget - st->roll) * STAGGER_SMOOTH;
    st->pitch += (pitchTarget - st->pitch) * STAGGER_SMOOTH;
    float strideTarget = 1.0f - (1.0f - st->stumbleStride) * env * bo1_mod_stagger_stride->current.value;
    st->strideScale += (strideTarget - st->strideScale) * STAGGER_SMOOTH;
    // the lurch: units this frame, across the path, towards the stumble side
    st->pushDelta = moving ? st->stumbleSide * st->stumblePush * env * 0.05f : 0.0f;

    if ( Stagger_Debug(self) )
        Com_Printf(18, "stagger: ent %i t %i moving %i env %.2f rec %.2f yaw %+.1f roll %+.1f pitch %+.1f stride %.2f push %+.1f\n",
            self->ent->s.number, level.time, moving, env, recover, st->yawOffset, st->roll, st->pitch, st->strideScale, st->pushDelta);
}

static void Stagger_Rotate2D(float *v, float degrees)
{
    float s = sinf(degrees * STAGGER_DEG2RAD);
    float c = cosf(degrees * STAGGER_DEG2RAD);
    float x = v[0];
    float y = v[1];
    v[0] = c * x - s * y;
    v[1] = s * x + c * y;
}

void Actor_Stagger_Move(actor_s *self, float *dir, float *lookDir, float *moveDist)
{
    actor_stagger_t *st = Stagger_Get(self);
    float amount = Stagger_Amount(st);
    if ( amount <= 0.0f )
        return;
    Stagger_Step(self, st, amount, true);
    Stagger_Rotate2D(dir, st->yawOffset);
    Stagger_Rotate2D(lookDir, st->yawOffset);
    *moveDist *= st->strideScale;
}

void Actor_Stagger_Push(actor_s *self)
{
    actor_stagger_t *st = Stagger_Get(self);
    if ( Stagger_Amount(st) <= 0.0f || st->pushDelta == 0.0f )
        return;
    float forward[3];
    float right[3];
    YawVectors(self->ent->r.currentAngles[1], forward, right);
    self->Physics.vWishDelta[0] += right[0] * st->pushDelta;
    self->Physics.vWishDelta[1] += right[1] * st->pushDelta;
}

void Actor_Stagger_Tilt(actor_s *self)
{
    actor_stagger_t *st = Stagger_Get(self);
    float amount = Stagger_Amount(st);
    if ( amount <= 0.0f )
        return;
    if ( self->eAnimMode != AI_ANIM_MOVE_CODE )
    {
        // a scripted animation (window climb, attack, traversal): retail angles (Actor_SetBodyAngle zeroed them),
        // the lean fades so the return to the path does not snap
        st->roll *= 0.5f;
        st->pitch *= 0.5f;
        st->yawOffset *= 0.5f;
        st->pushDelta = 0.0f;
        return;
    }
    Stagger_Step(self, st, amount, Actor_IsMoving(self) && Actor_HasPath(self));
    self->ent->r.currentAngles[0] = st->pitch;
    self->ent->r.currentAngles[2] = st->roll;
}
