#pragma once
// mod: euphoria (mods/euphoria) - CLIENT side of the active-ragdoll zombies. NOT part of the original game.
//
// For every actor the server flags (animState.fLeanAmount > 0, set by game_sp/actor_sp_euphoria.cpp) and that is close
// enough to the camera, the client runs the full-body simulation (src/euphoria/euphoria_body.h): 14 rigid segments on
// the zombie's own bones, joints with limits, motors tracking the retail animation frame by frame, a balance controller,
// and impulses from the bullet hit events the server already sends (EV_BULLET_HIT with the hit bone, the weapon and the
// bullet's start). The server's balance state and offset bias the body so a fall happens when the server says. The
// result replaces the animated bones in the model's skeleton through the controller hook (DObjSetSkelRotTransIndex,
// as the retail death ragdoll does). Beyond the distance limit, or when the slots are used up, the zombie shows the
// retail animation plus the server's entity tilt.
struct cpose_t;
struct DObj;

void CG_Euphoria_Controllers(const cpose_t *pose, const DObj *obj, int *partBits);
void CG_Euphoria_BulletHit(int localClientNum, int targetEntityNum, int weaponIndex, const float *startPos, const float *position, int damage, unsigned char boneIndex);
