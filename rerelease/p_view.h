#ifndef P_VIEW_H
#define P_VIEW_H

#include "g_local.h"

// External global variables used by view functions
extern float bobmove;
extern int bobcycle, bobcycle_run;
extern float bobfracsin;
extern vec3_t forward, right, up;
extern float xyspeed;

// Utility functions
bool SkipViewModifiers();
float SV_CalcRoll(const vec3_t &angles, const vec3_t &velocity);

// Core view manipulation functions
void P_DamageFeedback(edict_t *player);
void SV_CalcViewOffset(edict_t *ent);
void SV_CalcGunOffset(edict_t *ent);
void SV_CalcBlend(edict_t *ent);
void P_WorldEffects();

// Client appearance functions
void G_SetClientEffects(edict_t *ent);
void G_SetClientEvent(edict_t *ent);
void G_SetClientSound(edict_t *ent);
void G_SetClientFrame(edict_t *ent);

#endif // P_VIEW_H