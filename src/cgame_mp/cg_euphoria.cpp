// mod: euphoria - client side, see cg_euphoria.h. NOT part of the original game.
#include "cg_euphoria.h"
#include "cg_local_mp.h"
#include <euphoria/euphoria_body.h>
#include <xanim/dobj.h>
#include <xanim/dobj_utils.h>
#include <xanim/dobj_skel.h>
#include <bgame/bg_local.h>
#include <bgame/bg_weapons.h>
#include <bgame/bg_weapons_def.h>
#include <bgame/bg_misc.h>
#include <game/actor.h>
#include <clientscript/cscr_main.h>
#include <clientscript/cscr_stringlist.h>
#include <universal/q_shared.h>
#include <universal/com_math.h>
#include <universal/com_math_anglevectors.h>
#include <universal/dvar.h>
#include <qcommon/common.h>
#include <cmath>
#include <cstring>

using namespace euphoria;

static const dvar_s *bo1_mod_euphoria_client;
static const dvar_s *bo1_mod_euphoria_dist;
static const dvar_s *bo1_mod_euphoria_max;
static const dvar_s *bo1_mod_euphoria_hit_client;
static const dvar_s *bo1_mod_euphoria_hitmax_client;
static const dvar_s *bo1_mod_euphoria_bias;
static const dvar_s *bo1_mod_euphoria_muscle;
static const dvar_s *bo1_mod_euphoria_debug_client;

#define CG_EUPHORIA_MAX_BODIES 48

struct cg_euphoria_body_t
{
    int entnum;             // -1 = free
    int lastSeenTime;
    int lastStepTime;
    int bindTime;
    float blend;            // 0 animation .. 1 physics (ramps in after a bind)
    int serverState;
    int localFallenSince;
    unsigned char boneIdx[PART_COUNT];
    unsigned char tipIdx[PART_COUNT];
    bool tipValid[PART_COUNT];
    bool boneValid[PART_COUNT];
    ActiveRagdoll body;
};

static cg_euphoria_body_t g_bodies[CG_EUPHORIA_MAX_BODIES];
static bool g_bodiesInit;
static int g_unbindable[MAX_GENTITIES_SV / 32]; // entities whose skeleton lacks the needed bones (bit per entity)
static DObjAnimMat g_scratch[256];

static void CG_Euphoria_Init()
{
    if ( g_bodiesInit )
        return;
    g_bodiesInit = true;
    for ( int i = 0; i < CG_EUPHORIA_MAX_BODIES; ++i )
        g_bodies[i].entnum = -1;
    memset(g_unbindable, 0, sizeof(g_unbindable));
    bo1_mod_euphoria_client = _Dvar_RegisterBool("bo1_mod_euphoria_client", true, 0, "mod: the full-body simulation of euphoria zombies on the client (0 = the server's tilt only)");
    bo1_mod_euphoria_dist = _Dvar_RegisterFloat("bo1_mod_euphoria_dist", 1800.0f, 0.0f, 20000.0f, 0, "mod: full-body euphoria only within this distance of the camera (units)");
    bo1_mod_euphoria_max = _Dvar_RegisterInt("bo1_mod_euphoria_max", 24, 0, CG_EUPHORIA_MAX_BODIES, 0, "mod: at most this many full-body euphoria zombies at once (the rest show the server's tilt)");
    bo1_mod_euphoria_hit_client = _Dvar_RegisterFloat("bo1_mod_euphoria_hit_client", 1.0f, 0.0f, 20.0f, 0, "mod: client hit impulse on the full body, body units/s per point of weapon damage");
    bo1_mod_euphoria_hitmax_client = _Dvar_RegisterFloat("bo1_mod_euphoria_hitmax_client", 140.0f, 0.0f, 2000.0f, 0, "mod: client hit impulse cap per bullet");
    bo1_mod_euphoria_bias = _Dvar_RegisterFloat("bo1_mod_euphoria_bias", 12.0f, 0.0f, 200.0f, 0, "mod: how hard the server's balance offset pulls the client body (acceleration per unit of offset)");
    bo1_mod_euphoria_muscle = _Dvar_RegisterFloat("bo1_mod_euphoria_muscle", 1.0f, 0.05f, 5.0f, 0, "mod: motor strength of the full body (1 = tracks the animation closely, lower = floppier)");
    bo1_mod_euphoria_debug_client = _Dvar_RegisterInt("bo1_mod_euphoria_debug_client", 0, 0, 2047, 0, "mod: print this entity's client euphoria body to the console");
}

