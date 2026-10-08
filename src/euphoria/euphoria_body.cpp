// mod: euphoria - the active-ragdoll core, see euphoria_body.h. NOT part of the original game.
#include "euphoria_body.h"
#include <cstring>

namespace euphoria {

// Bone names are the game's (j_* of the zombie skeleton). The pelvis is j_mainroot's child j_hip? No: the zombie
// skeleton roots at j_mainroot (world root), the pelvis bone is "pelvis" on most CoD rigs; BO1's humans use j_mainroot
// as pelvis-level root with j_spinelower above it. The glue tries each name of a '|'-separated list in order.
const PartDesc kParts[PART_COUNT] =
{
    //  name            parent              bone                        tip bone           len   mass   rad  cone  muscle
    { "pelvis",       -1,                 "pelvis|j_mainroot",        "j_spinelower",    8.0f, 0.14f, 6.0f,  0.0f, 1.0f },
    { "torso_lower",  PART_PELVIS,        "j_spinelower",             "j_spine4",        8.0f, 0.14f, 6.5f, 40.0f, 1.2f },
    { "torso_upper",  PART_TORSO_LOWER,   "j_spine4|j_spineupper",    "j_neck",          9.0f, 0.16f, 7.0f, 35.0f, 1.2f },
    { "head",         PART_TORSO_UPPER,   "j_neck",                   "j_head",          4.0f, 0.07f, 4.0f, 45.0f, 0.8f },
    { "upper_arm_l",  PART_TORSO_UPPER,   "j_shoulder_le",            "j_elbow_le",     11.0f, 0.03f, 2.2f, 95.0f, 0.35f },
    { "forearm_l",    PART_UPPER_ARM_L,   "j_elbow_le",               "j_wrist_le",     10.0f, 0.02f, 1.8f, 90.0f, 0.25f },
    { "upper_arm_r",  PART_TORSO_UPPER,   "j_shoulder_ri",            "j_elbow_ri",     11.0f, 0.03f, 2.2f, 95.0f, 0.35f },
    { "forearm_r",    PART_UPPER_ARM_R,   "j_elbow_ri",               "j_wrist_ri",     10.0f, 0.02f, 1.8f, 90.0f, 0.25f },
    { "thigh_l",      PART_PELVIS,        "j_hip_le",                 "j_knee_le",      17.0f, 0.10f, 3.5f, 80.0f, 1.0f },
    { "shin_l",       PART_THIGH_L,       "j_knee_le",                "j_ankle_le",     16.0f, 0.05f, 2.5f, 90.0f, 0.9f },
    { "foot_l",       PART_SHIN_L,        "j_ankle_le",               "j_ball_le",       6.0f, 0.015f, 2.0f, 50.0f, 0.8f },
    { "thigh_r",      PART_PELVIS,        "j_hip_ri",                 "j_knee_ri",      17.0f, 0.10f, 3.5f, 80.0f, 1.0f },
    { "shin_r",       PART_THIGH_R,       "j_knee_ri",                "j_ankle_ri",     16.0f, 0.05f, 2.5f, 90.0f, 0.9f },
    { "foot_r",       PART_SHIN_R,        "j_ankle_ri",               "j_ball_ri",       6.0f, 0.015f, 2.0f, 50.0f, 0.8f },
};

Params defaultParams()
{
    Params p;
    p.gravity = 800.0f;
    p.bodyMass = 1.0f;
    p.muscleStrength = 1.0f;
    p.balanceStrength = 1.0f;
    p.authority = 1.0f;
    p.uprightStrength = 1.0f;
    p.footPlant = 1.0f;
    p.supportRadius = 9.0f;
    p.stepRadius = 26.0f;
    p.fallHeightFrac = 0.55f;
    p.fallTiltDeg = 65.0f;
    p.fallTiltTime = 0.35f;
    p.getupDelay = 1.2f;
    p.getupTime = 1.1f;
    p.groundFriction = 0.85f;
    p.linearDamping = 1.5f;
    p.angularDamping = 3.0f;
    p.substepDt = 1.0f / 120.0f;
    p.maxSubsteps = 8;
    p.fallenMuscle = 0.12f;
    p.impulseScale = 1.0f;
    return p;
}

int partForHitLocationName(const char *n)
{
    if ( !n )
        return PART_TORSO_UPPER;
    if ( !strcmp(n, "helmet") || !strcmp(n, "head") || !strcmp(n, "neck") )
        return PART_HEAD;
    if ( !strcmp(n, "torso_upper") )
        return PART_TORSO_UPPER;
    if ( !strcmp(n, "torso_lower") )
        return PART_TORSO_LOWER;
    if ( !strcmp(n, "right_arm_upper") )
        return PART_UPPER_ARM_R;
    if ( !strcmp(n, "left_arm_upper") )
        return PART_UPPER_ARM_L;
    if ( !strcmp(n, "right_arm_lower") || !strcmp(n, "right_hand") )
        return PART_FOREARM_R;
    if ( !strcmp(n, "left_arm_lower") || !strcmp(n, "left_hand") )
        return PART_FOREARM_L;
    if ( !strcmp(n, "right_leg_upper") )
        return PART_THIGH_R;
    if ( !strcmp(n, "left_leg_upper") )
        return PART_THIGH_L;
    if ( !strcmp(n, "right_leg_lower") )
        return PART_SHIN_R;
    if ( !strcmp(n, "left_leg_lower") )
        return PART_SHIN_L;
    if ( !strcmp(n, "right_foot") )
        return PART_FOOT_R;
    if ( !strcmp(n, "left_foot") )
        return PART_FOOT_L;
    return PART_TORSO_UPPER;
}

ActiveRagdoll::ActiveRagdoll()
{
    memset(this, 0, sizeof(*this));
    m_params = defaultParams();
    m_state = STATE_STANDING;
    m_stability = 1.0f;
    m_swingFoot = -1;
    m_authorityScale = 1.0f;
}

bool ActiveRagdoll::init(const PoseInput &pose, const Params &params)
{
    m_params = params;
    m_target = pose;
    m_groundZ = pose.groundZ;
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        if ( !finite3(pose.bonePos[i]) || !finiteq(pose.boneQuat[i]) )
            return false;
    }
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        const PartDesc &d = kParts[i];
        Segment &s = m_segs[i];
        Vec3 P = pose.bonePos[i];
        Quat q = qnormalize(pose.boneQuat[i]);
        Vec3 tipDir;
        float len;
        if ( pose.tipValid[i] && length(pose.tipPos[i] - P) > 0.5f )
        {
            tipDir = normalize(pose.tipPos[i] - P);
            len = length(pose.tipPos[i] - P);
        }
        else
        {
            tipDir = rotate(q, v3(1, 0, 0)); // the game's bones point down their x
            len = d.defaultLength;
        }
        s.length = len;
        s.radius = d.radius;
        s.axisLocal = rotateInv(q, tipDir);
        Vec3 com = P + tipDir * (len * 0.5f);
        s.comOffsetLocal = rotateInv(q, com - P);
        s.pos = com;
        s.q = q;
        s.vel = v3(0, 0, 0);
        s.omega = v3(0, 0, 0);
        s.prevPos = com;
        s.prevQ = q;
        s.mass = d.massFrac * params.bodyMass;
        s.invMass = 1.0f / s.mass;
        // isotropic: a solid cylinder's mean of its axial and transverse moments
        float iAxis = 0.5f * s.mass * s.radius * s.radius;
        float iPerp = s.mass * (s.radius * s.radius * 0.25f + len * len / 12.0f);
        float inertia = (iAxis + 2.0f * iPerp) / 3.0f;
        s.invInertia = inertia > 1e-6f ? 1.0f / inertia : 0.0f;
        s.onGround = false;
        s.relTarget = qIdentity();
        s.relBind = qIdentity();
        s.anchorParent = v3(0, 0, 0);
        s.anchorChild = -s.comOffsetLocal;
    }
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        int p = kParts[i].parent;
        if ( p < 0 )
            continue;
        Segment &s = m_segs[i];
        Segment &ps = m_segs[p];
        s.anchorParent = rotateInv(ps.q, pose.bonePos[i] - ps.pos);
        s.relBind = qnormalize(conj(ps.q) * s.q);
        s.relTarget = s.relBind;
    }
    m_standingPelvisHeight = m_segs[PART_PELVIS].pos.z - pose.groundZ;
    if ( m_standingPelvisHeight < 10.0f )
        return false;
    m_state = STATE_STANDING;
    m_stateTime = 0.0f;
    m_stability = 1.0f;
    m_comOffset = v3(0, 0, 0);
    m_bias = v3(0, 0, 0);
    m_tiltTime = 0.0f;
    m_swingFoot = -1;
    m_authorityScale = 1.0f;
    m_leanAngle = 0.0f;
    m_armRaise = 0.0f;
    m_budget = 1.0f;
    m_prevRootTarget = pose.bonePos[PART_PELVIS];
    m_rootVel = v3(0, 0, 0);
    m_haveRoot = true;
    m_initialized = true;
    writeOutput();
    return true;
}

