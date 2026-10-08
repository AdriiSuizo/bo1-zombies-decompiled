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
    p.fallHeightFrac = 0.45f;
    p.fallTiltDeg = 65.0f;
    p.fallTiltTime = 0.35f;
    p.getupDelay = 1.2f;
    p.getupTime = 1.1f;
    p.groundFriction = 0.85f;
    p.linearDamping = 1.5f;
    p.angularDamping = 3.0f;
    p.jointDamping = 14.0f;
    p.substepDt = 1.0f / 120.0f;
    p.maxSubsteps = 8;
    p.fallenMuscle = 0.12f;
    p.impulseScale = 1.0f;
    p.addShockSpin = true;
    p.shockSpinMin = 1.5f;
    p.shockSpinMax = 7.0f;
    p.shockSpinDecayMult = 6.0f;
    p.spinePainMultiplier = 0.55f;
    p.spinePainTime = 0.45f;
    p.spinePainTwistMultiplier = 0.35f;
    p.reachForWound = true;
    p.timeBeforeReachForWound = 0.2f;
    p.reachAbsorbtionTime = 0.7f;
    p.armReachAmount = 0.8f;
    p.useHeadLook = true;
    p.headLookAtWoundMinTimer = 0.3f;
    p.headLookAtWoundMaxTimer = 0.8f;
    p.timeBeforeCollapseWoundLeg = 0.08f;
    p.woundLegCollapseTime = 0.45f;
    p.woundLegStiffness = 0.6f;
    p.upperBodyFlinch = true;
    p.flinchTime = 0.25f;
    p.stiffnessDecayTarget = 0.45f;
    p.useCatchFall = true;
    p.useArmToSlowDown = true;
    p.tryToAvoidHeadbuttingGround = true;
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
    m_woundActive = false;
    m_woundLegTimer = 0.0f;
    m_shockSpin = 0.0f;
    m_headLookTime = 0.0f;
    m_catchFalling = false;
    m_fallDir = v3(1, 0, 0);
    m_fwd = v3(1, 0, 0);
    m_left = v3(0, 1, 0);
    m_curAuthority = 1.0f;
    m_curUpright = 1.0f;
    updateBodyAxes();
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
    // a bullet on one light segment: its own velocity change is capped (the joints spread the rest), and the spin it
    // gets from the lever arm is capped too (a 7-unit lever on a torso segment would otherwise be ~100 rad/s)
    Vec3 dv = J * s.invMass;
    float dvl = length(dv);
    if ( dvl > 500.0f )
    {
        // momentum is kept: what the segment cannot take at once goes to the whole body (the joints would pass it on)
        Vec3 dvSeg = dv * (500.0f / dvl);
        Vec3 rest = J - dvSeg * s.mass;
        float total = 0.0f;
        for ( int i = 0; i < PART_COUNT; ++i )
            total += m_segs[i].mass;
        for ( int i = 0; i < PART_COUNT; ++i )
            m_segs[i].vel += rest * (1.0f / total);
        dv = dvSeg;
    }
    s.vel += dv;
    Vec3 r = worldPoint - s.pos;
    Vec3 dw = cross(r, J) * s.invInertia;
    float dwl = length(dw);
    if ( dwl > 10.0f )
        dw = dw * (10.0f / dwl);
    s.omega += dw;
    // the balance budget: a hit of 230 (mass * u/s) on a 1-mass body spends it all; it recovers in updateState
    float strength = clampf(length(J) / (230.0f * m_params.bodyMass), 0.0f, 1.0f);
    m_budget = clampf(m_budget - strength, 0.0f, 1.0f);
    // a hit also shoves the neighbours a little through the joint solve; the COM velocity change feeds the balance
    if ( m_state == STATE_STANDING )
        m_state = STATE_STUMBLING, m_stateTime = 0.0f, ++m_stumbles;

    // --- the shot behaviour (GTA IV's NmRsCBUShot, in this mod's own terms) ---
    if ( strength < 0.02f )
        return;
    Vec3 dir = normalize(J);
    m_woundActive = true;
    m_woundPart = part;
    m_woundLocal = rotateInv(s.q, worldPoint - s.pos);
    m_woundDir = dir;
    m_woundTime = 0.0f;
    m_woundStrength = strength;
    // which side was hit (the body's left is from the hips; bones have no known lateral axis)
    updateBodyAxes();
    Vec3 fromPelvis = worldPoint - m_segs[PART_PELVIS].pos;
    m_woundSide = dot(fromPelvis, m_left) >= 0.0f ? -1 : 1; // -1 = the left hand reaches (the nearer one)
    if ( part == PART_UPPER_ARM_L || part == PART_FOREARM_L )
        m_woundSide = 1;
    if ( part == PART_UPPER_ARM_R || part == PART_FOREARM_R )
        m_woundSide = -1;
    bool upperBody = part == PART_TORSO_LOWER || part == PART_TORSO_UPPER || part == PART_HEAD || part == PART_UPPER_ARM_L || part == PART_UPPER_ARM_R || part == PART_FOREARM_L || part == PART_FOREARM_R;
    if ( m_params.addShockSpin && upperBody )
    {
        // torque about the vertical from a hit off the centre line: the body spins away from the struck side
        Vec3 r2 = v3(fromPelvis.x, fromPelvis.y, 0.0f);
        Vec3 d2 = v3(dir.x, dir.y, 0.0f);
        float lever = cross(r2, d2).z; // units: a centre hit has none, a shoulder hit ~7
        float spin = clampf(fabsf(lever) * 0.12f, 0.0f, 1.0f);
        float mag = spin > 0.1f ? lerpf(m_params.shockSpinMin, m_params.shockSpinMax, spin) * (0.3f + 0.7f * strength) : 0.0f;
        m_shockSpin = lever >= 0.0f ? mag : -mag;
    }
    if ( part == PART_THIGH_L || part == PART_SHIN_L || part == PART_FOOT_L )
        m_woundLeg = PART_THIGH_L, m_woundLegTimer = m_params.timeBeforeCollapseWoundLeg + m_params.woundLegCollapseTime;
    else if ( part == PART_THIGH_R || part == PART_SHIN_R || part == PART_FOOT_R )
        m_woundLeg = PART_THIGH_R, m_woundLegTimer = m_params.timeBeforeCollapseWoundLeg + m_params.woundLegCollapseTime;
    if ( m_params.useHeadLook )
        m_headLookTime = lerpf(m_params.headLookAtWoundMinTimer, m_params.headLookAtWoundMaxTimer, strength);
}