static cg_euphoria_body_t *CG_Euphoria_Find(int entnum)
{
    for ( int i = 0; i < CG_EUPHORIA_MAX_BODIES; ++i )
        if ( g_bodies[i].entnum == entnum )
            return &g_bodies[i];
    return NULL;
}

static void CG_Euphoria_Release(cg_euphoria_body_t *b)
{
    b->entnum = -1;
    b->body = ActiveRagdoll();
}

static int CG_Euphoria_Count()
{
    int n = 0;
    for ( int i = 0; i < CG_EUPHORIA_MAX_BODIES; ++i )
        if ( g_bodies[i].entnum >= 0 )
            ++n;
    return n;
}

static bool CG_Euphoria_LookupBone(const DObj *obj, const char *names, unsigned char *outIdx)
{
    char name[48];
    const char *p = names;
    while ( *p )
    {
        int n = 0;
        while ( p[n] && p[n] != '|' && n < 47 )
            ++n;
        memcpy(name, p, n);
        name[n] = 0;
        p += n;
        if ( *p == '|' )
            ++p;
        unsigned int str = SL_FindString(name, SCRIPTINSTANCE_SERVER);
        if ( str )
        {
            unsigned char idx = 254;
            if ( DObjGetBoneIndex(obj, str, &idx, -1) )
            {
                *outIdx = idx;
                return true;
            }
        }
    }
    return false;
}

// the bones the body cannot do without
static bool CG_Euphoria_PartRequired(int part)
{
    switch ( part )
    {
    case PART_PELVIS: case PART_TORSO_LOWER: case PART_TORSO_UPPER: case PART_HEAD:
    case PART_THIGH_L: case PART_SHIN_L: case PART_THIGH_R: case PART_SHIN_R:
        return true;
    default:
        return false;
    }
}

static bool CG_Euphoria_Bind(cg_euphoria_body_t *b, const DObj *obj)
{
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        b->boneValid[i] = CG_Euphoria_LookupBone(obj, kParts[i].bone, &b->boneIdx[i]);
        b->tipValid[i] = kParts[i].tipBone[0] && CG_Euphoria_LookupBone(obj, kParts[i].tipBone, &b->tipIdx[i]);
        if ( !b->boneValid[i] && CG_Euphoria_PartRequired(i) )
            return false;
    }
    return true;
}

static Quat CG_Euphoria_QuatFromAxis(float (*axis)[3])
{
    return qFromAxes(v3(axis[0][0], axis[0][1], axis[0][2]), v3(axis[1][0], axis[1][1], axis[1][2]), v3(axis[2][0], axis[2][1], axis[2][2]));
}

static Vec3 CG_Euphoria_ModelToWorld(const float *origin, float (*axis)[3], const float *t)
{
    return v3(origin[0] + t[0] * axis[0][0] + t[1] * axis[1][0] + t[2] * axis[2][0],
              origin[1] + t[0] * axis[0][1] + t[1] * axis[1][1] + t[2] * axis[2][1],
              origin[2] + t[0] * axis[0][2] + t[1] * axis[1][2] + t[2] * axis[2][2]);
}

