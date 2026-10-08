#pragma once
// mod: euphoria - the active-ragdoll core (NOT part of the original game, and not NaturalMotion's Euphoria).
//
// An articulated body of 14 rigid segments (pelvis, lower and upper torso, head, upper arms, forearms, thighs,
// shins, feet) simulated with position-based rigid-body dynamics (XPBD-style substeps):
//   - ball joints between segments, with a swing (cone) limit per joint;
//   - motors: every joint is pulled towards the RELATIVE rotation the game's animation has for it this frame, so the
//     body tracks the retail walk / run / attack animations while gravity, contacts and impacts push it around;
//   - locomotion authority: the pelvis is pulled towards the animated pelvis (the actor's root motion), weaker the
//     less stable the body is, none while it is on the ground;
//   - a balance controller: capture point (COM + velocity * sqrt(h/g)) against the support between the feet; it
//     leans the torso and raises the arms against the fall, steps the swing foot towards the capture point, bends
//     the knee of that leg, and, when the capture point is beyond reach or the pelvis is too low, declares a fall;
//   - falls and get-ups: on the ground the motors go weak and every segment collides with the ground; after a delay
//     the motors and the authority ramp back and the body is pulled up into the animation again;
//   - impulses at a point of a segment (bullets, melee, explosions) with the segment's mass and inertia.
// The engine glue converts the game's animated bone matrices into the PoseInput every frame and writes the Output
// back into the model's skeleton (src/cgame_mp/cg_euphoria.cpp). This file has no engine dependency: it builds in
// the game (MSVC x86) and in tests/euphoria (g++), and is the piece to share with the Rust / C++ controller.
//
// Approximations, stated plainly: the ground is a plane at the actor's ground height (no world collision for the
// segments); segment inertia is isotropic (a sphere of the capsule's mass); the "ankle strategy" is an upright
// spring on the pelvis rather than ankle torque; there is no self-collision between segments.
#include "euphoria_math.h"

