// Copyright (c) Ericson Willians.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "q_vec3.h"  // Provides our vec3_t definition and vector operators

/*
==============
G_SetThirdPersonView
==============
Supreme and safe third-person view calculation.
This version computes a desired camera position behind the player,
traces from the player’s eye (origin + viewheight) to that position,
and then sets the client’s view offset based on the trace result.
==============
*/
void G_SetThirdPersonView(edict_t *ent)
{
    // Validate input and skip if player is dead or spectating.
    if (!ent || !ent->client)
        return;
    if (ent->health <= 0 || ent->client->resp.spectator)
        return;

    // Compute the starting point: player's origin plus view height.
    vec3_t ownerOrigin = ent->s.origin;
    ownerOrigin.z += ent->viewheight;

    // Compute the forward, right, and up vectors from the player's view angles.
    vec3_t forward, right, up;
    AngleVectors(ent->client->v_angle, forward, right, up);
    forward.normalize();  // Normalize the forward vector

    // Set desired camera parameters.
    const float distance = 60.0f;    // How far behind the player.
    const float heightOffset = 20.0f;  // Additional vertical offset.

    // Calculate the desired camera position: behind the player along the forward vector.
    // desiredPos = ownerOrigin - (forward * distance), then add height offset to z.
    vec3_t desiredPos = ownerOrigin - (forward * distance);
    desiredPos.z += heightOffset;

    // Trace a line from the owner's eye position to the desired camera position
    // to ensure the camera does not clip into world geometry.
    trace_t trace = gi.traceline(ownerOrigin, desiredPos, ent, MASK_SOLID);
    vec3_t finalPos = trace.endpos;

    // Optionally, push the camera a little further back if space allows.
    finalPos = finalPos + (forward * 2.0f);

    // Set the view offset to be the difference between the final camera position
    // and the player's origin. This offset is relative to the player.
    vec3_t viewOffset = finalPos - ent->s.origin;
    ent->client->ps.viewoffset = viewOffset;  // Uses overloaded operator=

    // Update view angles (here, simply copying the player's current view angles).
    ent->client->ps.viewangles = ent->client->v_angle;

    // Link the entity to update its state.
    gi.linkentity(ent);
}