// the animated pose of this frame, in world space, computed into a scratch skeleton so the real one stays untouched
static bool CG_Euphoria_AnimatedPose(const cpose_t *pose, const DObj *obj, cg_euphoria_body_t *b, PoseInput *out)
{
    DObj *o = (DObj *)obj;
    if ( o->numBones == 0 || o->numBones > 255 )
        return false;
    DSkel saved = o->skel;
    o->skel.mat = g_scratch;
    memset(&o->skel.partBits, 0, sizeof(o->skel.partBits));
    int all[5] = { -1, -1, -1, -1, -1 };
    DObjCalcSkel(o, all);
    o->skel = saved;

    // the animation is judged upright: only the yaw of the entity (the server tilts pitch / roll for its hit boxes)
    float anglesYaw[3] = { 0.0f, pose->angles[1], 0.0f };
    float axis[3][3];
    AnglesToAxis(anglesYaw, axis);
    Quat qPose = CG_Euphoria_QuatFromAxis(axis);
    memset(out, 0, sizeof(*out));
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        int src = b->boneValid[i] ? i : kParts[i].parent;
        if ( src < 0 )
            src = PART_PELVIS;
        const DObjAnimMat &m = g_scratch[b->boneIdx[src]];
        out->bonePos[i] = CG_Euphoria_ModelToWorld(pose->origin, axis, m.trans);
        out->boneQuat[i] = qPose * qnormalize(q4(m.quat[0], m.quat[1], m.quat[2], m.quat[3]));
        if ( b->tipValid[i] )
        {
            const DObjAnimMat &t = g_scratch[b->tipIdx[i]];
            out->tipPos[i] = CG_Euphoria_ModelToWorld(pose->origin, axis, t.trans);
            out->tipValid[i] = true;
        }
    }
    out->groundZ = pose->origin[2];
    return true;
}

static void CG_Euphoria_Write(const cpose_t *pose, const DObj *obj, int *partBits, cg_euphoria_body_t *b, const PoseInput &anim)
{
    DObjAnimMat *skel = DObjGetRotTransArray(obj);
    if ( !skel )
        return;
    float axis[3][3];
    AnglesToAxis(pose->angles, axis);
    Quat qPoseFull = CG_Euphoria_QuatFromAxis(axis);
    Quat qInv = conj(qPoseFull);
    const Output &out = b->body.output();
    float t = b->blend;
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        if ( !b->boneValid[i] )
            continue;
        int idx = b->boneIdx[i];
        Vec3 pw = lerp(anim.bonePos[i], out.bonePos[i], t);
        Quat qw = qslerp(anim.boneQuat[i], out.boneQuat[i], t);
        if ( !DObjSetSkelRotTransIndex(obj, partBits, idx) )
            continue;
        DObjAnimMat *m = &skel[idx];
        Vec3 d = pw - v3(pose->origin[0], pose->origin[1], pose->origin[2]);
        m->trans[0] = d.x * axis[0][0] + d.y * axis[0][1] + d.z * axis[0][2];
        m->trans[1] = d.x * axis[1][0] + d.y * axis[1][1] + d.z * axis[1][2];
        m->trans[2] = d.x * axis[2][0] + d.y * axis[2][1] + d.z * axis[2][2];
        Quat q = qnormalize(qInv * qw);
        m->quat[0] = q.x;
        m->quat[1] = q.y;
        m->quat[2] = q.z;
        m->quat[3] = q.w;
        float l2 = qdot(q, q);
        m->transWeight = l2 > 0.0f ? 2.0f / l2 : 2.0f;
    }
}

