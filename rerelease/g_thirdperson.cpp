/**
 * g_thirdperson.cpp
 * 
 * Advanced third-person camera implementation for Quake 2 Rerelease
 * Provides smooth camera movement, collision detection, and player avatar visualization
 */

 #include "g_thirdperson.h"
 #include "q_vec3.h"
 #include "game.h"
 #include <cmath>
 #include <algorithm>
 
 /**
  * Interpolates between two float values
  * 
  * @param a Starting value
  * @param b Target value
  * @param t Interpolation factor (0.0 to 1.0)
  * @return Interpolated value
  */
 static inline float Lerp(float a, float b, float t) {
     return a + (b - a) * t;
 }
 
 /**
  * Component-wise linear interpolation for vectors
  * 
  * @param a Starting vector
  * @param b Target vector
  * @param t Interpolation factor (0.0 to 1.0)
  * @return Interpolated vector
  */
 static inline vec3_t LerpVec(const vec3_t &a, const vec3_t &b, float t) {
     vec3_t result;
     result.x = Lerp(a.x, b.x, t);
     result.y = Lerp(a.y, b.y, t);
     result.z = Lerp(a.z, b.z, t);
     return result;
 }
 
 /**
  * Linear interpolation for angles with wrap-around handling
  * 
  * @param a Starting angles
  * @param b Target angles
  * @param t Interpolation factor (0.0 to 1.0)
  * @return Interpolated angles
  */
 static inline vec3_t LerpAngles(const vec3_t &a, const vec3_t &b, float t) {
     vec3_t result;
     result.x = Lerp(a.x, b.x, t);
     result.y = Lerp(a.y, b.y, t);
     result.z = Lerp(a.z, b.z, t);
     return result;
 }
 
 /**
  * Create visual representation entity for third-person view
  * 
  * This creates a separate entity called "playerAvatar" that represents 
  * the player's model visually while the original player entity remains
  * invisible and controls game logic.
  * 
  * @param ent Player entity
  * @return Pointer to the newly created avatar entity
  */
 static edict_t* CreatePlayerAvatar(edict_t *ent) {
     // Create a new entity for visual representation
     edict_t *avatar = G_Spawn();
     
     // Configure basic properties
     avatar->classname = "playerAvatar";   // Descriptive classname
     avatar->owner = ent;                  // Reference to controlling entity
     avatar->solid = SOLID_BBOX;           // Collision type
     avatar->movetype = MOVETYPE_NONE;     // No independent movement
     
     // Copy collision boundaries
     avatar->mins[0] = ent->mins[0];
     avatar->mins[1] = ent->mins[1];
     avatar->mins[2] = ent->mins[2];
     
     avatar->maxs[0] = ent->maxs[0];
     avatar->maxs[1] = ent->maxs[1];
     avatar->maxs[2] = ent->maxs[2];
     
     // Copy entity state from player
     avatar->s = ent->s;
     avatar->s.modelindex = 255;  // Player model index
     
     // Register with the game world
     gi.linkentity(avatar);
     
     return avatar;
 }
 
 /**
  * Update the player's avatar entity to match current state
  * 
  * This function synchronizes the avatar entity with the player's current
  * position, animation state, and effects while adjusting angles for
  * proper third-person appearance.
  * 
  * @param ent Player entity
  */
 static void UpdatePlayerAvatar(edict_t *ent) {
     if (!ent->client->playerAvatar)
         return;
     
     // Copy full entity state (animations, effects, etc.)
     ent->client->playerAvatar->s = ent->s;
     ent->client->playerAvatar->s.modelindex = 255;  // Ensure correct model index
     
     // Keep reference to owner entity
     ent->client->playerAvatar->owner = ent;
     
     // Apply proper rotation for third-person appearance
     // Copy yaw (horizontal rotation) for proper facing direction
     ent->client->playerAvatar->s.angles[YAW] = ent->client->v_angle[YAW];
     
     // Reset pitch and roll for natural appearance
     ent->client->playerAvatar->s.angles[PITCH] = 0;
     ent->client->playerAvatar->s.angles[ROLL] = 0;
     
     // Update in game world
     gi.linkentity(ent->client->playerAvatar);
 }
 
 /**
  * Calculate aim trace for weapon firing and native crosshair
  * 
  * Performs a trace from the player's eye position in the direction they're looking,
  * storing the impact point for weapon firing systems.
  * 
  * @param ent Player entity
  * @return The endpoint of the aim trace
  */
 static vec3_t CalculateAimTrace(edict_t *ent) {
     vec3_t forward, right, up;
     vec3_t start, end;
     trace_t tr;
     
     // Get view vectors from current angles
     AngleVectors(ent->client->v_angle, forward, right, up);
     
     // Start trace from eye position
     start = ent->s.origin;
     start.z += ent->viewheight;
     
     // Project forward to find target point
     end.x = start.x + forward.x * 8192;
     end.y = start.y + forward.y * 8192;
     end.z = start.z + forward.z * 8192;
     
     // Trace to find aim point
     tr = gi.traceline(start, end, ent, MASK_SHOT);
     
     // Return the impact point
     return tr.endpos;
 }
 
 /**
  * Main third-person view implementation
  * 
  * Manages camera positioning, player model visibility, and aiming in third-person view.
  * This is the core function that should be called every frame when third-person is active.
  * 
  * @param ent Player entity
  */
 void G_SetThirdPersonView(edict_t *ent) {
     // --- Early Exit Checks ---
     if (!ent || !ent->client || ent->health <= 0 || ent->client->resp.spectator)
         return;
     
     if (!sv_thirdperson || !sv_thirdperson->integer)
         return;
 
     // --- Create playerAvatar entity if it doesn't exist ---
     if (!ent->client->playerAvatar) {
         ent->client->playerAvatar = CreatePlayerAvatar(ent);
     }
     
     // --- Update playerAvatar with current player state ---
     UpdatePlayerAvatar(ent);
     
     // --- Hide the real player entity ---
     ent->svflags |= SVF_NOCLIENT;  // Make invisible to other clients
     ent->client->ps.gunindex = 0;  // Hide weapon model
 
     // --- Store the aim trace endpoint for weapon firing ---
     ent->client->thirdperson_target = CalculateAimTrace(ent);
 
     // --- Retrieve and Clamp CVAR Parameters ---
     const float distance = (tp_distance ? tp_distance->value : 64.0f);
     const float heightOffset = (tp_height ? tp_height->value : 0.0f);
     const float sideOffset = (tp_side ? tp_side->value : 0.0f);
     const float smoothFactor = (tp_smooth ? tp_smooth->value : 0.5f);
     const float minCollisionDistance = 8.0f;
     const float maxDistance = 512.0f;
 
     const float effective_distance = std::clamp(distance, 16.0f, maxDistance);
     const float effective_height = std::clamp(heightOffset, -64.0f, 128.0f);
     const float effective_side = std::clamp(sideOffset, -128.0f, 128.0f);
     const float effective_smooth = std::clamp(smoothFactor, 0.0f, 1.0f);
 
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
     // Prevent camera from going through walls and other solid objects
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
 
     // --- Client-Side Prediction Setup ---
     ent->client->ps.pmove.pm_type = PM_SPECTATOR;
     ent->client->ps.pmove.delta_angles = ent->client->v_angle - ent->client->resp.cmd_angles;
 
     // --- Link Entity Update ---
     gi.linkentity(ent);
 
     // --- Final: Revert Movement Type to Normal ---
     ent->client->ps.pmove.pm_type = PM_NORMAL;
 }
 
 /**
  * Cleanup function to remove third-person effects
  * 
  * Removes the playerAvatar entity and restores normal first-person view settings.
  * Should be called when disabling third-person view, or when the player disconnects.
  * 
  * @param ent Player entity
  */
 void G_RemoveThirdPersonView(edict_t *ent) {
     if (!ent || !ent->client)
         return;
         
     // Free the playerAvatar entity
     if (ent->client->playerAvatar) {
         G_FreeEdict(ent->client->playerAvatar);
         ent->client->playerAvatar = NULL;
     }
     
     // Restore player visibility
     ent->svflags &= ~SVF_NOCLIENT;
     
     // Restore weapon model if player is alive
     if (ent->health > 0 && ent->client->pers.weapon) {
         ent->client->ps.gunindex = gi.modelindex(ent->client->pers.weapon->view_model);
     }
 }
 