namespace euphoria {

enum Part
{
    PART_PELVIS = 0,
    PART_TORSO_LOWER,
    PART_TORSO_UPPER,
    PART_HEAD,
    PART_UPPER_ARM_L,
    PART_FOREARM_L,
    PART_UPPER_ARM_R,
    PART_FOREARM_R,
    PART_THIGH_L,
    PART_SHIN_L,
    PART_FOOT_L,
    PART_THIGH_R,
    PART_SHIN_R,
    PART_FOOT_R,
    PART_COUNT
};

struct PartDesc
{
    const char *name;
    int parent;             // -1 for the pelvis
    const char *bone;       // the game bone the segment starts at (its orientation is the segment's)
    const char *tipBone;    // the bone at the far end (gives the length); "" = use defaultLength along the bone's x
    float defaultLength;    // units, when the tip bone is missing
    float massFrac;         // fraction of the body mass
    float radius;           // capsule radius, units
    float coneDeg;          // swing limit around the bind pose
    float muscle;           // motor stiffness scale (1 = default)
};

extern const PartDesc kParts[PART_COUNT];

struct Params
{
    float gravity;          // u/s^2 (bg_gravity, 800)
    float bodyMass;         // the whole body; impulses are in mass * u/s
    float muscleStrength;   // global motor scale
    float balanceStrength;  // lean / step / arm response scale
    float authority;        // pelvis follows the animation (1 = normal)
    float uprightStrength;  // pelvis orientation follows the animation (the ankle strategy stand-in)
    float footPlant;        // feet follow the animation's feet while standing
    float supportRadius;    // capture point inside this radius of the support centre = stable (units)
    float stepRadius;       // beyond this the body cannot step to recover: it falls (units)
    float fallHeightFrac;   // pelvis below this fraction of its standing height = fallen
    float fallTiltDeg;      // torso tilt from vertical beyond this for fallTiltTime = fallen
    float fallTiltTime;     // s
    float getupDelay;       // s on the ground before getting up
    float getupTime;        // s to ramp back into the animation
    float groundFriction;   // 0..1 per contact, tangential velocity kept = 1 - friction
    float linearDamping;    // 1/s
    float angularDamping;   // 1/s
    float substepDt;        // s
    int maxSubsteps;
    float fallenMuscle;     // motor scale while on the ground
    float impulseScale;     // global scale for applyImpulse
};

Params defaultParams();

// world-space animated pose, every frame (the engine glue fills it from the skeleton)
struct PoseInput
{
    Vec3 bonePos[PART_COUNT];
    Quat boneQuat[PART_COUNT];
    Vec3 tipPos[PART_COUNT];
    bool tipValid[PART_COUNT];
    float groundZ;          // the floor under the actor
};

struct Output
{
    Vec3 bonePos[PART_COUNT];   // world
    Quat boneQuat[PART_COUNT];  // world
};

enum State
{
    STATE_STANDING = 0,
    STATE_STUMBLING,
    STATE_FALLEN,
    STATE_GETTING_UP
};

struct Segment
{
    Vec3 pos;           // COM, world
    Vec3 vel;
    Quat q;             // world orientation (= the bone's orientation)
    Vec3 omega;         // world, rad/s
    Vec3 prevPos;
    Quat prevQ;
    float mass;
    float invMass;
    float invInertia;   // isotropic, body units
    float length;
    float radius;
    Vec3 axisLocal;     // capsule axis in the segment frame (towards the tip)
    Vec3 comOffsetLocal;// COM - bone origin, segment frame
    Vec3 anchorParent;  // joint point in the parent's frame
    Vec3 anchorChild;   // joint point in this segment's frame
    Quat relBind;       // bind rotation of this segment in the parent frame (limits)
    Quat relTarget;     // this frame's animated rotation of this segment in the parent frame (motor target)
    bool onGround;
};

class ActiveRagdoll
{
public:
    ActiveRagdoll();
    // from the first animated pose (lengths, anchors, masses); false if the pose is unusable
    bool init(const PoseInput &pose, const Params &params);
    bool initialized() const { return m_initialized; }
    void setParams(const Params &params) { m_params = params; }
    const Params &params() const { return m_params; }
    // the animated target of this frame
    void setTarget(const PoseInput &pose);
    // an impulse (mass * u/s) at a world point of a segment: bullets, melee, blasts
    void applyImpulse(int part, Vec3 worldPoint, Vec3 impulse);
    // an acceleration on the pelvis for the frame (the server's balance bias on the client)
    void setExternalBias(Vec3 accel) { m_bias = accel; }
    // the authority (server) says it went down: drop now, regardless of the local balance
    void forceFall();
    // the authority says it is up again
    void forceGetUp();
    // 1 fresh .. 0 exhausted: every hit spends it, it recovers over ~1.5 s; the animation's hold on the body scales with it
    float budget() const { return m_budget; }
    void step(float dt);
    const Output &output() const { return m_out; }
    State state() const { return m_state; }
    float stability() const { return m_stability; }   // 1 stable .. 0 falling
    Vec3 comOffset() const { return m_comOffset; }     // capture point - support centre (xy, z = 0)
    float pelvisHeight() const { return m_segs[PART_PELVIS].pos.z - m_groundZ; }
    float standingPelvisHeight() const { return m_standingPelvisHeight; }
    int stumbles() const { return m_stumbles; }
    int falls() const { return m_falls; }
    const Segment &segment(int part) const { return m_segs[part]; }
    Vec3 com() const;
    // 0..1 ramp of the get-up (1 = standing); used by the glue to blend the output back into the animation
    float getupBlend() const;
    // the motors' tracking error this frame, rad (how far from the animation the body is), for tests and debug
    float trackingError() const { return m_trackingError; }

private:
    void integrate(float h);
    void solveJoints(float h);
    void solveMotors(float h, float muscle);
    void solveLimits(float h);
    void solveGround(float h);
    void solveAuthority(float h, float authority, float upright, float footPlant);
    void updateVelocities(float h);
    void balanceController(float dt);
    void updateState(float dt);
    void writeOutput();
    void positionalCorrection(int a, int b, Vec3 ra, Vec3 rb, Vec3 corr, float compliance, float h);
    void angularCorrection(int a, int b, Vec3 rotVec, float compliance, float h);
    float worldInvInertia(int part) const { return m_segs[part].invInertia; }

    Params m_params;
    Segment m_segs[PART_COUNT];
    PoseInput m_target;
    Output m_out;
    State m_state;
    float m_stateTime;
    float m_stability;
    Vec3 m_comOffset;
    Vec3 m_bias;
    float m_groundZ;
    float m_standingPelvisHeight;
    float m_tiltTime;
    int m_stumbles;
    int m_falls;
    bool m_initialized;
    float m_trackingError;
    // balance responses for this frame
    Vec3 m_leanAxis;        // world axis to lean the torso around
    float m_leanAngle;      // rad
    int m_swingFoot;        // PART_FOOT_L / PART_FOOT_R or -1
    Vec3 m_swingTarget;     // world
    float m_armRaise;       // rad
    float m_authorityScale;
    Vec3 m_prevRootTarget;  // the animated pelvis of the previous frame (root motion velocity)
    Vec3 m_rootVel;         // world, u/s
    bool m_haveRoot;
    float m_budget;
};

// hit-location helpers for the glue: which segment a game hit location / bone maps to
int partForHitLocationName(const char *hitLocName);

} // namespace euphoria
