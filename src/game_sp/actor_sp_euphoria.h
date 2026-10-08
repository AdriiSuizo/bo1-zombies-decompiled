#pragma once
// mod: euphoria (mods/euphoria) - SERVER side of the active-ragdoll zombies. NOT part of the original game.
//
// The server is the authority on balance: per actor it runs the reduced balance model (src/euphoria/euphoria_balance.h,
// a capture-point inverted pendulum), fed by the real damage path (Actor_Pain: damage, hit location, bullet direction,
// weapon class, means of death). Its outputs drive the actor's own movement (a sideways push and a shorter stride
// through the stagger hooks in actor_sp_stagger.cpp), stop it while it is down, tilt its entity for the hit boxes
// (an approximation: the server has no full body), notify the script on falls and get-ups, and go to the client in
// three netfields zombies never use (animState.fLeanAmount = state, fAimUpDown / fAimLeftRight = the capture point
// offset in the entity's yaw frame), where the full-body simulation (cgame_mp/cg_euphoria.cpp) follows them.
// Per actor it is switched on by the script field self.euphoria (0 = retail; 1 = on, also scales the hit impulses).
#include <euphoria/euphoria_balance.h>

struct actor_s;

struct actor_euphoria_sv_t
{
    bool active;
    int lastThinkTime;
    int lastState;
    float frameImpulse;     // impulse applied this frame (a shotgun's pellets add up to a cap)
    int frameImpulseTime;
    // outputs of the last think, for the move hooks
    float strideScale;      // 1 = the animation's stride
    float pushDelta[2];     // world xy units to add to the wish delta this frame
    float tiltPitch;        // degrees, entity angles (hit boxes / far LOD)
    float tiltRoll;
    float tiltBlend;        // the fall tilt ramps in and out
};

void Actor_Euphoria_RegisterDvars();
void Actor_Euphoria_LoadConsts();
// the real damage path (Actor_Pain): an impulse on the balance model
void Actor_Euphoria_OnPain(actor_s *self, int damage, const float *point, const float *dir, int hitLoc, int weaponIdx, int mod);
// once per actor frame (the first caller steps the model): NULL when the actor is retail
const actor_euphoria_sv_t *Actor_Euphoria_Think(actor_s *self);