void ActiveRagdoll::setTarget(const PoseInput &pose)
{
    m_target = pose;
    m_groundZ = pose.groundZ;
    m_haveRoot = m_haveRoot && m_initialized;
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        int p = kParts[i].parent;
        if ( p < 0 )
            continue;
        m_segs[i].relTarget = qnormalize(conj(qnormalize(pose.boneQuat[p])) * qnormalize(pose.boneQuat[i]));
    }
}

void ActiveRagdoll::applyImpulse(int part, Vec3 worldPoint, Vec3 impulse)
{
    if ( part < 0 || part >= PART_COUNT || !m_initialized )
        return;
    Segment &s = m_segs[part];
    Vec3 J = impulse * m_params.impulseScale;
    s.vel += J * s.invMass;
    Vec3 r = worldPoint - s.pos;
    s.omega += cross(r, J) * s.invInertia;
    // the balance budget: a hit of 230 (mass * u/s) on a 1-mass body spends it all; it recovers in updateState
    m_budget = clampf(m_budget - length(J) / (230.0f * m_params.bodyMass), 0.0f, 1.0f);
    // a hit also shoves the neighbours a little through the joint solve; the COM velocity change feeds the balance
    if ( m_state == STATE_STANDING )
        m_state = STATE_STUMBLING, m_stateTime = 0.0f, ++m_stumbles;
}

