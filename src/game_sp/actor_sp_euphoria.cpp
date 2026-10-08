// mod: euphoria - server side, see actor_sp_euphoria.h. NOT part of the original game.
#include "actor_sp_euphoria.h"
#include "actor_sp_ext.h"
#include <game_mp/actor_mp.h>
#include <game_mp/g_main_mp.h>
#include <game_mp/g_utils_mp.h>
#include <game_mp/g_spawn_mp.h>
#include <game_mp/g_scr_main_mp.h>
#include <game_mp/g_combat_mp.h>
#include <bgame/bg_misc.h>
#include <bgame/bg_weapons.h>
#include <bgame/bg_weapons_def.h>
#include <clientscript/cscr_stringlist.h>
#include <qcommon/common.h>
#include <universal/dvar.h>
#include <universal/com_math.h>
#include <cmath>
#include <cstring>

static const dvar_s *bo1_mod_euphoria;
static const dvar_s *bo1_mod_euphoria_hit;
static const dvar_s *bo1_mod_euphoria_hitmax;
static const dvar_s *bo1_mod_euphoria_push;
static const dvar_s *bo1_mod_euphoria_tilt;
static const dvar_s *bo1_mod_euphoria_step;
static const dvar_s *bo1_mod_euphoria_fall;
static const dvar_s *bo1_mod_euphoria_getup;
static const dvar_s *bo1_mod_euphoria_debug;

static unsigned __int16 g_euphoriaFallString;
static unsigned __int16 g_euphoriaGetupString;
static unsigned __int16 g_euphoriaStumbleString;

static euphoria::BalanceModel g_balance[MAX_ACTORS_CAP];
static actor_euphoria_sv_t g_sv[MAX_ACTORS_CAP];

void Actor_Euphoria_RegisterDvars()
{
    bo1_mod_euphoria = _Dvar_RegisterBool("bo1_mod_euphoria", true, 0, "mod: the active-ragdoll balance of actors with self.euphoria > 0 (mods/euphoria)");
    bo1_mod_euphoria_hit = _Dvar_RegisterFloat("bo1_mod_euphoria_hit", 1.0f, 0.0f, 20.0f, 0, "mod: euphoria hit impulse, units/s of body velocity per point of damage");
    bo1_mod_euphoria_hitmax = _Dvar_RegisterFloat("bo1_mod_euphoria_hitmax", 140.0f, 0.0f, 2000.0f, 0, "mod: euphoria hit impulse cap per hit (units/s); a shotgun blast adds its pellets up to 2.5x this");
    bo1_mod_euphoria_push = _Dvar_RegisterFloat("bo1_mod_euphoria_push", 3.0f, 0.0f, 30.0f, 0, "mod: how fast the actor moves under its displaced centre of mass (fraction of the offset per second)");
    bo1_mod_euphoria_tilt = _Dvar_RegisterFloat("bo1_mod_euphoria_tilt", 75.0f, 0.0f, 90.0f, 0, "mod: server hit-box tilt of a fallen actor, degrees (an approximation: the server has no full body)");
    bo1_mod_euphoria_step = _Dvar_RegisterFloat("bo1_mod_euphoria_step", 9.0f, 1.0f, 60.0f, 0, "mod: capture point beyond this many units from the support = a stumble (recovery step)");
    bo1_mod_euphoria_fall = _Dvar_RegisterFloat("bo1_mod_euphoria_fall", 26.0f, 2.0f, 120.0f, 0, "mod: capture point beyond this many units = a fall");
    bo1_mod_euphoria_getup = _Dvar_RegisterFloat("bo1_mod_euphoria_getup", 1.2f, 0.0f, 10.0f, 0, "mod: seconds on the ground before getting up (the get-up itself takes 1.1 s)");
    bo1_mod_euphoria_debug = _Dvar_RegisterInt("bo1_mod_euphoria_debug", 0, 0, 2047, 0, "mod: print this entity's euphoria balance to the console");
    memset(g_sv, 0, sizeof(g_sv));
}

