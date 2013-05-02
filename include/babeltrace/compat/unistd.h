#ifndef _BABELTRACE_INCLUDE_COMPAT_UNISTD_H
#define _BABELTRACE_INCLUDE_COMPAT_UNISTD_H

#include <unistd.h>

#ifndef _PC_NAME_MAX
#define _PC_NAME_MAX 0
#endif

#ifdef __MINGW32__

static inline
int fpathconf (int fd, int name)
{
	/* dummy function that always returns MAX_PATH */
	return MAX_PATH;
}

#define sleep(x) Sleep(1000 * (x))

#endif

#endif