Vec3 ActiveRagdoll::com() const
{
    Vec3 c = v3(0, 0, 0);
    float m = 0.0f;
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        c += m_segs[i].pos * m_segs[i].mass;
        m += m_segs[i].mass;
    }
    return c * (1.0f / m);
}

void ActiveRagdoll::forceFall()
{
    if ( !m_initialized || m_state == STATE_FALLEN )
        return;
    m_state = STATE_FALLEN;
    m_stateTime = 0.0f;
    ++m_falls;
}

void ActiveRagdoll::forceGetUp()
{
    if ( !m_initialized || m_state != STATE_FALLEN )
        return;
    m_state = STATE_GETTING_UP;
    m_stateTime = 0.0f;
}

float ActiveRagdoll::getupBlend() const
{
    if ( m_state == STATE_FALLEN )
        return 0.0f;
    if ( m_state == STATE_GETTING_UP )
        return clampf(m_stateTime / m_params.getupTime, 0.0f, 1.0f);
    return 1.0f;
}

// --- XPBD corrections ---------------------------------------------------------------------------------------------
// positional: move the point ra (world offset from a's COM) and rb so that ra - rb changes by corr; b may be -1 (world)
void ActiveRagdoll::positionalCorrection(int a, int b, Vec3 ra, Vec3 rb, Vec3 corr, float compliance, float h)
{
    float c = length(corr);
    if ( c < 1e-6f )
        return;
    Vec3 n = corr * (1.0f / c);
    Segment &A = m_segs[a];
    Vec3 rna = cross(ra, n);
    float wa = A.invMass + dot(rna, rna) * A.invInertia;
    float wb = 0.0f;
    Vec3 rnb = v3(0, 0, 0);
    if ( b >= 0 )
    {
        Segment &B = m_segs[b];
        rnb = cross(rb, n);
        wb = B.invMass + dot(rnb, rnb) * B.invInertia;
    }
    float w = wa + wb + compliance / (h * h);
    if ( w < 1e-9f )
        return;
    float lambda = c / w;
    Vec3 p = n * lambda;
    A.pos += p * A.invMass;
    A.q = qIntegrate(A.q, cross(ra, p) * A.invInertia, 1.0f);
    if ( b >= 0 )
    {
        Segment &B = m_segs[b];
        B.pos -= p * B.invMass;
        B.q = qIntegrate(B.q, cross(rb, p) * (-B.invInertia), 1.0f);
    }
}

