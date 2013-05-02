#ifndef _BABELTRACE_INCLUDE_COMPAT_FCNTL_H
#define _BABELTRACE_INCLUDE_COMPAT_FCNTL_H

#include <fcntl.h>
#ifdef __MINGW32__
static inline
int posix_fallocate(int fd, off_t offset, off_t len)
{
	return 0;
}
#endif

#endif