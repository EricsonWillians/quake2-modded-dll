// g_thirdperson.cpp
// Copyright (c) Ericson Willians.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "q_vec3.h"  // Provides our vec3_t definition and vector operators
#include "game.h"   // For entity_state_t

/*
==============
G_SetThirdPersonView
==============
*/

void G_SetThirdPersonView(edict_t *ent) {
    // --- Robustness Checks ---
    if (!ent || !ent->client) {
        return;
    }
    // Early exit if dead, spectating.  This prevents camera glitches.
    if (ent->health <= 0 || ent->client->resp.spectator)
    {
        return;
    }
    if (!sv_thirdperson || !sv_thirdperson->integer)
    {
        return;
    }

    // --- Customizable Configuration (Read from CVARs) ---
    const float distance = clampval(tp_distance->value, 16.0f, 512.0f);  // Prevent extreme values
    const float heightOffset = clampval(tp_height->value, -64.0f, 128.0f); // Allow going below the player
    const float sideOffset = clampval(tp_side->value, -128.0f, 128.0f);
    const float cameraSmoothingFactor = clampval(tp_smooth->value, 0.0f, 1.0f); //0 to 1 range.
    const float angleSmoothingFactor = clampval(tp_smooth->value, 0.0f, 1.0f); //Use the same cvar for angle smooth
    const float minCollisionDistance = 8.0f; // Minimum distance from walls

    // --- Camera Positioning ---

    // 1. Calculate the *ideal* camera position (no collision checks yet).
    vec3_t forward, right, up;
    AngleVectors(ent->client->v_angle, forward, right, up); // Use player's *input* angles

    vec3_t ownerOrigin = ent->s.origin;
    ownerOrigin.z += ent->viewheight;  // Start at eye level

    vec3_t desiredPos = ownerOrigin - (forward * distance) + (right * sideOffset);
    desiredPos.z += heightOffset;

    // --- Collision Detection ---

    // 2. Perform a trace from the player's eyes to the desired camera position.
    trace_t trace = gi.traceline(ownerOrigin, desiredPos, ent, MASK_SOLID);
    // 3. Adjust camera position based on collision, maintaining minimum distance.
    vec3_t goal = trace.endpos;
     if (trace.fraction < 1.0f)
    {
        // Calculate distance and direction to the obstruction.
        const vec3_t obstructionDir = goal - ownerOrigin;
        const float obstructionDist = obstructionDir.length();
        // Prevent camera to glitch inside the player model when colliding.
        if(obstructionDist <= minCollisionDistance)
        {
           goal = ownerOrigin;
        }
    }

    // --- Smoothing (Optional) ---
    vec3_t newViewOffset;
    // Apply smoothing, unless the factor is 0
    if (cameraSmoothingFactor > 0.0f) {
        vec3_t currentViewOffset = ent->client->ps.viewoffset;
        newViewOffset = goal - ent->s.origin; // Calculate offset from the entity origin
        ent->client->ps.viewoffset = currentViewOffset + (newViewOffset - currentViewOffset) * cameraSmoothingFactor;
    } else {
        // No smoothing: directly set the view offset
        ent->client->ps.viewoffset = goal - ent->s.origin;
    }

    // --- Angle Handling (Smooth Rotation) ---

    // 1. Calculate current and desired camera directions.
    vec3_t currentAngles = ent->client->ps.viewangles;
    vec3_t desiredAngles = ent->client->v_angle;  // Use player's *input* angles

    vec3_t currentDir, desiredDir;
    AngleVectors(currentAngles, currentDir, nullptr, nullptr);
    AngleVectors(desiredAngles, desiredDir, nullptr, nullptr);

    // 2.  Use spherical linear interpolation (slerp) for smooth angle transitions.
    vec3_t slerpedDir;
    if(angleSmoothingFactor > 0.0f)
    {
        slerpedDir = slerp(currentDir, desiredDir, angleSmoothingFactor);
        slerpedDir.normalize(); // Normalize to ensure a unit vector
    }
    else
    {
       slerpedDir = desiredDir; //If no smoothing, do not slerp
    }

    // 3. Convert the slerped direction back to angles.
     ent->client->ps.viewangles = vectoangles(slerpedDir);

    // 4. *Crucially* synchronize the player model's orientation with the camera.
    ent->s.angles = ent->client->ps.viewangles;
    ent->s.angles[PITCH] = 0; // Player model should not pitch.
    ent->s.angles[ROLL] = 0;  // Player model should not roll.

    // --- Weapon and Model Visibility ---

    // Hide first-person weapon model and hands (essential for third-person).
    ent->client->ps.gunindex = 0;
    ent->client->ps.rdflags |= RDF_NOWORLDMODEL;

    // --- Client Prediction ---
    // Update delta_angles for client-side prediction.
    ent->client->ps.pmove.delta_angles = ent->client->v_angle - ent->client->resp.cmd_angles;

    // --- Link Entity ---
    gi.linkentity(ent); // *Always* link the entity after modifying its position/angles.
}