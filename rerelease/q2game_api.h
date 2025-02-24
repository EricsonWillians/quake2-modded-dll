#ifdef __cplusplus
  #define Q2GAME_EXTERN extern "C"
#else
  #define Q2GAME_EXTERN
#endif

#ifdef _WIN32
  #if defined(KEX_Q2GAME_EXPORTS)
    #define Q2GAME_API Q2GAME_EXTERN __declspec(dllexport)
  #elif defined(KEX_Q2GAME_IMPORTS)
    #define Q2GAME_API Q2GAME_EXTERN __declspec(dllimport)
  #else
    #define Q2GAME_API
  #endif
#else
  #if defined(KEX_Q2GAME_EXPORTS)
    #define Q2GAME_API Q2GAME_EXTERN __attribute__((visibility("default")))
  #else
    #define Q2GAME_API
  #endif
#endif