// angular: rotate b by +rotVec relative to a (world axis * angle); a may be -1 (world)
void ActiveRagdoll::angularCorrection(int a, int b, Vec3 rotVec, float compliance, float h)
{
    float theta = length(rotVec);
    if ( theta < 1e-7f )
        return;
    Vec3 n = rotVec * (1.0f / theta);
    float wa = a >= 0 ? m_segs[a].invInertia : 0.0f;
    float wb = b >= 0 ? m_segs[b].invInertia : 0.0f;
    float w = wa + wb + compliance / (h * h);
    if ( w < 1e-9f )
        return;
    float lambda = theta / w;
    if ( b >= 0 )
        m_segs[b].q = qIntegrate(m_segs[b].q, n * (lambda * wb), 1.0f);
    if ( a >= 0 )
        m_segs[a].q = qIntegrate(m_segs[a].q, n * (-lambda * wa), 1.0f);
}

// --- substep pieces -----------------------------------------------------------------------------------------------
void ActiveRagdoll::integrate(float h)
{
    Vec3 g = v3(0, 0, -m_params.gravity);
    float ld = clampf(1.0f - m_params.linearDamping * h, 0.0f, 1.0f);
    float ad = clampf(1.0f - m_params.angularDamping * h, 0.0f, 1.0f);
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        Segment &s = m_segs[i];
        s.prevPos = s.pos;
        s.prevQ = s.q;
        s.vel += g * h;
        if ( i == PART_PELVIS )
            s.vel += m_bias * h;
        s.vel = s.vel * ld;
        s.omega = s.omega * ad;
        s.pos += s.vel * h;
        s.q = qIntegrate(s.q, s.omega, h);
    }
}

void ActiveRagdoll::solveJoints(float h)
{
    for ( int i = 1; i < PART_COUNT; ++i )
    {
        int p = kParts[i].parent;
        Segment &s = m_segs[i];
        Segment &ps = m_segs[p];
        Vec3 ra = rotate(ps.q, s.anchorParent);
        Vec3 rb = rotate(s.q, s.anchorChild);
        Vec3 pa = ps.pos + ra;
        Vec3 pb = s.pos + rb;
        // bring pa to pb: correction on a is (pb - pa)
        positionalCorrection(p, i, ra, rb, pb - pa, 0.0f, h);
    }
}

void ActiveRagdoll::solveMotors(float h, float muscle)
{
    float err = 0.0f;
    for ( int i = 1; i < PART_COUNT; ++i )
    {
        int p = kParts[i].parent;
        Segment &s = m_segs[i];
        Segment &ps = m_segs[p];
        Quat target = ps.q * s.relTarget;
        // balance: lean the torso against the fall, raise the arms, bend the swing knee
        if ( m_leanAngle != 0.0f && (i == PART_TORSO_LOWER || i == PART_TORSO_UPPER) )
            target = qFromAxisAngle(m_leanAxis, m_leanAngle * 0.5f) * target;
        if ( m_armRaise != 0.0f && (i == PART_UPPER_ARM_L || i == PART_UPPER_ARM_R) )
        {
            Vec3 side = i == PART_UPPER_ARM_L ? v3(0, 1, 0) : v3(0, -1, 0);
            Vec3 axis = rotate(ps.q, side);
            target = qFromAxisAngle(axis, -m_armRaise) * target; // out and up
        }
        if ( m_swingFoot >= 0 && ((i == PART_SHIN_L && m_swingFoot == PART_FOOT_L) || (i == PART_SHIN_R && m_swingFoot == PART_FOOT_R)) )
        {
            Vec3 axis = rotate(ps.q, v3(0, 1, 0));
            target = qFromAxisAngle(axis, 0.6f) * target; // knee flexion
        }
        Quat qerr = qnormalize(target * conj(s.q));
        Vec3 e = qToRotVec(qerr);
        err += length(e);
        float k = kParts[i].muscle * muscle;
        if ( k <= 0.0f )
            continue;
        // XPBD compliance (rad per unit of generalized correction, times h^2 inside): the segments' inverse
        // inertias are ~0.3-30, so 5e-5 at k = 1 is a stiff muscle that still gives under a hit
        float compliance = 5e-5f / k;
        angularCorrection(p, i, e, compliance, h);
    }
    m_trackingError = err / (PART_COUNT - 1);
}