void CG_Euphoria_Controllers(const cpose_t *pose, const DObj *obj, int *partBits)
{
    CG_Euphoria_Init();
    if ( !bo1_mod_euphoria_client->current.enabled )
        return;
    unsigned int entnum = DObjGetEntNum(obj);
    if ( entnum >= MAX_GENTITIES_SV )
        return;
    cg_s *cg = CG_GetLocalClientGlobals(0);
    centity_s *cent = CG_GetEntity(0, entnum);
    const entityState_s *es = &cent->nextState;
    cg_euphoria_body_t *b = CG_Euphoria_Find((int)entnum);
    bool actor = es->eType == ET_ACTOR || es->eType == ET_ACTOR_CORPSE;
    bool serverOn = actor && es->animState.fLeanAmount > 0.1f && !Actor_IsDogSpecies(es->lerp.u.actor.species);
    if ( pose->isRagdoll || !serverOn )
    {
        // the retail death ragdoll took over, or the server turned it off / the actor is gone
        if ( b )
            CG_Euphoria_Release(b);
        return;
    }
    float dx = pose->origin[0] - cg->refdef.vieworg[0], dy = pose->origin[1] - cg->refdef.vieworg[1], dz = pose->origin[2] - cg->refdef.vieworg[2];
    float maxDist = bo1_mod_euphoria_dist->current.value;
    if ( dx * dx + dy * dy + dz * dz > maxDist * maxDist )
    {
        if ( b )
            CG_Euphoria_Release(b);
        return;
    }
    if ( !b )
    {
        if ( g_unbindable[entnum >> 5] & (1 << (entnum & 31)) )
            return;
        if ( CG_Euphoria_Count() >= bo1_mod_euphoria_max->current.integer )
            return;
        for ( int i = 0; i < CG_EUPHORIA_MAX_BODIES; ++i )
        {
            if ( g_bodies[i].entnum < 0 )
            {
                b = &g_bodies[i];
                break;
            }
        }
        if ( !b )
            return;
        memset(b, 0, sizeof(*b));
        b->body = ActiveRagdoll();
        b->entnum = (int)entnum;
        if ( !CG_Euphoria_Bind(b, obj) )
        {
            g_unbindable[entnum >> 5] |= 1 << (entnum & 31);
            if ( bo1_mod_euphoria_debug_client->current.integer == (int)entnum )
                Com_Printf(18, "euphoria client: ent %u has no usable skeleton (needs pelvis, spine, neck, hips, knees)\n", entnum);
            b->entnum = -1;
            return;
        }
        b->bindTime = cg->time;
        b->lastStepTime = cg->time;
        b->blend = 0.0f;
        b->serverState = 0;
        b->localFallenSince = 0;
    }
    b->lastSeenTime = cg->time;

    PoseInput anim;
    if ( !CG_Euphoria_AnimatedPose(pose, obj, b, &anim) )
        return;
    if ( !b->body.initialized() )
    {
        Params p = defaultParams();
        p.gravity = bg_gravity ? bg_gravity->current.value : 800.0f;
        p.muscleStrength = bo1_mod_euphoria_muscle->current.value;
        if ( !b->body.init(anim, p) )
        {
            // a pose the body cannot start from (e.g. the spawn's first frames): try again next frame
            return;
        }
    }
    else
    {
        Params p = b->body.params();
        p.muscleStrength = bo1_mod_euphoria_muscle->current.value;
        b->body.setParams(p);
        b->body.setTarget(anim);
    }
    int now = cg->time;
    float dt = (float)(now - b->lastStepTime) * 0.001f;
    if ( now < b->lastStepTime )
        dt = 0.0f;
    if ( dt > 0.0f )
    {
        b->lastStepTime = now;
        // the server's balance: its offset (in the entity's yaw frame) pulls the body, its state decides falls
        float yaw = pose->angles[1] * 0.017453292f;
        float lx = es->animState.fAimUpDown * 32.0f, ly = es->animState.fAimLeftRight * 32.0f;
        float ox = lx * cosf(yaw) - ly * sinf(yaw), oy = lx * sinf(yaw) + ly * cosf(yaw);
        float k = bo1_mod_euphoria_bias->current.value;
        b->body.setExternalBias(v3(ox * k, oy * k, 0.0f));
        int sstate = (int)((es->animState.fLeanAmount - 0.25f) / 0.25f + 0.5f); // 0 standing 1 stumbling 2 fallen 3 getting up
        if ( sstate == 2 )
            b->body.forceFall();
        else if ( sstate == 3 && b->body.state() == STATE_FALLEN )
            b->body.forceGetUp();
        else if ( (sstate == 0 || sstate == 1) && b->body.state() == STATE_FALLEN )
        {
            // the client went down on its own (its local impulses): the server did not, so it gets up after a moment
            if ( !b->localFallenSince )
                b->localFallenSince = now;
            else if ( now - b->localFallenSince > 700 )
                b->body.forceGetUp();
        }
        if ( b->body.state() != STATE_FALLEN )
            b->localFallenSince = 0;
        b->serverState = sstate;
        b->body.step(dt);
        b->blend += dt / 0.25f;
        if ( b->blend > 1.0f )
            b->blend = 1.0f;
        if ( bo1_mod_euphoria_debug_client->current.integer == (int)entnum )
            Com_Printf(18, "euphoria client: ent %u t %i server %i local %i stab %.2f pelvis %.1f/%.1f err %.2f stumbles %i falls %i\n",
                entnum, now, sstate, (int)b->body.state(), b->body.stability(), b->body.pelvisHeight(), b->body.standingPelvisHeight(),
                b->body.trackingError(), b->body.stumbles(), b->body.falls());
    }
    CG_Euphoria_Write(pose, obj, partBits, b, anim);
}

