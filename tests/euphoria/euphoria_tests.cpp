// mod: euphoria - tests of the active-ragdoll core on a synthetic zombie-sized skeleton (no engine).
// Build and run: tests/euphoria/run.sh (g++ or clang++; on Windows: cl /EHsc /O2 /I src tests\euphoria\euphoria_tests.cpp src\euphoria\euphoria_body.cpp src\euphoria\euphoria_balance.cpp)
#include "euphoria/euphoria_body.h"
#include "euphoria/euphoria_balance.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>

using namespace euphoria;

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++g_fail; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// A standing humanoid, inches, z up, facing +x. Bone x axes are not needed: the core takes the tip direction.
static void bindPose(PoseInput &p, Vec3 root, float phase, float stride)
{
    memset(&p, 0, sizeof(p));
    p.groundZ = 0.0f;
    for ( int i = 0; i < PART_COUNT; ++i )
        p.boneQuat[i] = qIdentity();
    Vec3 o = root;
    // legs: a simple gait, feet alternate ahead / behind the pelvis by 'stride'
    float swingL = sinf(phase) * stride, swingR = -sinf(phase) * stride;
    float liftL = swingL > 0 ? sinf(phase) * 3.0f : 0.0f, liftR = swingR > 0 ? -sinf(phase) * 3.0f : 0.0f;
    p.bonePos[PART_PELVIS] = o + v3(0, 0, 36);       p.tipPos[PART_PELVIS] = o + v3(0, 0, 40);
    p.bonePos[PART_TORSO_LOWER] = o + v3(0, 0, 40);  p.tipPos[PART_TORSO_LOWER] = o + v3(0, 0, 48);
    p.bonePos[PART_TORSO_UPPER] = o + v3(0, 0, 48);  p.tipPos[PART_TORSO_UPPER] = o + v3(0, 0, 58);
    p.bonePos[PART_HEAD] = o + v3(0, 0, 58);         p.tipPos[PART_HEAD] = o + v3(0, 0, 64);
    p.bonePos[PART_UPPER_ARM_L] = o + v3(0, 7, 56);  p.tipPos[PART_UPPER_ARM_L] = o + v3(0, 9, 45);
    p.bonePos[PART_FOREARM_L] = o + v3(0, 9, 45);    p.tipPos[PART_FOREARM_L] = o + v3(0, 10, 35);
    p.bonePos[PART_UPPER_ARM_R] = o + v3(0, -7, 56); p.tipPos[PART_UPPER_ARM_R] = o + v3(0, -9, 45);
    p.bonePos[PART_FOREARM_R] = o + v3(0, -9, 45);   p.tipPos[PART_FOREARM_R] = o + v3(0, -10, 35);
    p.bonePos[PART_THIGH_L] = o + v3(0, 4, 36);      p.tipPos[PART_THIGH_L] = o + v3(swingL * 0.5f, 4, 19 + liftL * 0.5f);
    p.bonePos[PART_SHIN_L] = p.tipPos[PART_THIGH_L]; p.tipPos[PART_SHIN_L] = o + v3(swingL, 4, 3 + liftL);
    p.bonePos[PART_FOOT_L] = p.tipPos[PART_SHIN_L];  p.tipPos[PART_FOOT_L] = p.bonePos[PART_FOOT_L] + v3(5, 0, -2);
    p.bonePos[PART_THIGH_R] = o + v3(0, -4, 36);     p.tipPos[PART_THIGH_R] = o + v3(swingR * 0.5f, -4, 19 + liftR * 0.5f);
    p.bonePos[PART_SHIN_R] = p.tipPos[PART_THIGH_R]; p.tipPos[PART_SHIN_R] = o + v3(swingR, -4, 3 + liftR);
    p.bonePos[PART_FOOT_R] = p.tipPos[PART_SHIN_R];  p.tipPos[PART_FOOT_R] = p.bonePos[PART_FOOT_R] + v3(5, 0, -2);
    for ( int i = 0; i < PART_COUNT; ++i )
    {
        p.tipValid[i] = true;
        // orient each bone so its x points at its tip (as the game's bones do), to exercise non-identity quats
        Vec3 d = normalize(p.tipPos[i] - p.bonePos[i]);
        p.boneQuat[i] = qFromTo(v3(1, 0, 0), d);
    }
}

