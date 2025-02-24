// g_thirdperson.cpp
// Improved third-person camera implementation for Quake 2 Rerelease

#include "g_local.h"
#include "q_vec3.h"
#include "game.h"
#include <cmath>
#include <algorithm>

// Helper: Linear interpolation for floats
static inline float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// Helper: Component-wise linear interpolation for vectors
static inline vec3_t LerpVec(const vec3_t &a, const vec3_t &b, float t) {
    vec3_t result;
    result.x = Lerp(a.x, b.x, t);
    result.y = Lerp(a.y, b.y, t);
    result.z = Lerp(a.z, b.z, t);
    return result;
}

// Helper: Linear interpolation for angles (handles wrap-around if needed)
static inline vec3_t LerpAngles(const vec3_t &a, const vec3_t &b, float t) {
    vec3_t result;
    result.x = Lerp(a.x, b.x, t);
    result.y = Lerp(a.y, b.y, t);
    result.z = Lerp(a.z, b.z, t);
    return result;
}

void G_SetThirdPersonView(edict_t *ent) {
    // --- Early Exit Checks ---
    if (!ent || !ent->client || ent->health <= 0 || ent->client->resp.spectator)
        return;
    
    if (!sv_thirdperson || !sv_thirdperson->integer)
        return;

    // --- Force Player Model Visibility ---
    ent->svflags   &= ~SVF_NOCLIENT;
    ent->flags     &= ~FL_NOVISIBLE;
    ent->solid      = SOLID_BBOX;
    ent->clipmask   = MASK_PLAYERSOLID;
    ent->s.modelindex = 255;      // Force player model index
    ent->client->ps.gunindex = 0;   // Hide weapon model
    P_AssignClientSkinnum(ent);

    // --- Retrieve and Clamp CVAR Parameters ---
    const float distance    = (tp_distance ? tp_distance->value : 64.0f);
    const float heightOffset = (tp_height   ? tp_height->value   : 0.0f);
    const float sideOffset  = (tp_side     ? tp_side->value     : 0.0f);
    const float smoothFactor = (tp_smooth   ? tp_smooth->value   : 0.5f);
    const float minCollisionDistance = 8.0f;
    const float maxDistance = 512.0f;

    const float effective_distance = std::clamp(distance, 16.0f, maxDistance);
    const float effective_height   = std::clamp(heightOffset, -64.0f, 128.0f);
    const float effective_side     = std::clamp(sideOffset, -128.0f, 128.0f);
    const float effective_smooth   = std::clamp(smoothFactor, 0.0f, 1.0f);

    // --- Calculate Base Vectors and Player Eye Position ---
    vec3_t forward, right, up;
    AngleVectors(ent->client->v_angle, forward, right, up);

    vec3_t playerEyePos = ent->s.origin;
    playerEyePos.z += ent->viewheight;

    // --- Determine Desired Camera Position ---
    vec3_t desiredPos;
    desiredPos.x = playerEyePos.x - (forward.x * effective_distance) + (right.x * effective_side);
    desiredPos.y = playerEyePos.y - (forward.y * effective_distance) + (right.y * effective_side);
    desiredPos.z = playerEyePos.z - (forward.z * effective_distance) + effective_height;

    // --- Collision Detection ---
    trace_t trace = gi.traceline(playerEyePos, desiredPos, ent, MASK_SOLID);
    vec3_t cameraPos = trace.endpos;

    if (trace.fraction < 1.0f) {
        // Compute distance between player's eye and camera position
        float obstructionDist = std::sqrt(
            (cameraPos.x - playerEyePos.x) * (cameraPos.x - playerEyePos.x) +
            (cameraPos.y - playerEyePos.y) * (cameraPos.y - playerEyePos.y) +
            (cameraPos.z - playerEyePos.z) * (cameraPos.z - playerEyePos.z)
        );

        if (obstructionDist < minCollisionDistance) {
            // Prevent camera from clipping too close
            cameraPos = playerEyePos;
        } else {
            // Pull camera away from collision surface to avoid clipping
            const float pullbackDistance = 2.0f;
            cameraPos.x += trace.plane.normal.x * pullbackDistance;
            cameraPos.y += trace.plane.normal.y * pullbackDistance;
            cameraPos.z += trace.plane.normal.z * pullbackDistance;
        }
    }

    // --- Compute New View Offset ---
    vec3_t targetViewOffset;
    targetViewOffset.x = cameraPos.x - ent->s.origin.x;
    targetViewOffset.y = cameraPos.y - ent->s.origin.y;
    targetViewOffset.z = cameraPos.z - ent->s.origin.z;

    // Smooth the view offset transition
    ent->client->ps.viewoffset = LerpVec(ent->client->ps.viewoffset, targetViewOffset, effective_smooth);

    // --- Smooth View Angle Update ---
    vec3_t targetAngles = ent->client->v_angle;
    ent->client->ps.viewangles = LerpAngles(ent->client->ps.viewangles, targetAngles, effective_smooth);

    // --- Update Player Model Orientation ---
    ent->s.angles = ent->client->ps.viewangles;
    ent->s.angles[PITCH] = 0;
    ent->s.angles[ROLL]  = 0;

    // --- Client-Side Prediction Setup ---
    // Set PM_SPECTATOR for view update, then defer reversion until after linking
    ent->client->ps.pmove.pm_type = PM_SPECTATOR;
    ent->client->ps.pmove.delta_angles = ent->client->v_angle - ent->client->resp.cmd_angles;

    // --- Link Entity Update ---
    gi.linkentity(ent);

    // --- Final: Revert Movement Type to Normal ---
    // Delay reversion so that our custom view persists through the frame update
    ent->client->ps.pmove.pm_type = PM_NORMAL;
}
