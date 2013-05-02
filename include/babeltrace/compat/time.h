#ifndef _BABELTRACE_INCLUDE_COMPAT_TIME_H
#define _BABELTRACE_INCLUDE_COMPAT_TIME_H

#include <time.h>
#include <stdlib.h>

#ifdef __MINGW32__

static inline
struct tm *gmtime_r(const time_t *timep, struct tm *result)
{
	struct tm * r;
	r = gmtime(timep);
	memcpy(result, r, sizeof (struct tm));
	return result;
}

#endif
#endif