void ActiveRagdoll::updateBodyAxes()
{
    Vec3 l = m_target.bonePos[PART_THIGH_L] - m_target.bonePos[PART_THIGH_R];
    l.z = 0.0f;
    if ( lengthSq(l) < 1e-4f )
        return;
    m_left = normalize(l);
    m_fwd = normalize(cross(m_left, v3(0, 0, 1)));
}

Vec3 ActiveRagdoll::woundPoint() const
{
    if ( !m_woundActive )
        return v3(0, 0, 0);
    const Segment &s = m_segs[m_woundPart];
    return s.pos + rotate(s.q, m_woundLocal);
}

Vec3 ActiveRagdoll::reachingHand() const
{
    int fore = m_woundSide < 0 ? PART_FOREARM_L : PART_FOREARM_R;
    const Segment &f = m_segs[fore];
    return f.pos + rotate(f.q, f.axisLocal) * (f.length * 0.5f);
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
        {
            s.vel += m_bias * h;
            // the pelvis springs (authority, upright) need damping or the whole body rings around the animation
            Vec3 rel = s.vel - m_rootVel;
            s.vel -= rel * clampf(6.0f * m_curAuthority * h, 0.0f, 0.9f);
            s.omega = s.omega * (1.0f - clampf(10.0f * m_curUpright * h, 0.0f, 0.9f));
        }
        // shock spin: the torso's yaw rate is driven towards the spin (a rate, not an accumulation per substep)
        if ( m_shockSpin != 0.0f && (i == PART_TORSO_LOWER || i == PART_TORSO_UPPER || i == PART_HEAD) )
            s.omega.z += (m_shockSpin - s.omega.z) * clampf(h * 30.0f, 0.0f, 1.0f);
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
            // out and up: abduction about the body's forward axis (a hanging left arm swings towards the body's left)
            float sign = i == PART_UPPER_ARM_L ? 1.0f : -1.0f;
            target = qFromAxisAngle(m_fwd, sign * m_armRaise) * target;
        }
        if ( m_swingFoot >= 0 && ((i == PART_SHIN_L && m_swingFoot == PART_FOOT_L) || (i == PART_SHIN_R && m_swingFoot == PART_FOOT_R)) )
            target = qFromAxisAngle(m_left, 0.6f) * target; // knee flexion: the shin swings back about the body's lateral axis
        float partMuscle = 1.0f;
        if ( m_woundActive )
        {
            float t = m_woundTime;
            // spine pain: the torso folds around the wound (bends in the bullet's direction) and twists, then recovers
            if ( (i == PART_TORSO_LOWER || i == PART_TORSO_UPPER) && t < m_params.spinePainTime )
            {
                float env = sinf(PI * t / m_params.spinePainTime);
                Vec3 bendAxis = cross(v3(0, 0, 1), m_woundDir);
                float bend = m_params.spinePainMultiplier * m_woundStrength * env * 0.5f;
                if ( lengthSq(bendAxis) > 1e-6f )
                    target = qFromAxisAngle(normalize(bendAxis), bend) * target;
                float twist = m_params.spinePainTwistMultiplier * m_woundStrength * env * (float)m_woundSide * 0.5f;
                target = qFromAxisAngle(v3(0, 0, 1), twist) * target;
            }
            // flinch: shoulders up and in for an instant
            if ( m_params.upperBodyFlinch && (i == PART_UPPER_ARM_L || i == PART_UPPER_ARM_R) && t < m_params.flinchTime )
            {
                // arms come up and forward about the body's lateral axis
                float env = sinf(PI * t / m_params.flinchTime);
                target = qFromAxisAngle(m_left, -0.5f * env * m_woundStrength) * target;
            }
            // head look: the head turns towards the wound (yaw from the body's facing; the head bone has no forward axis)
            if ( i == PART_HEAD && m_headLookTime > 0.0f )
            {
                Vec3 toWound = woundPoint() - s.pos;
                toWound.z = 0.0f;
                if ( lengthSq(toWound) > 1.0f )
                {
                    Vec3 w = normalize(toWound);
                    float ang = atan2f(cross(m_fwd, w).z, dot(m_fwd, w));
                    target = qFromAxisAngle(v3(0, 0, 1), clampf(ang, -0.8f, 0.8f) * 0.35f) * target;
                }
            }
        }
        // a wounded leg gives after a moment: its knee cannot hold
        if ( m_woundLegTimer > 0.0f && m_woundLegTimer < m_params.woundLegCollapseTime
            && (i == m_woundLeg || (m_woundLeg == PART_THIGH_L && i == PART_SHIN_L) || (m_woundLeg == PART_THIGH_R && i == PART_SHIN_R)) )
        {
            partMuscle = m_params.woundLegStiffness;
            if ( i != m_woundLeg )
                target = qFromAxisAngle(m_left, 0.35f) * target; // the knee buckles
        }
        // catch fall: the arms go towards the ground in the fall direction, the head away from it
        if ( m_catchFalling )
        {
            if ( m_params.useArmToSlowDown && (i == PART_UPPER_ARM_L || i == PART_UPPER_ARM_R) )
            {
                Vec3 want = normalize(m_fallDir + v3(0, 0, -0.9f));
                Vec3 cur = rotate(target, s.axisLocal);
                target = qFromTo(normalize(cur), want) * target;
                partMuscle *= 1.5f;
            }
            if ( m_params.tryToAvoidHeadbuttingGround && i == PART_HEAD )
            {
                Vec3 axis = cross(m_fallDir, v3(0, 0, 1));
                if ( lengthSq(axis) > 1e-6f )
                    target = qFromAxisAngle(normalize(axis), 0.5f) * target;
            }
        }
        Quat qerr = qnormalize(target * conj(s.q));
        Vec3 e = qToRotVec(qerr);
        err += length(e);
        float k = kParts[i].muscle * muscle * partMuscle;
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