void Actor_Euphoria_LoadConsts()
{
    g_euphoriaFallString = (unsigned __int16)GScr_AllocString("euphoria_fall");
    g_euphoriaGetupString = (unsigned __int16)GScr_AllocString("euphoria_getup");
    g_euphoriaStumbleString = (unsigned __int16)GScr_AllocString("euphoria_stumble");
}

static bool Euphoria_Debug(const actor_s *self)
{
    return bo1_mod_euphoria_debug && bo1_mod_euphoria_debug->current.integer == self->ent->s.number;
}

static euphoria::BalanceParams Euphoria_Params()
{
    euphoria::BalanceParams p = euphoria::defaultBalanceParams();
    p.gravity = bg_gravity ? bg_gravity->current.value : 800.0f;
    p.supportRadius = bo1_mod_euphoria_step->current.value;
    p.stepRadius = bo1_mod_euphoria_fall->current.value;
    if ( p.stepRadius < p.supportRadius + 1.0f )
        p.stepRadius = p.supportRadius + 1.0f;
    p.getupDelay = bo1_mod_euphoria_getup->current.value;
    return p;
}

static float Euphoria_HitLocScale(int hitLoc)
{
    switch ( hitLoc )
    {
    case HITLOC_HELMET: case HITLOC_HEAD: return 0.6f;
    case HITLOC_NECK: return 0.7f;
    case HITLOC_TORSO_UPR: case HITLOC_TORSO_LWR: return 1.0f;
    case HITLOC_R_ARM_UPR: case HITLOC_L_ARM_UPR: return 0.45f;
    case HITLOC_R_ARM_LWR: case HITLOC_L_ARM_LWR: return 0.35f;
    case HITLOC_R_HAND: case HITLOC_L_HAND: return 0.25f;
    case HITLOC_R_LEG_UPR: case HITLOC_L_LEG_UPR: return 1.25f;
    case HITLOC_R_LEG_LWR: case HITLOC_L_LEG_LWR: return 1.35f;
    case HITLOC_R_FOOT: case HITLOC_L_FOOT: return 1.0f;
    default: return 0.6f;
    }
}

static float Euphoria_ClassScale(int weaponIdx, int mod)
{
    if ( mod == MOD_MELEE || mod == MOD_BAYONET )
        return 0.0f; // handled as a fixed shove
    if ( mod == MOD_EXPLOSIVE || mod == MOD_GRENADE_SPLASH || mod == MOD_PROJECTILE_SPLASH || mod == MOD_GRENADE || mod == MOD_PROJECTILE )
        return 0.6f;
    if ( weaponIdx <= 0 || (unsigned int)weaponIdx >= BG_GetNumWeapons() )
        return 1.0f;
    const WeaponDef *wd = BG_GetWeaponDef(weaponIdx);
    if ( !wd )
        return 1.0f;
    switch ( wd->weapClass )
    {
    case WEAPCLASS_SPREAD: return 0.55f; // per pellet; the pellets of one blast add up
    case WEAPCLASS_SMG: return 0.8f;
    case WEAPCLASS_PISTOL: return 1.0f;
    case WEAPCLASS_MG: return 1.05f;
    case WEAPCLASS_RIFLE: return 1.0f;
    default: return 1.0f;
    }
}

