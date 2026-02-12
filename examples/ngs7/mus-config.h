#pragma once

#ifdef __GNUC__
#define HAVE_OVERFLOW_CHECKS 0
#else
#define HAVE_OVERFLOW_CHECKS 0
#endif
#define WITH_SYSTEM_EXTRAS 1
#define WITH_GMP 0

#ifdef _MSC_VER
#define popen _popen
#define pclose _pclose
#endif

#include <time.h>
