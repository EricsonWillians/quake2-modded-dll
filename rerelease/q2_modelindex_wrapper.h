#ifndef Q2_MODELINDEX_WRAPPER_H
#define Q2_MODELINDEX_WRAPPER_H

#include "g_local.h"
#include "q_std.h"   // Contains Q_strlcpy, Q_strlcat, etc.
#include <cstdlib>   // For getenv
#include <cstring>

// Global pointer to the engine's original modelindex function.
// Defined once in q2_modelindex_wrapper.cpp.
extern int (*q2_real_modelindex)(const char *name);

/*
===============================================================================
  Q2_IsRunningUnderWineOrProton
  Checks environment variables commonly set by Wine or Proton.
===============================================================================
*/
static inline bool Q2_IsRunningUnderWineOrProton()
{
    if (std::getenv("WINELOADERNOEXEC") != nullptr)
        return true;
    if (std::getenv("STEAM_COMPAT_DATA_PATH") != nullptr)
        return true;
    return false;
}

/*
===============================================================================
  Q2_ModelIndexWrapper
  This inline function normalizes the given file path by:
    1. Converting backslashes to forward slashes.
    2. If running under Wine/Proton, replacing "model_players/" with "models/players/".
  Then it calls the engine's original modelindex function.
===============================================================================
*/
static inline int Q2_ModelIndexWrapper(const char *name)
{
    char fixedName[MAX_QPATH];
    Q_strlcpy(fixedName, name, sizeof(fixedName));

    // Convert backslashes to forward slashes.
    for (char *p = fixedName; *p; p++) {
        if (*p == '\\')
            *p = '/';
    }

    // If running under Wine/Proton, do extra prefix replacement.
    if (Q2_IsRunningUnderWineOrProton()) {
        static const char oldPrefix[] = "model_players/";
        static const char newPrefix[] = "models/players/";
        if (!std::strncmp(fixedName, oldPrefix, std::strlen(oldPrefix))) {
            char temp[MAX_QPATH];
            const char *suffix = fixedName + std::strlen(oldPrefix);
            Q_strlcpy(temp, newPrefix, sizeof(temp));
            Q_strlcat(temp, suffix, sizeof(temp));
            Q_strlcpy(fixedName, temp, sizeof(fixedName));
#ifdef HAVE_GI_DPRINTF
            // If gi.dprintf is available, print a debug message.
            gi.dprintf("Wine/Proton path fix: %s -> %s\n", name, fixedName);
#endif
        }
    }

    return q2_real_modelindex(fixedName);
}

/*
===============================================================================
  Install_ModelIndexWrapper
  This inline function should be called during game module initialization (e.g. in GetGameAPI).
  It saves the engine's original modelindex pointer and replaces it with Q2_ModelIndexWrapper,
  so that all subsequent calls to gi.modelindex are normalized.
===============================================================================
*/
static inline void Install_ModelIndexWrapper(game_import_t *import)
{
    q2_real_modelindex = import->modelindex;
    import->modelindex = Q2_ModelIndexWrapper;
    gi = *import;
}

#endif // Q2_MODELINDEX_WRAPPER_H
