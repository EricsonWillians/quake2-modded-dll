/**
 * g_thirdperson.h
 * 
 * Header file for the third-person camera implementation in Quake 2 Rerelease
 */

 #pragma once
 #include "g_local.h"
 
 /**
  * Register console variables for third-person view
  * Should be called during game initialization
  */
 void G_RegisterThirdpersonCVars();
 
 /**
  * Update third-person camera and player avatar
  *
  * Main function that handles the third-person view implementation.
  * Call this each frame when third-person view is active.
  * 
  * @param ent Player entity
  */
 void G_SetThirdPersonView(edict_t* ent);
 
 /**
  * Remove third-person effects and restore first-person view
  *
  * Call this when disabling third-person view or when the player disconnects.
  * 
  * @param ent Player entity
  */
 void G_RemoveThirdPersonView(edict_t* ent);
 
 /**
  * Console command handler for toggling third-person mode
  *
  * @param ent Player entity that issued the command
  */
 void G_Thirdperson_Command(edict_t* ent);
 
 /**
  * Adjust weapon aiming for third-person view
  *
  * Call this from weapon firing functions to ensure accurate aiming
  * when in third-person mode.
  * 
  * @param self Player entity
  * @param start Firing start position (may be modified)
  * @param aimdir Direction vector (will be modified for accurate aiming)
  */
 void G_AdjustThirdPersonAim(edict_t *self, vec3_t &start, vec3_t &aimdir);
