#pragma once
// mod: euphoria - the reduced balance model the SERVER runs per actor (NOT part of the original game).
//
// A linear inverted pendulum of the actor's centre of mass over its support, in the ground plane: the same capture
// point rule as the full body (euphoria_body.h) but with one point mass, so it costs nothing per zombie and is
// deterministic enough to be the authority: it decides stumbles, falls and get-ups, and its offset is networked to
// the client, which biases its full-body simulation with it so both agree on when a zombie goes down.
//   - impulses from damage shift the COM velocity;
//   - an "ankle" controller accelerates the COM back under the support, up to a limit;
//   - when the capture point leaves the support the body steps (the support jumps under the capture point, a
//     stumble), but a step takes time: hits during the step cooldown accumulate, and a capture point beyond the step
//     radius is a fall;
//   - a fall lasts getupDelay, then getupTime of getting up, then the pendulum restarts upright.
#include "euphoria_math.h"

namespace euphoria {

struct BalanceParams
{
    float gravity;        // u/s^2
    float supportRadius;  // capture point inside = stable (units)
    float stepRadius;     // capture point beyond = fall (units)
    float ankleAccel;     // max restoring acceleration, u/s^2
    float ankleGain;      // 1/s^2 on the capture point
    float damping;        // 1/s
    float stepCooldown;   // s between recovery steps
    float getupDelay;     // s
    float getupTime;      // s
    float impulseScale;
};

BalanceParams defaultBalanceParams();

enum BalanceState
{
    BALANCE_STANDING = 0,
    BALANCE_STUMBLING,
    BALANCE_FALLEN,
    BALANCE_GETTING_UP
};

class BalanceModel
{
public:
    BalanceModel();
    void init(const BalanceParams &p, float comHeight);
    void setParams(const BalanceParams &p) { m_params = p; }
    // a change of the COM velocity in world units/s (xy used)
    void applyImpulse(Vec3 dv);
    void step(float dt);
    Vec3 offset() const { return m_cp; }           // capture point - support, xy (units)
    Vec3 velocity() const { return m_vel; }
    float stability() const { return m_stability; } // 1 .. 0
    BalanceState state() const { return m_state; }
    bool fallen() const { return m_state == BALANCE_FALLEN; }
    bool down() const { return m_state == BALANCE_FALLEN || m_state == BALANCE_GETTING_UP; }
    float getupBlend() const;                        // 0 on the ground .. 1 standing
    int stumbles() const { return m_stumbles; }
    int falls() const { return m_falls; }
    float stateTime() const { return m_stateTime; }
    Vec3 fallDirection() const { return m_fallDir; } // xy unit, where it went down

private:
    BalanceParams m_params;
    float m_height;
    Vec3 m_pos;       // COM relative to the support, xy
    Vec3 m_vel;
    Vec3 m_cp;
    Vec3 m_fallDir;
    float m_stability;
    float m_stepTimer;
    float m_stateTime;
    BalanceState m_state;
    int m_stumbles;
    int m_falls;
};

} // namespace euphoria