void ActiveRagdoll::solveLimits(float h)
{
    for ( int i = 1; i < PART_COUNT; ++i )
    {
        const PartDesc &d = kParts[i];
        if ( d.coneDeg <= 0.0f )
            continue;
        int p = d.parent;
        Segment &s = m_segs[i];
        Segment &ps = m_segs[p];
        Vec3 axisCur = rotate(s.q, s.axisLocal);
        Vec3 axisBind = rotate(ps.q * s.relBind, s.axisLocal);
        float c = clampf(dot(axisCur, axisBind), -1.0f, 1.0f);
        float ang = acosf(c);
        float lim = d.coneDeg * DEG2RAD;
        if ( ang <= lim )
            continue;
        Vec3 axis = cross(axisCur, axisBind);
        if ( lengthSq(axis) < 1e-10f )
            continue;
        angularCorrection(p, i, normalize(axis) * (ang - lim), 0.0f, h);
    }
}

void ActiveRagdoll::solveGround(float h)
{
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        Segment &s = m_segs[i];
        s.onGround = false;
        Vec3 axis = rotate(s.q, s.axisLocal) * (s.length * 0.5f);
        for ( int e = 0; e < 2; ++e )
        {
            Vec3 r = e ? axis : -axis;
            Vec3 pt = s.pos + r;
            float pen = m_groundZ - (pt.z - s.radius);
            if ( pen <= 0.0f )
                continue;
            s.onGround = true;
            positionalCorrection(i, -1, r, v3(0, 0, 0), v3(0, 0, pen), 0.0f, h);
        }
    }
}

void ActiveRagdoll::solveAuthority(float h, float authority, float upright, float footPlant)
{
    Segment &pel = m_segs[PART_PELVIS];
    if ( authority > 0.0f )
    {
        // the animated pelvis COM
        Vec3 target = m_target.bonePos[PART_PELVIS] + rotate(qnormalize(m_target.boneQuat[PART_PELVIS]), pel.comOffsetLocal);
        Vec3 d = target - pel.pos;
        float compliance = 2e-3f / authority; // pelvis inverse mass ~7: a sixth of the gap per substep, so hits add up against it
        positionalCorrection(PART_PELVIS, -1, v3(0, 0, 0), v3(0, 0, 0), d, compliance, h);
    }
    if ( upright > 0.0f )
    {
        Quat qerr = qnormalize(qnormalize(m_target.boneQuat[PART_PELVIS]) * conj(pel.q));
        angularCorrection(-1, PART_PELVIS, qToRotVec(qerr), 1e-4f / upright, h);
    }
    if ( footPlant > 0.0f )
    {
        for ( int f = 0; f < 2; ++f )
        {
            int part = f ? PART_FOOT_R : PART_FOOT_L;
            Segment &foot = m_segs[part];
            Vec3 target;
            if ( m_swingFoot == part )
                target = m_swingTarget + rotate(foot.q, foot.comOffsetLocal);
            else
                target = m_target.bonePos[part] + rotate(qnormalize(m_target.boneQuat[part]), foot.comOffsetLocal);
            // only the planted / stepping foot while it is near the ground: a foot in the air follows the motors
            positionalCorrection(part, -1, v3(0, 0, 0), v3(0, 0, 0), target - foot.pos, 2e-4f / footPlant, h);
        }
    }
}

