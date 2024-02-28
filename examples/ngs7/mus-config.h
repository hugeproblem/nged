#pragma once

#ifdef __GNUC__
#define HAVE_OVERFLOW_CHECKS 0
#else
#define HAVE_OVERFLOW_CHECKS 0
#endif
#define WITH_SYSTEM_EXTRAS 1
#define WITH_GMP 0
