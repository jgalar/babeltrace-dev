#ifndef _BABELTRACE_COMPAT_STDIO_H
#define _BABELTRACE_COMPAT_STDIO_H

#include <stdio.h>

#ifdef __MINGW32__
static inline void flockfile (FILE * filehandle)
{
	return;
}

#endif

#endif