void ActiveRagdoll::updateVelocities(float h)
{
    float inv = 1.0f / h;
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        Segment &s = m_segs[i];
        s.vel = (s.pos - s.prevPos) * inv;
        Quat dq = qnormalize(s.q * conj(s.prevQ));
        s.omega = qToRotVec(dq) * inv;
        if ( s.onGround )
        {
            // friction: tangential velocity of a resting segment
            float keep = 1.0f - m_params.groundFriction;
            s.vel.x *= keep;
            s.vel.y *= keep;
            if ( s.vel.z < 0.0f )
                s.vel.z = 0.0f;
            s.omega = s.omega * keep;
        }
        // safety clamps: nothing in a game body moves faster than this
        float vl = length(s.vel);
        if ( vl > 4000.0f )
            s.vel = s.vel * (4000.0f / vl);
        float wl = length(s.omega);
        if ( wl > 60.0f )
            s.omega = s.omega * (60.0f / wl);
    }
}

// --- the balance controller ---------------------------------------------------------------------------------------
void ActiveRagdoll::balanceController(float dt)
{
    Vec3 c = com();
    Vec3 cv = v3(0, 0, 0);
    float m = 0.0f;
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        cv += m_segs[i].vel * m_segs[i].mass;
        m += m_segs[i].mass;
    }
    cv = cv * (1.0f / m) - m_rootVel;
    cv.z = 0.0f;
    float hgt = c.z - m_groundZ;
    if ( hgt < 1.0f )
        hgt = 1.0f;
    float tau = sqrtf(hgt / m_params.gravity);
    // the support: between the animation's feet (where the gait wants them)
    Vec3 fl = m_target.bonePos[PART_FOOT_L];
    Vec3 fr = m_target.bonePos[PART_FOOT_R];
    Vec3 support = (fl + fr) * 0.5f;
    Vec3 cp = c + cv * tau;
    Vec3 d = v3(cp.x - support.x, cp.y - support.y, 0.0f);
    m_comOffset = d;
    float dist = length(d);
    float stab = 1.0f - (dist - m_params.supportRadius) / (m_params.stepRadius - m_params.supportRadius);
    m_stability = clampf(stab, 0.0f, 1.0f);

    m_leanAngle = 0.0f;
    m_armRaise = 0.0f;
    m_swingFoot = -1;
    m_authorityScale = 0.15f + 0.85f * m_budget;
    if ( m_state == STATE_FALLEN || m_state == STATE_GETTING_UP )
        return;
    if ( dist <= m_params.supportRadius * 0.6f )
        return;
    float excess = dist - m_params.supportRadius * 0.6f;
    float k = m_params.balanceStrength;
    Vec3 dir = d * (1.0f / dist);
    // hip strategy: lean the torso away from the capture point (about the horizontal axis perpendicular to d)
    m_leanAxis = cross(dir, v3(0, 0, 1));
    m_leanAngle = clampf(excess * 0.035f * k, 0.0f, 0.6f);
    // arms out, more the worse it is
    m_armRaise = clampf(excess * 0.05f * k, 0.0f, 1.2f);
    // step strategy: the foot on the side of the fall steps under the capture point
    if ( dist > m_params.supportRadius )
    {
        float sideL = dot(fl - support, dir);
        float sideR = dot(fr - support, dir);
        m_swingFoot = sideL >= sideR ? PART_FOOT_L : PART_FOOT_R;
        Vec3 base = m_swingFoot == PART_FOOT_L ? fl : fr;
        Vec3 want = support + dir * (dist * 1.1f);
        want.z = base.z;
        // the foot does not teleport: towards the capture point at a limited rate
        Vec3 cur = m_segs[m_swingFoot].pos - rotate(m_segs[m_swingFoot].q, m_segs[m_swingFoot].comOffsetLocal);
        Vec3 to = want - cur;
        float maxStep = 120.0f * dt * k; // u per frame at 1
        float tl = length(to);
        if ( tl > maxStep )
            to = to * (maxStep / tl);
        m_swingTarget = cur + to;
        m_swingTarget.z = base.z;
    }
    m_authorityScale = (0.35f + 0.65f * m_stability) * (0.15f + 0.85f * m_budget);
    if ( m_state == STATE_STANDING && m_stability < 0.75f )
    {
        m_state = STATE_STUMBLING;
        m_stateTime = 0.0f;
        ++m_stumbles;
    }
}