struct Sim
{
    ActiveRagdoll rd;
    PoseInput pose;
    float t;
    Vec3 root;
    float speed;
    float stride;
    Sim(float speed_ = 0.0f, float stride_ = 0.0f) : t(0), root(v3(0, 0, 0)), speed(speed_), stride(stride_)
    {
        bindPose(pose, root, 0.0f, stride);
        bool ok = rd.init(pose, defaultParams());
        CHECK(ok, "init");
    }
    void advance(float dt)
    {
        t += dt;
        root.x += speed * dt;
        float phase = speed > 0 ? t * 2.0f * PI * 1.6f : 0.0f;
        bindPose(pose, root, phase, stride);
        rd.setTarget(pose);
        rd.step(dt);
    }
    bool allFinite() const
    {
        for ( int i = 0; i < PART_COUNT; ++i )
            if ( !finite3(rd.output().bonePos[i]) || !finiteq(rd.output().boneQuat[i]) )
                return false;
        return true;
    }
    float jointDeviation(int part) const
    {
        const Segment &s = rd.segment(part);
        const Segment &p = rd.segment(kParts[part].parent);
        Quat rel = qnormalize(conj(p.q) * s.q);
        Quat err = qnormalize(rel * conj(s.relTarget));
        return length(qToRotVec(err));
    }
};

static const char *stateName(State s)
{
    switch ( s ) { case STATE_STANDING: return "standing"; case STATE_STUMBLING: return "stumbling"; case STATE_FALLEN: return "fallen"; default: return "getting_up"; }
}

static void test_standing_stays_up()
{
    printf("standing_stays_up\n");
    Sim s;
    float h0 = s.rd.pelvisHeight();
    float minH = h0, maxErr = 0;
    for ( int i = 0; i < 240; ++i )
    {
        s.advance(1.0f / 60.0f);
        if ( s.rd.pelvisHeight() < minH ) minH = s.rd.pelvisHeight();
        if ( s.rd.trackingError() > maxErr ) maxErr = s.rd.trackingError();
    }
    printf("  pelvis %.1f -> min %.1f, max tracking error %.3f rad, state %s, stumbles %d falls %d\n", h0, minH, maxErr, stateName(s.rd.state()), s.rd.stumbles(), s.rd.falls());
    CHECK(minH > h0 * 0.9f, "pelvis sagged to %.1f of %.1f", minH, h0);
    CHECK(s.rd.falls() == 0, "fell while standing");
    CHECK(s.rd.state() == STATE_STANDING, "state %s", stateName(s.rd.state()));
    CHECK(maxErr < 0.2f, "tracking error %.3f", maxErr);
    CHECK(s.allFinite(), "nan");
}

static void test_small_hit_recovers()
{
    printf("small_hit_recovers\n");
    Sim s;
    for ( int i = 0; i < 60; ++i ) s.advance(1.0f / 60.0f);
    // a chest hit from the front: 60 mass*u/s = the whole body gains 60 u/s backwards
    s.rd.applyImpulse(PART_TORSO_UPPER, s.rd.segment(PART_TORSO_UPPER).pos, v3(-60, 0, 0));
    float minStab = 1, minH = s.rd.pelvisHeight();
    bool stumbled = false;
    for ( int i = 0; i < 180; ++i )
    {
        s.advance(1.0f / 60.0f);
        if ( s.rd.stability() < minStab ) minStab = s.rd.stability();
        if ( s.rd.pelvisHeight() < minH ) minH = s.rd.pelvisHeight();
        if ( s.rd.state() == STATE_STUMBLING ) stumbled = true;
    }
    printf("  min stability %.2f, min pelvis %.1f, state %s, stumbles %d falls %d, com offset %.1f\n", minStab, minH, stateName(s.rd.state()), s.rd.stumbles(), s.rd.falls(), length(s.rd.comOffset()));
    CHECK(stumbled, "no stumble registered");
    CHECK(s.rd.falls() == 0, "fell from a small hit");
    CHECK(s.rd.state() == STATE_STANDING, "did not recover: %s", stateName(s.rd.state()));
    CHECK(minH > s.rd.standingPelvisHeight() * 0.7f, "pelvis dropped to %.1f", minH);
}

