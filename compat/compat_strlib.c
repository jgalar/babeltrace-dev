#include <babeltrace/compat/string.h>

int compat_strerror_r(int errnum, char *buf, size_t buflen)
{
#if !defined(__linux__) || ((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !defined(_GNU_SOURCE))
/* XSI-compliant strerror_r */
	return strerror_r(errnum, buf, buflen);
#else
/* GNU-compliant strerror_r */
	char * retbuf;
	retbuf = strerror_r(errnum, buf, buflen);
	if (retbuf != buf)
		strcpy(buf, retbuf);
	return 0;
#endif
}