const actor_euphoria_sv_t *Actor_Euphoria_Think(actor_s *self)
{
    int index = G_GetActorIndex(self);
    if ( index < 0 || index >= MAX_ACTORS_CAP )
        return NULL;
    actor_sp_ext_t *ext = Actor_SP_Ext(self);
    actor_euphoria_sv_t *st = &g_sv[index];
    euphoria::BalanceModel &model = g_balance[index];
    gentity_s *ent = self->ent;
    bool on = bo1_mod_euphoria && bo1_mod_euphoria->current.enabled && ext->euphoria > 0.0f && self->Physics.bIsAlive;
    if ( !on )
    {
        if ( st->active )
        {
            memset(st, 0, sizeof(*st));
            ent->s.animState.fLeanAmount = 0.0f;
            ent->s.animState.fAimUpDown = 0.0f;
            ent->s.animState.fAimLeftRight = 0.0f;
            ext->euphoriaState = 0;
        }
        return NULL;
    }
    if ( st->active && st->lastThinkTime == level.time )
        return st;
    float dt = 0.05f;
    if ( st->active && st->lastThinkTime )
        dt = (float)(level.time - st->lastThinkTime) * 0.001f;
    if ( dt > 0.1f )
        dt = 0.1f;
    if ( dt < 0.001f )
        dt = 0.001f;
    if ( !st->active )
    {
        memset(st, 0, sizeof(*st));
        float comHeight = (ent->r.maxs[2] - ent->r.mins[2]) * 0.55f;
        model.init(Euphoria_Params(), comHeight > 10.0f ? comHeight : 38.0f);
        st->active = true;
        st->strideScale = 1.0f;
        st->lastState = euphoria::BALANCE_STANDING;
    }
    else
    {
        model.setParams(Euphoria_Params());
    }
    st->lastThinkTime = level.time;
    model.step(dt);

    // the capture point offset in the entity's yaw frame (what the client gets, and what tilts the hit boxes)
    float yaw = ent->r.currentAngles[1] * 0.017453292f;
    float fx = cosf(yaw), fy = sinf(yaw);
    euphoria::Vec3 off = model.offset();
    float localX = off.x * fx + off.y * fy;        // forward
    float localY = -off.x * fy + off.y * fx;       // left
    float stab = model.stability();
    euphoria::BalanceState state = model.state();

    // movement: the feet move under the displaced centre of mass; nothing while down
    st->pushDelta[0] = 0.0f;
    st->pushDelta[1] = 0.0f;
    if ( state == euphoria::BALANCE_STANDING || state == euphoria::BALANCE_STUMBLING )
    {
        float k = bo1_mod_euphoria_push->current.value * dt;
        st->pushDelta[0] = off.x * k;
        st->pushDelta[1] = off.y * k;
        st->strideScale = 1.0f - 0.6f * (1.0f - stab);
    }
    else if ( state == euphoria::BALANCE_GETTING_UP )
    {
        st->strideScale = 0.3f * model.getupBlend();
    }
    else
    {
        st->strideScale = 0.0f;
    }

    // hit-box tilt (server approximation): a lean towards the offset while up, a fall towards the fall direction
    float targetPitch, targetRoll;
    if ( state == euphoria::BALANCE_FALLEN || state == euphoria::BALANCE_GETTING_UP )
    {
        euphoria::Vec3 fd = model.fallDirection();
        float dlx = fd.x * fx + fd.y * fy, dly = -fd.x * fy + fd.y * fx;
        float t = bo1_mod_euphoria_tilt->current.value * (state == euphoria::BALANCE_FALLEN ? 1.0f : 1.0f - model.getupBlend());
        targetPitch = dlx * t;
        targetRoll = -dly * t;
    }
    else
    {
        float lean = 0.8f;
        targetPitch = euphoria::clampf(localX * lean, -20.0f, 20.0f);
        targetRoll = euphoria::clampf(-localY * lean, -20.0f, 20.0f);
    }
    float smooth = euphoria::clampf(dt * 8.0f, 0.0f, 1.0f);
    st->tiltPitch += (targetPitch - st->tiltPitch) * smooth;
    st->tiltRoll += (targetRoll - st->tiltRoll) * smooth;

    // to the client: state in fLeanAmount (0 = retail / off), the offset in the two aim fields (32 units = 1)
    ent->s.animState.fLeanAmount = 0.25f + 0.25f * (float)state;
    ent->s.animState.fAimUpDown = euphoria::clampf(localX / 32.0f, -1.0f, 1.0f);
    ent->s.animState.fAimLeftRight = euphoria::clampf(localY / 32.0f, -1.0f, 1.0f);

    // script: fields and notifies on transitions
    ext->euphoriaState = (int)state;
    ext->euphoriaFalls = model.falls();
    if ( (int)state != st->lastState )
    {
        if ( state == euphoria::BALANCE_FALLEN )
            Scr_Notify(ent, g_euphoriaFallString, 0);
        else if ( state == euphoria::BALANCE_STUMBLING )
            Scr_Notify(ent, g_euphoriaStumbleString, 0);
        else if ( state == euphoria::BALANCE_STANDING && st->lastState == (int)euphoria::BALANCE_GETTING_UP )
            Scr_Notify(ent, g_euphoriaGetupString, 0);
        if ( Euphoria_Debug(self) )
            Com_Printf(18, "euphoria: ent %i t %i state %i -> %i falls %i stumbles %i\n", ent->s.number, level.time, st->lastState, (int)state, model.falls(), model.stumbles());
        st->lastState = (int)state;
    }
    if ( Euphoria_Debug(self) )
        Com_Printf(18, "euphoria: ent %i t %i state %i stab %.2f off %+.1f %+.1f stride %.2f push %+.2f %+.2f tilt %+.1f %+.1f\n",
            ent->s.number, level.time, (int)state, stab, localX, localY, st->strideScale, st->pushDelta[0], st->pushDelta[1], st->tiltPitch, st->tiltRoll);
    return st;
}