static void test_big_hit_falls_and_gets_up()
{
    printf("big_hit_falls_and_gets_up\n");
    Sim s;
    for ( int i = 0; i < 60; ++i ) s.advance(1.0f / 60.0f);
    s.rd.applyImpulse(PART_TORSO_UPPER, s.rd.segment(PART_TORSO_UPPER).pos + v3(0, 0, 4), v3(-320, 0, 60));
    float minH = s.rd.pelvisHeight();
    bool fell = false, gotUp = false;
    float fellAt = -1, upAt = -1;
    for ( int i = 0; i < 60 * 6; ++i )
    {
        s.advance(1.0f / 60.0f);
        if ( s.rd.pelvisHeight() < minH ) minH = s.rd.pelvisHeight();
        if ( s.rd.state() == STATE_FALLEN && !fell ) { fell = true; fellAt = s.t; }
        if ( fell && s.rd.state() == STATE_STANDING && !gotUp ) { gotUp = true; upAt = s.t; }
    }
    printf("  min pelvis %.1f of %.1f, fell at %.2f s, standing again at %.2f s, final pelvis %.1f, state %s, falls %d\n",
        minH, s.rd.standingPelvisHeight(), fellAt, upAt, s.rd.pelvisHeight(), stateName(s.rd.state()), s.rd.falls());
    CHECK(fell, "did not fall from a 320 hit");
    CHECK(minH < s.rd.standingPelvisHeight() * 0.6f, "pelvis never got low: %.1f", minH);
    CHECK(gotUp, "never got up");
    CHECK(s.rd.pelvisHeight() > s.rd.standingPelvisHeight() * 0.8f, "did not stand back up: %.1f", s.rd.pelvisHeight());
    CHECK(s.allFinite(), "nan");
}

static void test_walking_follows_root()
{
    printf("walking_follows_root\n");
    Sim s(60.0f, 9.0f);
    float maxLag = 0;
    for ( int i = 0; i < 180; ++i )
    {
        s.advance(1.0f / 60.0f);
        float lag = fabsf(s.rd.output().bonePos[PART_PELVIS].x - s.pose.bonePos[PART_PELVIS].x);
        if ( lag > maxLag ) maxLag = lag;
    }
    printf("  max pelvis lag %.1f u, state %s, stumbles %d falls %d, pelvis %.1f\n", maxLag, stateName(s.rd.state()), s.rd.stumbles(), s.rd.falls(), s.rd.pelvisHeight());
    CHECK(maxLag < 8.0f, "pelvis lags the root motion by %.1f", maxLag);
    CHECK(s.rd.falls() == 0, "fell while walking");
    CHECK(s.rd.pelvisHeight() > s.rd.standingPelvisHeight() * 0.85f, "sagged while walking");
}

static void test_walking_hit_recovers_and_keeps_walking()
{
    printf("walking_hit_recovers_and_keeps_walking\n");
    Sim s(60.0f, 9.0f);
    for ( int i = 0; i < 60; ++i ) s.advance(1.0f / 60.0f);
    s.rd.applyImpulse(PART_TORSO_LOWER, s.rd.segment(PART_TORSO_LOWER).pos, v3(-70, 30, 0));
    float maxLag = 0;
    bool stumbled = false;
    for ( int i = 0; i < 180; ++i )
    {
        s.advance(1.0f / 60.0f);
        if ( s.rd.state() == STATE_STUMBLING ) stumbled = true;
        float lag = fabsf(s.rd.output().bonePos[PART_PELVIS].x - s.pose.bonePos[PART_PELVIS].x);
        if ( lag > maxLag ) maxLag = lag;
    }
    printf("  stumbled %d, max lag %.1f, falls %d, state %s\n", stumbled, maxLag, s.rd.falls(), stateName(s.rd.state()));
    CHECK(stumbled, "no stumble");
    CHECK(s.rd.falls() == 0, "fell");
    CHECK(s.rd.state() == STATE_STANDING, "state %s", stateName(s.rd.state()));
    CHECK(maxLag < 20.0f, "lost the root by %.1f", maxLag);
}

