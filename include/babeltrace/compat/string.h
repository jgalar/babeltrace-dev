#ifndef _BABELTRACE_COMPAT_STRING_H
#define BABELTRACE_COMPAT_STRING_H

#include <string.h>

#ifdef __MINGW32__
char *strtok_r(char *str, const char *delim, char **nextp);
size_t strnlen(const char *str, size_t maxlen);
#endif

int compat_strerror_r(int errnum, char *buf, size_t buflen);

#endif

