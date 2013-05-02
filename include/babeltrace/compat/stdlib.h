#ifndef _BABELTRACE_COMPAT_STDLIB_H
#define BABELTRACE_COMPAT_STDLIB_H

#include <stdlib.h>

#ifdef __MINGW32__
int mkstemp(char *template);
#endif

#endif