// the shot behaviour's reach for wound: the hand of the far side crosses to the wound and stays a moment
void ActiveRagdoll::solveReach(float h)
{
    if ( !m_woundActive || !m_params.reachForWound )
        return;
    if ( m_state == STATE_FALLEN || m_catchFalling )
        return;
    // only wounds a standing body can reach without bending down: the torso and the pelvis
    if ( m_woundPart != PART_TORSO_LOWER && m_woundPart != PART_TORSO_UPPER && m_woundPart != PART_PELVIS )
        return;
    float t = m_woundTime - m_params.timeBeforeReachForWound;
    if ( t < 0.0f || t > m_params.reachAbsorbtionTime )
        return;
    float env = sinf(PI * t / m_params.reachAbsorbtionTime);
    int fore = m_woundSide < 0 ? PART_FOREARM_L : PART_FOREARM_R;
    Segment &f = m_segs[fore];
    Vec3 r = rotate(f.q, f.axisLocal) * (f.length * 0.5f);
    Vec3 hand = f.pos + r;
    Vec3 want = woundPoint();
    float compliance = 2e-2f / (m_params.armReachAmount * env * (0.3f + 0.7f * m_woundStrength) + 1e-3f);
    positionalCorrection(fore, -1, r, v3(0, 0, 0), want - hand, compliance, h);
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
        // joint damping: the angular velocity relative to the parent decays (XPBD motors alone ring)
        int p = kParts[i].parent;
        if ( p >= 0 )
        {
            Vec3 rel = s.omega - m_segs[p].omega;
            s.omega -= rel * clampf(m_params.jointDamping * h, 0.0f, 0.9f);
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
        // the arms are light and fast (a reach, a flinch): their velocity would swamp the capture point
        if ( i == PART_UPPER_ARM_L || i == PART_UPPER_ARM_R || i == PART_FOREARM_L || i == PART_FOREARM_R )
            continue;
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

    updateBodyAxes();
    balanceController(dt);

    // the shot behaviour's clocks
    if ( m_woundActive )
    {
        m_woundTime += dt;
        if ( m_woundTime > 2.0f )
            m_woundActive = false;
    }
    if ( m_headLookTime > 0.0f )
        m_headLookTime -= dt;
    if ( m_woundLegTimer > 0.0f )
        m_woundLegTimer -= dt;
    m_shockSpin *= clampf(1.0f - m_params.shockSpinDecayMult * dt, 0.0f, 1.0f);
    if ( fabsf(m_shockSpin) < 0.05f )
        m_shockSpin = 0.0f;
    // catch fall: once the balance is gone (still up, or just down), arms out towards the ground in the fall direction
    m_catchFalling = false;
    if ( m_params.useCatchFall && ((m_state == STATE_STUMBLING && m_stability < 0.2f) || (m_state == STATE_FALLEN && m_stateTime < 0.6f)) )
    {
        Vec3 d = m_comOffset;
        d.z = 0.0f;
        if ( lengthSq(d) > 1.0f )
        {
            m_fallDir = normalize(d);
            m_catchFalling = true;
        }
    }

    // shotRelax: the muscles relax as the balance budget is spent (stiffnessDecayTarget at 0), back with it
    float muscle = m_params.muscleStrength * lerpf(m_params.stiffnessDecayTarget, 1.0f, m_budget);
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
    m_curAuthority = authority;
    m_curUpright = upright;
    for ( int s = 0; s < n; ++s )
    {
        integrate(h);
        solveAuthority(h, authority, upright, footPlant);
        solveMotors(h, muscle);
        solveReach(h);
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