static void test_arm_hit_moves_arm_independently()
{
    printf("arm_hit_moves_arm_independently\n");
    Sim s;
    for ( int i = 0; i < 60; ++i ) s.advance(1.0f / 60.0f);
    Vec3 pelvisBefore = s.rd.output().bonePos[PART_PELVIS];
    s.rd.applyImpulse(PART_FOREARM_L, s.rd.segment(PART_FOREARM_L).pos, v3(-12, 0, 6));
    float maxDev = 0, maxPelvis = 0;
    for ( int i = 0; i < 30; ++i )
    {
        s.advance(1.0f / 60.0f);
        float dev = s.jointDeviation(PART_FOREARM_L);
        if ( dev > maxDev ) maxDev = dev;
        float pm = length(s.rd.output().bonePos[PART_PELVIS] - pelvisBefore);
        if ( pm > maxPelvis ) maxPelvis = pm;
    }
    for ( int i = 0; i < 90; ++i ) s.advance(1.0f / 60.0f);
    float devAfter = s.jointDeviation(PART_FOREARM_L);
    printf("  elbow deviation peak %.2f rad, after 1.5 s %.2f rad, pelvis moved %.2f u, falls %d\n", maxDev, devAfter, maxPelvis, s.rd.falls());
    CHECK(maxDev > 0.25f, "the arm did not react: %.2f", maxDev);
    CHECK(devAfter < 0.15f, "the arm did not come back to the animation: %.2f", devAfter);
    CHECK(maxPelvis < 3.0f, "a forearm hit moved the pelvis %.2f", maxPelvis);
    CHECK(s.rd.falls() == 0, "fell");
}

static void test_leg_hit_knee_bends_no_fall()
{
    printf("leg_hit_knee_bends_no_fall\n");
    Sim s;
    for ( int i = 0; i < 60; ++i ) s.advance(1.0f / 60.0f);
    s.rd.applyImpulse(PART_SHIN_L, s.rd.segment(PART_SHIN_L).pos, v3(-30, 0, 0));
    float maxKnee = 0, minH = s.rd.pelvisHeight();
    for ( int i = 0; i < 120; ++i )
    {
        s.advance(1.0f / 60.0f);
        float k = s.jointDeviation(PART_SHIN_L);
        if ( k > maxKnee ) maxKnee = k;
        if ( s.rd.pelvisHeight() < minH ) minH = s.rd.pelvisHeight();
    }
    printf("  knee deviation peak %.2f rad, min pelvis %.1f, stumbles %d falls %d, state %s\n", maxKnee, minH, s.rd.stumbles(), s.rd.falls(), stateName(s.rd.state()));
    CHECK(maxKnee > 0.2f, "knee did not give: %.2f", maxKnee);
    CHECK(s.rd.falls() == 0, "fell from a leg hit of 30");
}

static void test_shotgun_class_hit_falls()
{
    printf("shotgun_class_hit_falls\n");
    Sim s(60.0f, 9.0f);
    for ( int i = 0; i < 60; ++i ) s.advance(1.0f / 60.0f);
    s.rd.applyImpulse(PART_TORSO_UPPER, s.rd.segment(PART_TORSO_UPPER).pos, v3(-260, 0, 40));
    bool fell = false;
    for ( int i = 0; i < 120; ++i ) { s.advance(1.0f / 60.0f); if ( s.rd.state() == STATE_FALLEN ) fell = true; }
    printf("  fell %d\n", fell);
    CHECK(fell, "a 260 chest hit while running did not knock the body down");
}

static void test_accumulated_hits_fall()
{
    printf("accumulated_hits_fall\n");
    Sim s(60.0f, 9.0f);
    for ( int i = 0; i < 60; ++i ) s.advance(1.0f / 60.0f);
    bool fell = false;
    int hits = 0;
    for ( int i = 0; i < 240 && !fell; ++i )
    {
        if ( i % 6 == 0 ) { s.rd.applyImpulse(PART_TORSO_UPPER, s.rd.segment(PART_TORSO_UPPER).pos, v3(-45, 0, 8)); ++hits; }
        s.advance(1.0f / 60.0f);
        if ( s.rd.state() == STATE_FALLEN ) fell = true;
    }
    printf("  fell after %d hits of 45 (one would not)\n", hits);
    CHECK(fell, "rapid hits never accumulated into a fall");
    CHECK(hits >= 3, "fell too early: %d hits", hits);
}

static void test_no_nan_random_hits()
{
    printf("no_nan_random_hits\n");
    Sim s(45.0f, 8.0f);
    srand(7);
    bool finite = true;
    int frames = 0;
    for ( int i = 0; i < 60 * 25; ++i )
    {
        if ( i % 20 == 0 )
        {
            int part = rand() % PART_COUNT;
            Vec3 J = v3((rand() % 400) - 200.0f, (rand() % 400) - 200.0f, (rand() % 100) - 20.0f);
            s.rd.applyImpulse(part, s.rd.segment(part).pos + v3(0, 0, (rand() % 10) - 5.0f), J);
        }
        s.advance(1.0f / 60.0f);
        ++frames;
        if ( !s.allFinite() ) { finite = false; break; }
    }
    printf("  %d frames, finite %d, falls %d stumbles %d, state %s\n", frames, finite, s.rd.falls(), s.rd.stumbles(), stateName(s.rd.state()));
    CHECK(finite, "NaN after %d frames", frames);
}

