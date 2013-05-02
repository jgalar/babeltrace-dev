#ifndef _BABELTRACE_COMPAT_STRING_H
#define BABELTRACE_COMPAT_STRING_H

#include <string.h>

int compat_strerror_r(int errnum, char *buf, size_t buflen);

#endif

