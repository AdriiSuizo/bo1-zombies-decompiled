// mod: euphoria - the reduced server balance model, see euphoria_balance.h. NOT part of the original game.
#include "euphoria_balance.h"
#include <cstring>

namespace euphoria {

BalanceParams defaultBalanceParams()
{
    BalanceParams p;
    p.gravity = 800.0f;
    p.supportRadius = 9.0f;
    p.stepRadius = 26.0f;
    p.ankleAccel = 320.0f;
    p.ankleGain = 25.0f;
    p.damping = 3.0f;
    p.stepCooldown = 0.35f;
    p.getupDelay = 1.2f;
    p.getupTime = 1.1f;
    p.impulseScale = 1.0f;
    return p;
}

BalanceModel::BalanceModel()
{
    memset(this, 0, sizeof(*this));
    m_params = defaultBalanceParams();
    m_height = 38.0f;
    m_stability = 1.0f;
}

void BalanceModel::init(const BalanceParams &p, float comHeight)
{
    m_params = p;
    m_height = comHeight > 4.0f ? comHeight : 4.0f;
    m_pos = v3(0, 0, 0);
    m_vel = v3(0, 0, 0);
    m_cp = v3(0, 0, 0);
    m_fallDir = v3(1, 0, 0);
    m_stability = 1.0f;
    m_stepTimer = 0.0f;
    m_stateTime = 0.0f;
    m_state = BALANCE_STANDING;
    m_stumbles = 0;
    m_falls = 0;
}

void BalanceModel::applyImpulse(Vec3 dv)
{
    if ( m_state == BALANCE_FALLEN )
        return;
    dv.z = 0.0f;
    m_vel += dv * m_params.impulseScale;
    if ( m_state == BALANCE_GETTING_UP )
    {
        // a hit while getting up puts it back down for a moment
        if ( length(dv) > 30.0f )
        {
            m_state = BALANCE_FALLEN;
            m_stateTime = m_params.getupDelay * 0.5f;
        }
    }
}

float BalanceModel::getupBlend() const
{
    if ( m_state == BALANCE_FALLEN )
        return 0.0f;
    if ( m_state == BALANCE_GETTING_UP )
        return clampf(m_stateTime / m_params.getupTime, 0.0f, 1.0f);
    return 1.0f;
}

void BalanceModel::step(float dt)
{
    if ( dt <= 0.0f )
        return;
    if ( dt > 0.1f )
        dt = 0.1f;
    m_stateTime += dt;
    if ( m_stepTimer > 0.0f )
        m_stepTimer -= dt;
    switch ( m_state )
    {
    case BALANCE_FALLEN:
        m_pos = v3(0, 0, 0);
        m_vel = v3(0, 0, 0);
        m_cp = v3(0, 0, 0);
        m_stability = 0.0f;
        if ( m_stateTime > m_params.getupDelay )
        {
            m_state = BALANCE_GETTING_UP;
            m_stateTime = 0.0f;
        }
        return;
    case BALANCE_GETTING_UP:
        // the pendulum is held while getting up; stability rises with the blend
        m_vel = m_vel * clampf(1.0f - 6.0f * dt, 0.0f, 1.0f);
        m_pos = m_pos * clampf(1.0f - 6.0f * dt, 0.0f, 1.0f);
        m_cp = m_pos;
        m_stability = getupBlend();
        if ( m_stateTime > m_params.getupTime )
        {
            m_state = BALANCE_STANDING;
            m_stateTime = 0.0f;
            m_stability = 1.0f;
        }
        return;
    default:
        break;
    }
    float tau = sqrtf(m_height / m_params.gravity);
    float w2 = m_params.gravity / m_height;
    // controller: push the capture point back to the support, limited like an ankle
    Vec3 cp = m_pos + m_vel * tau;
    m_cp = cp; // the offset a hit produced, before the ankle answers it (what the client is told)
    Vec3 u = cp * m_params.ankleGain + m_vel * m_params.damping;
    float ul = length(u);
    // mid-step (one foot in the air) the ankle has less to push with and the body is easier to topple
    float ankle = m_stepTimer > 0.0f ? m_params.ankleAccel * 0.4f : m_params.ankleAccel;
    float fallRadius = m_stepTimer > 0.0f ? m_params.stepRadius * 0.6f : m_params.stepRadius;
    if ( ul > ankle )
        u = u * (ankle / ul);
    // x'' = w2 x - u (an inverted pendulum falls away from the support)
    Vec3 acc = m_pos * w2 - u;
    acc.z = 0.0f;
    m_vel += acc * dt;
    m_pos += m_vel * dt;
    m_pos.z = 0.0f;
    cp = m_pos + m_vel * tau;
    float dist = length(cp);
    if ( dist > length(m_cp) )
        m_cp = cp;
    m_stability = clampf(1.0f - (dist - m_params.supportRadius) / (m_params.stepRadius - m_params.supportRadius), 0.0f, 1.0f);
    if ( dist > fallRadius )
    {
        m_state = BALANCE_FALLEN;
        m_stateTime = 0.0f;
        m_fallDir = dist > 1e-4f ? cp * (1.0f / dist) : v3(1, 0, 0);
        ++m_falls;
        return;
    }
    if ( dist > m_params.supportRadius )
    {
        if ( m_state == BALANCE_STANDING )
        {
            m_state = BALANCE_STUMBLING;
            m_stateTime = 0.0f;
            ++m_stumbles;
        }
        if ( m_stepTimer <= 0.0f )
        {
            // a recovery step: the support moves under the capture point; the body keeps some of the sway
            m_pos -= cp * 0.85f;
            m_stepTimer = m_params.stepCooldown;
        }
    }
    else if ( m_state == BALANCE_STUMBLING && dist < m_params.supportRadius * 0.5f && m_stateTime > 0.5f )
    {
        m_state = BALANCE_STANDING;
        m_stateTime = 0.0f;
    }
}

} // namespace euphoria
