#pragma once
#include "g_local.h"
void G_RegisterThirdpersonCVars();
void G_SetThirdPersonView(edict_t* ent);

extern cvar_t* sv_thirdperson; // Declare the cvar.

// Make the command available
static void G_Thirdperson_Command(edict_t* ent);