void Actor_Euphoria_OnPain(actor_s *self, int damage, const float *point, const float *dir, int hitLoc, int weaponIdx, int mod)
{
    const actor_euphoria_sv_t *stc = Actor_Euphoria_Think(self);
    if ( !stc )
        return;
    int index = G_GetActorIndex(self);
    actor_euphoria_sv_t *st = &g_sv[index];
    euphoria::BalanceModel &model = g_balance[index];
    actor_sp_ext_t *ext = Actor_SP_Ext(self);
    float hitmax = bo1_mod_euphoria_hitmax->current.value;
    float dv;
    float dx, dy;
    if ( mod == MOD_MELEE || mod == MOD_BAYONET )
    {
        dv = 70.0f;
    }
    else
    {
        dv = (float)damage * bo1_mod_euphoria_hit->current.value * Euphoria_ClassScale(weaponIdx, mod) * Euphoria_HitLocScale(hitLoc);
    }
    dv *= ext->euphoria;
    if ( dv > hitmax )
        dv = hitmax;
    // a shotgun's pellets (one call each) add up, to a cap per frame
    if ( st->frameImpulseTime != level.time )
    {
        st->frameImpulseTime = level.time;
        st->frameImpulse = 0.0f;
    }
    float room = hitmax * 2.5f - st->frameImpulse;
    if ( room <= 0.0f )
        return;
    if ( dv > room )
        dv = room;
    st->frameImpulse += dv;
    // direction: the bullet's, in the ground plane; a blast pushes away from its point
    bool blast = mod == MOD_EXPLOSIVE || mod == MOD_GRENADE_SPLASH || mod == MOD_PROJECTILE_SPLASH;
    if ( blast && point )
    {
        dx = self->ent->r.currentOrigin[0] - point[0];
        dy = self->ent->r.currentOrigin[1] - point[1];
    }
    else
    {
        dx = dir ? dir[0] : 0.0f;
        dy = dir ? dir[1] : 0.0f;
    }
    float l = sqrtf(dx * dx + dy * dy);
    if ( l < 0.05f )
    {
        // from above or no direction: shove it backwards (away from where it faces)
        float yaw = self->ent->r.currentAngles[1] * 0.017453292f;
        dx = -cosf(yaw);
        dy = -sinf(yaw);
        l = 1.0f;
    }
    dx /= l;
    dy /= l;
    model.applyImpulse(euphoria::v3(dx * dv, dy * dv, 0.0f));
    if ( Euphoria_Debug(self) )
        Com_Printf(18, "euphoria: ent %i t %i hit damage %i hitloc %i mod %i weapon %i -> dv %.0f dir %+.2f %+.2f\n",
            self->ent->s.number, level.time, damage, hitLoc, mod, weaponIdx, dv, dx, dy);
}