static void test_determinism_and_cost()
{
    printf("determinism_and_cost\n");
    Output a, b;
    {
        Sim s(60.0f, 9.0f);
        for ( int i = 0; i < 120; ++i ) { if ( i == 30 ) s.rd.applyImpulse(PART_HEAD, s.rd.segment(PART_HEAD).pos, v3(-40, 10, 0)); s.advance(1.0f / 60.0f); }
        a = s.rd.output();
    }
    {
        Sim s(60.0f, 9.0f);
        for ( int i = 0; i < 120; ++i ) { if ( i == 30 ) s.rd.applyImpulse(PART_HEAD, s.rd.segment(PART_HEAD).pos, v3(-40, 10, 0)); s.advance(1.0f / 60.0f); }
        b = s.rd.output();
    }
    CHECK(memcmp(&a, &b, sizeof(a)) == 0, "two identical runs differ");
    Sim s(60.0f, 9.0f);
    auto t0 = std::chrono::steady_clock::now();
    const int N = 3000;
    for ( int i = 0; i < N; ++i ) s.advance(1.0f / 60.0f);
    auto t1 = std::chrono::steady_clock::now();
    double us = std::chrono::duration<double, std::micro>(t1 - t0).count() / N;
    printf("  %.1f us per 60 Hz step (%d substeps) on this machine: ~%.0f bodies per ms\n", us, 3, 1000.0 / us);
}

static void test_balance_model()
{
    printf("balance_model (server reduced model)\n");
    BalanceModel m;
    BalanceParams p = defaultBalanceParams();
    m.init(p, 38.0f);
    // walking along +x at 60 u/s; a sideways shove of 25 u/s: stumble, no fall
    for ( int i = 0; i < 20; ++i ) m.step(0.05f);
    m.applyImpulse(v3(0, 25, 0));
    float maxOff = 0; bool fell = false;
    for ( int i = 0; i < 60; ++i ) { m.step(0.05f); float o = length(m.offset()); if ( o > maxOff ) maxOff = o; if ( m.fallen() ) fell = true; }
    printf("  shove 25: max offset %.1f, fell %d, stumbles %d, settled offset %.2f\n", maxOff, fell, m.stumbles(), length(m.offset()));
    CHECK(maxOff > 2.0f, "no reaction");
    CHECK(!fell, "fell from 25");
    CHECK(length(m.offset()) < 1.5f, "did not settle: %.2f", length(m.offset()));
    // 140 u/s: falls, then gets up
    m.applyImpulse(v3(-140, 0, 0));
    bool f2 = false, up = false;
    for ( int i = 0; i < 120; ++i ) { m.step(0.05f); if ( m.fallen() ) f2 = true; if ( f2 && !m.fallen() && m.getupBlend() >= 1.0f ) up = true; }
    printf("  shove 140: fell %d, back up %d, falls %d\n", f2, up, m.falls());
    CHECK(f2, "did not fall from 140");
    CHECK(up, "did not get up");
    // accumulation: 6 hits of 40 within a second fall, one does not
    BalanceModel m2; m2.init(p, 38.0f);
    m2.applyImpulse(v3(-40, 0, 0)); bool f3 = false;
    for ( int i = 0; i < 40; ++i ) { m2.step(0.05f); if ( m2.fallen() ) f3 = true; }
    CHECK(!f3, "one hit of 40 fell");
    bool f4 = false;
    for ( int i = 0; i < 40; ++i ) { if ( i < 12 && i % 2 == 0 ) m2.applyImpulse(v3(-40, 0, 0)); m2.step(0.05f); if ( m2.fallen() ) f4 = true; }
    printf("  one hit of 40 fell %d, six fast hits fell %d\n", f3, f4);
    CHECK(f4, "six fast hits of 40 did not fall");
}