static float CG_Euphoria_PartScale(int part)
{
    switch ( part )
    {
    case PART_HEAD: return 0.6f;
    case PART_UPPER_ARM_L: case PART_UPPER_ARM_R: return 0.45f;
    case PART_FOREARM_L: case PART_FOREARM_R: return 0.35f;
    case PART_THIGH_L: case PART_THIGH_R: return 1.25f;
    case PART_SHIN_L: case PART_SHIN_R: return 1.35f;
    case PART_FOOT_L: case PART_FOOT_R: return 1.0f;
    default: return 1.0f;
    }
}

void CG_Euphoria_BulletHit(int localClientNum, int targetEntityNum, int weaponIndex, const float *startPos, const float *position, int damage, unsigned char boneIndex)
{
    (void)localClientNum;
    (void)boneIndex;
    if ( !g_bodiesInit || targetEntityNum < 0 || targetEntityNum >= MAX_GENTITIES_SV )
        return;
    cg_euphoria_body_t *b = CG_Euphoria_Find(targetEntityNum);
    if ( !b || !b->body.initialized() )
        return;
    Vec3 p = v3(position[0], position[1], position[2]);
    Vec3 dir = normalize(p - v3(startPos[0], startPos[1], startPos[2]));
    if ( lengthSq(dir) < 0.5f )
        return;
    // the segment the bullet struck: the nearest one to the hit point (the hit bone's part would be exact; this is close)
    int part = PART_TORSO_UPPER;
    float best = 1e30f;
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        if ( !b->boneValid[i] )
            continue;
        float d = lengthSq(b->body.segment(i).pos - p);
        if ( d < best )
        {
            best = d;
            part = i;
        }
    }
    float classScale = 1.0f;
    if ( weaponIndex > 0 && (unsigned int)weaponIndex < BG_GetNumWeapons() )
    {
        const WeaponDef *wd = BG_GetWeaponDef(weaponIndex);
        if ( wd )
        {
            switch ( wd->weapClass )
            {
            case WEAPCLASS_SPREAD: classScale = 0.55f; break;
            case WEAPCLASS_SMG: classScale = 0.8f; break;
            case WEAPCLASS_MG: classScale = 1.05f; break;
            default: break;
            }
        }
    }
    float J = (float)damage * bo1_mod_euphoria_hit_client->current.value * classScale * CG_Euphoria_PartScale(part);
    float cap = bo1_mod_euphoria_hitmax_client->current.value;
    if ( J > cap )
        J = cap;
    // a little lift so a chest hit lifts the shoulders rather than dragging the feet
    Vec3 imp = dir * J;
    imp.z += J * 0.15f;
    b->body.applyImpulse(part, p, imp);
    if ( bo1_mod_euphoria_debug_client->current.integer == targetEntityNum )
        Com_Printf(18, "euphoria client: ent %i hit part %s damage %i weapon %i -> impulse %.0f\n", targetEntityNum, kParts[part].name, damage, weaponIndex, J);
}
