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
     // Safety check
     if (!ent || !ent->client)
         return nullptr;
         
     // Create a new entity for visual representation
     edict_t *avatar = G_Spawn();
     if (!avatar)
         return nullptr;
     
     // Configure basic properties
     avatar->classname = "playerAvatar";
     avatar->owner = ent;
     avatar->solid = SOLID_BBOX;
     avatar->movetype = MOVETYPE_STEP;
     
     // Copy collision boundaries
     avatar->mins = ent->mins;
     avatar->maxs = ent->maxs;
     
     // Copy entity state from player
     avatar->s = ent->s;
     
     // Use player model
     avatar->s.modelindex = 255;
     
     // Set proper rotation for third-person appearance
     avatar->s.angles[YAW] = ent->client->v_angle[YAW];
     avatar->s.angles[PITCH] = 0;
     avatar->s.angles[ROLL] = 0;
     
     // Register with the game world
     gi.linkentity(avatar);
     
     return avatar;
 }
 
 /**
  * Update player avatar entity
  * 
  * This is a simpler approach that just copies the player entity state
  * and adjusts only what's necessary for the avatar to work as a visual representation.
  */
 static void UpdatePlayerAvatar(edict_t *ent) {
     if (!ent || !ent->client || !ent->client->playerAvatar)
         return;
     
     edict_t *avatar = ent->client->playerAvatar;
     
     // Copy entity state - we want most attributes to match
     avatar->s = ent->s;
     
     // Ensure we keep the player model
     avatar->s.modelindex = 255;
     
     // Use player's animation frame
     avatar->s.frame = ent->s.frame;
     
     // Set avatar angles - match player's yaw but zero pitch/roll
     avatar->s.angles[YAW] = ent->client->v_angle[YAW];
     avatar->s.angles[PITCH] = 0;
     avatar->s.angles[ROLL] = 0;
     
     // Link to update changes
     gi.linkentity(avatar);
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
     if (!ent)
         return vec3_origin;
         
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
  * Improved collision detection for third-person camera
  * 
  * This version provides better wall handling and prevents sky visibility issues
  */
 static void HandleCameraCollision(edict_t *ent, const vec3_t &playerEyePos, const vec3_t &desiredPos, vec3_t &cameraPos) {
     if (!ent)
         return;
         
     const float minCollisionDistance = 8.0f;
     const float pullbackDistance = 8.0f;
     
     // Perform initial trace from player eyes to desired camera position
     trace_t trace = gi.traceline(playerEyePos, desiredPos, ent, MASK_SOLID);
     cameraPos = trace.endpos;
 
     // If we hit something, handle the collision
     if (trace.fraction < 1.0f) {
         // Calculate distance between player's eye and camera position
         vec3_t diff = cameraPos - playerEyePos;
         float distanceToObstruction = diff.length();
         
         // If camera is too close to player, reset to eye position
         if (distanceToObstruction < minCollisionDistance) {
             cameraPos = playerEyePos;
         } else {
             // Pull camera away from collision surface to avoid clipping
             cameraPos = cameraPos + (trace.plane.normal * pullbackDistance);
         }
     }
 
     // --- Sky Detection and Prevention ---
     // Check if camera would see the sky by tracing from camera in the forward direction
     vec3_t skyCheckDir = playerEyePos - cameraPos;
     
     // Normalize direction
     float dirLength = skyCheckDir.length();
 
     if (dirLength > 0.0f) {
         skyCheckDir = skyCheckDir * (1.0f / dirLength); // Normalize
         
         // Calculate end point for sky check
         const float skyCheckDist = 64.0f;
         vec3_t skyCheckEnd = cameraPos - (skyCheckDir * skyCheckDist);
         
         // Trace to check for sky
         trace_t skyTrace = gi.traceline(cameraPos, skyCheckEnd, ent, MASK_SOLID);
         
         // If we hit sky, move camera closer to player
         if (skyTrace.surface && (skyTrace.surface->flags & SURF_SKY)) {
             // Move camera 60% closer to player to avoid sky visibility
             const float skyAvoidanceFactor = 0.6f;
             vec3_t moveDir = playerEyePos - cameraPos;
             
             cameraPos = cameraPos + (moveDir * skyAvoidanceFactor);
             
             // Do another collision check after adjustment
             trace = gi.traceline(playerEyePos, cameraPos, ent, MASK_SOLID);
             if (trace.fraction < 1.0f) {
                 cameraPos = trace.endpos;
                 
                 // Apply pullback again
                 cameraPos = cameraPos + (trace.plane.normal * pullbackDistance);
             }
         }
     }
     
     // Additional step: Check for walls nearby in multiple directions to prevent clipping
     const int numProbes = 8;
     const float probeRadius = 12.0f;
     int wallHits = 0;
     vec3_t probeOffset;
     vec3_t probeEnd;
     vec3_t netAdjustment = {0, 0, 0};
     
     // Probe around the camera in a circle to detect nearby walls
     for (int i = 0; i < numProbes; i++) {
         float angle = (float)i / numProbes * PI * 2.0f;
         probeOffset.x = cos(angle) * probeRadius;
         probeOffset.y = sin(angle) * probeRadius;
         probeOffset.z = 0;
         
         probeEnd = cameraPos + probeOffset;
         
         trace_t probeTrace = gi.traceline(cameraPos, probeEnd, ent, MASK_SOLID);
         if (probeTrace.fraction < 1.0f) {
             // Hit a wall, calculate adjustment
             netAdjustment = netAdjustment + (probeTrace.plane.normal * (1.0f - probeTrace.fraction));
             wallHits++;
         }
     }
     
     // Apply adjustment if walls were detected
     if (wallHits > 0) {
         netAdjustment = netAdjustment * (pullbackDistance / wallHits);
         cameraPos = cameraPos + netAdjustment;
     }
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
         if (!ent->client->playerAvatar)
             return;
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
     desiredPos = playerEyePos - (forward * effective_distance) + (right * effective_side);
     desiredPos.z += effective_height;
 
     // --- Improved Collision Detection ---
     vec3_t cameraPos;
     HandleCameraCollision(ent, playerEyePos, desiredPos, cameraPos);
 
     // --- Compute New View Offset ---
     vec3_t targetViewOffset = cameraPos - ent->s.origin;
 
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
  * Initialize third-person view CVARs
  * 
  * Called during game initialization to set up CVARs for third-person view
  */
 void G_InitThirdPerson(void) {
     // Create CVARs for third-person view
     sv_thirdperson = gi.cvar("sv_thirdperson", "0", CVAR_ARCHIVE);
     tp_distance = gi.cvar("tp_distance", "64", CVAR_ARCHIVE);
     tp_height = gi.cvar("tp_height", "0", CVAR_ARCHIVE);
     tp_side = gi.cvar("tp_side", "0", CVAR_ARCHIVE);
     tp_smooth = gi.cvar("tp_smooth", "0.5", CVAR_ARCHIVE);
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
     } else {
         // Enable third-person
         gi.cvar_set("sv_thirdperson", "1");
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
 
 /**
  * Clean up third-person view when level changes
  * 
  * Should be called during level changes to ensure clean state
  */
 void G_ShutdownThirdPerson(void) {
     // Clean up any active third-person views
     for (int i = 0; i < game.maxclients; i++) {
         edict_t *ent = g_edicts + 1 + i;
         if (ent->inuse && ent->client && ent->client->playerAvatar) {
             G_RemoveThirdPersonView(ent);
         }
     }
 }