/**
 * Command handler for toggling third-person view
 *
 * Allows players to switch between first-person and third-person views via console command.
 * 
 * @param ent Player entity that issued the command
 */
void G_Thirdperson_Command(edict_t* ent) {
    // Sanity checks
    if (!ent || !ent->client)
        return;
        
    // Toggle third-person view
    if (sv_thirdperson->integer) {
        // Disable third-person
        gi.cvar_set("sv_thirdperson", "0");
        G_RemoveThirdPersonView(ent);
        // gi.cprintf(ent, PRINT_HIGH, "Third-person view disabled\n");
    } else {
        // Enable third-person
        gi.cvar_set("sv_thirdperson", "1");
        // gi.cprintf(ent, PRINT_HIGH, "Third-person view enabled\n");
    }
}
 
 /**
  * Adjust weapon firing direction for third-person
  * 
  * This function should be called from weapon firing functions to correct
  * the aiming direction when in third-person view.
  * 
  * @param self Player entity
  * @param start Firing start position
  * @param aimdir Direction vector (will be modified)
  */
 void G_AdjustThirdPersonAim(edict_t *self, vec3_t &start, vec3_t &aimdir) {
     // Only adjust if in third-person mode
     if (!sv_thirdperson || !sv_thirdperson->integer || !self || !self->client)
         return;
     
     // Create vector from start position to target point
     vec3_t dir;
     dir.x = self->client->thirdperson_target.x - start.x;
     dir.y = self->client->thirdperson_target.y - start.y;
     dir.z = self->client->thirdperson_target.z - start.z;
     
     // Normalize to get direction
     float length = sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
     if (length > 0) {
         // Replace provided direction with more accurate one
         aimdir.x = dir.x / length;
         aimdir.y = dir.y / length;
         aimdir.z = dir.z / length;
     }
 }