static void test_axes_quat()
{
    printf("axes_quat (pose axis -> quaternion)\n");
    // a rotation about an arbitrary axis: its basis vectors must round-trip through qFromAxes
    Quat q = qFromAxisAngle(v3(0.3f, -0.5f, 0.8f), 1.1f);
    Vec3 ax = rotate(q, v3(1, 0, 0)), ay = rotate(q, v3(0, 1, 0)), az = rotate(q, v3(0, 0, 1));
    Quat r = qFromAxes(ax, ay, az);
    Vec3 t = rotate(r, v3(0.2f, -0.7f, 0.4f)), u = rotate(q, v3(0.2f, -0.7f, 0.4f));
    printf("  round trip error %.6f\n", length(t - u));
    CHECK(length(t - u) < 1e-4f, "qFromAxes mismatch %.6f", length(t - u));
}

static void test_shot_shock_spin()
{
    printf("shot_shock_spin (a hit off the centre line spins the torso, then it settles)\n");
    Sim s;
    for ( int i = 0; i < 60; ++i ) s.advance(1.0f / 60.0f);
    // a bullet from the front into the right shoulder area of the upper torso
    Vec3 p = s.rd.segment(PART_TORSO_UPPER).pos + v3(2, -7, 2);
    s.rd.applyImpulse(PART_TORSO_UPPER, p, v3(-55, 0, 0));
    float peak = 0, late = 0;
    for ( int i = 0; i < 90; ++i )
    {
        s.advance(1.0f / 60.0f);
        float w = fabsf(s.rd.segment(PART_TORSO_UPPER).omega.z);
        if ( i < 12 && w > peak ) peak = w;
        if ( i >= 60 && w > late ) late = w;
    }
    printf("  torso yaw rate peak %.2f rad/s (first 0.2 s), max after 1 s %.2f, falls %d\n", peak, late, s.rd.falls());
    CHECK(peak > 0.8f, "no shock spin: %.2f", peak);
    CHECK(late < 1.2f, "the spin did not settle: %.2f", late);
    CHECK(s.rd.falls() == 0, "fell");
}

static void test_shot_spine_pain_and_recovery()
{
    printf("shot_spine_pain (the torso folds around a gut shot and straightens again)\n");
    Sim s;
    for ( int i = 0; i < 60; ++i ) s.advance(1.0f / 60.0f);
    s.rd.applyImpulse(PART_TORSO_LOWER, s.rd.segment(PART_TORSO_LOWER).pos + v3(5, 0, 0), v3(-70, 0, 0));
    float peak = 0;
    for ( int i = 0; i < 30; ++i ) { s.advance(1.0f / 60.0f); float d = s.jointDeviation(PART_TORSO_LOWER) + s.jointDeviation(PART_TORSO_UPPER); if ( d > peak ) peak = d; }
    for ( int i = 0; i < 90; ++i ) s.advance(1.0f / 60.0f);
    float after = s.jointDeviation(PART_TORSO_LOWER) + s.jointDeviation(PART_TORSO_UPPER);
    printf("  spine deviation peak %.2f rad, after 2 s %.2f, falls %d\n", peak, after, s.rd.falls());
    CHECK(peak > 0.12f, "the spine did not bend: %.2f", peak);
    CHECK(after < 0.08f, "the spine did not straighten: %.2f", after);
    CHECK(s.rd.falls() == 0, "fell");
}

static void test_shot_reach_for_wound()
{
    printf("shot_reach_for_wound (a hand goes to the wound and comes back)\n");
    Sim s;
    for ( int i = 0; i < 60; ++i ) s.advance(1.0f / 60.0f);
    Vec3 wound = s.rd.segment(PART_TORSO_LOWER).pos + v3(6, 2, 0);
    s.rd.applyImpulse(PART_TORSO_LOWER, wound, v3(-60, 0, 0));
    float d0 = length(s.rd.reachingHand() - s.rd.woundPoint());
    float dmin = d0;
    for ( int i = 0; i < 60; ++i ) { s.advance(1.0f / 60.0f); if ( s.rd.woundActive() ) { float d = length(s.rd.reachingHand() - s.rd.woundPoint()); if ( d < dmin ) dmin = d; } }
    for ( int i = 0; i < 120; ++i ) s.advance(1.0f / 60.0f);
    float handDev = s.jointDeviation(PART_FOREARM_L) + s.jointDeviation(PART_FOREARM_R) + s.jointDeviation(PART_UPPER_ARM_L) + s.jointDeviation(PART_UPPER_ARM_R);
    printf("  hand to wound %.1f -> %.1f u, arms back to the animation within %.2f rad, falls %d\n", d0, dmin, handDev, s.rd.falls());
    CHECK(dmin < d0 * 0.6f, "the hand did not reach: %.1f of %.1f", dmin, d0);
    CHECK(handDev < 0.3f, "the arms stayed off the animation: %.2f", handDev);
    CHECK(s.rd.falls() == 0, "fell");
}