void ActiveRagdoll::updateState(float dt)
{
    m_stateTime += dt;
    m_budget = clampf(m_budget + dt * 0.7f, 0.0f, 1.0f);
    float pelvisH = m_segs[PART_PELVIS].pos.z - m_groundZ;
    // torso tilt from vertical
    Vec3 up = normalize(m_segs[PART_TORSO_UPPER].pos - m_segs[PART_PELVIS].pos);
    float tilt = acosf(clampf(up.z, -1.0f, 1.0f));
    bool tilted = tilt > m_params.fallTiltDeg * DEG2RAD;
    m_tiltTime = tilted ? m_tiltTime + dt : 0.0f;
    switch ( m_state )
    {
    case STATE_STANDING:
        break;
    case STATE_STUMBLING:
        if ( m_stability <= 0.0f || pelvisH < m_standingPelvisHeight * m_params.fallHeightFrac || m_tiltTime > m_params.fallTiltTime )
        {
            m_state = STATE_FALLEN;
            m_stateTime = 0.0f;
            ++m_falls;
        }
        else if ( m_stability > 0.9f && m_stateTime > 0.6f && m_trackingError < 0.25f )
        {
            m_state = STATE_STANDING;
            m_stateTime = 0.0f;
        }
        break;
    case STATE_FALLEN:
        if ( m_stateTime > m_params.getupDelay )
        {
            m_state = STATE_GETTING_UP;
            m_stateTime = 0.0f;
        }
        break;
    case STATE_GETTING_UP:
        if ( m_stateTime > m_params.getupTime )
        {
            m_state = STATE_STANDING;
            m_stateTime = 0.0f;
            m_tiltTime = 0.0f;
        }
        break;
    }
}

void ActiveRagdoll::step(float dt)
{
    if ( !m_initialized )
        return;
    if ( dt <= 0.0f )
        return;
    if ( dt > 0.1f )
        dt = 0.1f;
    int n = (int)(dt / m_params.substepDt) + 1;
    if ( n > m_params.maxSubsteps )
        n = m_params.maxSubsteps;
    float h = dt / (float)n;

    // the animation's own motion (root motion / the actor moving): the balance is judged relative to it
    Vec3 rootNow = m_target.bonePos[PART_PELVIS];
    Vec3 rv = (rootNow - m_prevRootTarget) * (1.0f / dt);
    if ( length(rv) > 1200.0f )
        rv = v3(0, 0, 0); // a teleport, not a velocity
    m_rootVel = lerp(m_rootVel, rv, 0.5f);
    m_prevRootTarget = rootNow;

    balanceController(dt);

    float muscle = m_params.muscleStrength;
    float authority = m_params.authority * m_authorityScale;
    float upright = m_params.uprightStrength * (0.3f + 0.7f * m_stability);
    float footPlant = m_params.footPlant * (0.2f + 0.8f * m_budget);
    if ( m_state == STATE_FALLEN )
    {
        muscle *= m_params.fallenMuscle;
        authority = 0.0f;
        upright = 0.0f;
        footPlant = 0.0f;
    }
    else if ( m_state == STATE_GETTING_UP )
    {
        float t = getupBlend();
        muscle *= lerpf(m_params.fallenMuscle, 1.0f, t);
        authority *= t;
        upright *= t;
        footPlant *= t;
    }
    for ( int s = 0; s < n; ++s )
    {
        integrate(h);
        solveAuthority(h, authority, upright, footPlant);
        solveMotors(h, muscle);
        solveJoints(h);
        solveLimits(h);
        solveGround(h);
        solveJoints(h);
        updateVelocities(h);
    }
    updateState(dt);
    m_bias = v3(0, 0, 0);
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        if ( !finite3(m_segs[i].pos) || !finiteq(m_segs[i].q) )
        {
            // never hand NaN to the renderer: snap back to the animation
            init(m_target, m_params);
            return;
        }
    }
    writeOutput();
}

void ActiveRagdoll::writeOutput()
{
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        const Segment &s = m_segs[i];
        m_out.boneQuat[i] = s.q;
        m_out.bonePos[i] = s.pos - rotate(s.q, s.comOffsetLocal);
    }
}

} // namespace euphoria
