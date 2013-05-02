#ifndef _BABELTRACE_COMPAT_STDIO_H
#define _BABELTRACE_COMPAT_STDIO_H

#include <stdio.h>

#ifdef __MINGW32__

#include <stdlib.h>
#include <stdarg.h>

static inline void flockfile (FILE * filehandle)
{
	return;
}
static inline void funlockfile(FILE * filehandle)
{
	return;
}

static inline
int vasprintf(char ** strp, const char * fmt, va_list argv)
{
	int ret;

	/* allocate even bigger chunks of memory until the string fits */
	char * buf;
	size_t size = 100;
	for(;;) {
		buf = malloc(size);
		if (buf == (char *)0) {
			ret = -1;
			break;
		}
		ret = vsnprintf(buf, size, fmt, argv);
		if (ret < size)
			break;
		size = size + 100;
		free(buf);
	}
	return ret;
}

static inline
int asprintf(char ** strp, const char *fmt, ...)
{
	int ret;
	va_list argv;
	va_start(argv, fmt);
	ret = vasprintf(strp, fmt, argv);
	va_end(argv);
	return ret;
}

#endif

#endif
