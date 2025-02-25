#include "q2_modelindex_wrapper.h"

// Define the global pointer exactly once.
int (*q2_real_modelindex)(const char *name) = nullptr;
