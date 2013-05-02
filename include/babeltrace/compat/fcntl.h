#ifndef _BABELTRACE_INCLUDE_COMPAT_FCNTL_H
#define _BABELTRACE_INCLUDE_COMPAT_FCNTL_H

#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __MINGW32__
static inline
int posix_fallocate(int fd, off_t offset, off_t len)
{
	return 0;
}
#endif

/*
   If we have BABELTRACE_HAVE_OPENAT, then
   openat() and dirfd() work, and open() and close() can
   work on directories.

   If we don't have BABELTRACE_HAVE_OPENAT,
   then we have to open files based on the directory name,
   not the directory handle. The functions
   compat_[opendirfd | closedirfd | dirfd] all are dummied
   to return zero, and compat_openat() takes care of the
   opening of the file.
*/

static inline
int compat_openat(const char * dirname, int dirfd, const char *pathname, int flags, ...)
{

#ifdef BABELTRACE_HAVE_OPENAT
	mode_t mode = 0;
	va_list args;
	va_start (args, flags);
	mode = (mode_t)va_arg(args, int);
	va_end(args);
	return openat(dirfd, pathname, flags, mode);
#else
	char szSearch[MAX_PATH]         = {0};
	sprintf(szSearch, "%s/%s", dirname, pathname);
#ifdef __MINGW32__
	flags = flags | O_BINARY;
#endif
	return open(szSearch, flags);
#endif
}

static inline
int compat_opendirfd(const char *fpath, int flags)
{
#ifdef BABELTRACE_HAVE_OPENAT
	return open(fpath, flags);
#else
	return 0;
#endif
}

static inline
int compat_closedirfd(int dirfd)
{
#ifdef BABELTRACE_HAVE_OPENAT
	return close(dirfd);
#else
	return 0;
#endif
}

#endif