static void test_shot_wounded_leg_gives()
{
    printf("shot_wounded_leg (a leg hit buckles that knee, the body dips and stays up)\n");
    Sim s;
    for ( int i = 0; i < 60; ++i ) s.advance(1.0f / 60.0f);
    s.rd.applyImpulse(PART_SHIN_R, s.rd.segment(PART_SHIN_R).pos, v3(-30, 0, 0));
    float kneeR = 0, kneeL = 0, minH = s.rd.pelvisHeight();
    for ( int i = 0; i < 60; ++i )
    {
        s.advance(1.0f / 60.0f);
        float r = s.jointDeviation(PART_SHIN_R), l = s.jointDeviation(PART_SHIN_L);
        if ( r > kneeR ) kneeR = r;
        if ( l > kneeL ) kneeL = l;
        if ( s.rd.pelvisHeight() < minH ) minH = s.rd.pelvisHeight();
    }
    for ( int i = 0; i < 90; ++i ) s.advance(1.0f / 60.0f);
    printf("  right knee %.2f rad, left knee %.2f, pelvis dipped to %.1f of %.1f, state %s, falls %d\n", kneeR, kneeL, minH, s.rd.standingPelvisHeight(), stateName(s.rd.state()), s.rd.falls());
    CHECK(kneeR > 0.3f, "the wounded knee did not give: %.2f", kneeR);
    CHECK(kneeR > kneeL * 1.5f, "the other knee gave as much: %.2f vs %.2f", kneeL, kneeR);
    CHECK(minH < s.rd.standingPelvisHeight() * 0.985f, "no dip");
    CHECK(s.rd.falls() == 0 && s.rd.state() == STATE_STANDING, "did not stay up / recover");
}

static void test_catch_fall_arms()
{
    printf("catch_fall (arms go towards the ground in the fall direction)\n");
    Sim s;
    for ( int i = 0; i < 60; ++i ) s.advance(1.0f / 60.0f);
    s.rd.applyImpulse(PART_TORSO_UPPER, s.rd.segment(PART_TORSO_UPPER).pos, v3(-300, 0, 40));
    bool caught = false, armsForward = false;
    for ( int i = 0; i < 90 && !armsForward; ++i )
    {
        s.advance(1.0f / 60.0f);
        if ( !s.rd.catchFalling() ) continue;
        caught = true;
        Vec3 fallDir = normalize(v3(s.rd.comOffset().x, s.rd.comOffset().y, 0));
        for ( int a = 0; a < 2; ++a )
        {
            int fore = a ? PART_FOREARM_R : PART_FOREARM_L, upper = a ? PART_UPPER_ARM_R : PART_UPPER_ARM_L;
            const Segment &f = s.rd.segment(fore);
            Vec3 hand = f.pos + rotate(f.q, f.axisLocal) * (f.length * 0.5f);
            Vec3 shoulder = s.rd.segment(upper).pos - rotate(s.rd.segment(upper).q, s.rd.segment(upper).comOffsetLocal);
            if ( dot(hand - shoulder, fallDir) > 4.0f ) armsForward = true;
        }
    }
    printf("  catch fall engaged %d, a hand ahead of its shoulder in the fall direction %d, falls %d\n", caught, armsForward, s.rd.falls());
    CHECK(caught, "catch fall never engaged");
    CHECK(armsForward, "the arms did not go towards the fall");
}

int main()
{
    test_axes_quat();
    test_shot_shock_spin();
    test_shot_spine_pain_and_recovery();
    test_shot_reach_for_wound();
    test_shot_wounded_leg_gives();
    test_catch_fall_arms();
    test_standing_stays_up();
    test_small_hit_recovers();
    test_big_hit_falls_and_gets_up();
    test_walking_follows_root();
    test_walking_hit_recovers_and_keeps_walking();
    test_arm_hit_moves_arm_independently();
    test_leg_hit_knee_bends_no_fall();
    test_shotgun_class_hit_falls();
    test_accumulated_hits_fall();
    test_no_nan_random_hits();
    test_determinism_and_cost();
    test_balance_model();
    printf(g_fail ? "\n%d FAILED\n" : "\nALL PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
