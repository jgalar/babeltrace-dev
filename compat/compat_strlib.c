#include <babeltrace/compat/string.h>

#ifdef __MINGW32__

char* strtok_r(
	char *str,
	const char *delim,
	char **nextp)
{
	char *ret;

	/* if str is NULL, continue from nextp */
	if (str == NULL) {
		str = *nextp;
	}
	/* skip the leading delimiter characters */
	str += strspn(str, delim);

	/* return NULL if all are delimiters */
	if (*str == '\0') {
		return NULL;
	}
	/* return the pointer to the first character that is not delimiter */
	ret = str;

	/* scan to the next delimiter */
	str += strcspn(str, delim);

	/* if we found a delimiter instead of a NUL */
	if (*str) {
		/* replace the delimiter with the NUL */
		*str = '\0';
		/* next time, scan from the character after NUL */
		*nextp = str + 1;
	}
	else {
		/* end of string: next time, scan at the end (NUL) again */
		*nextp = str;
	}

	return ret;
}

int strerror_r(int errnum, char *buf, size_t buflen)
{
	/* non-recursive implementation of strerror_r */
	char * retbuf;
	retbuf = strerror(errnum);
	strncpy(buf, retbuf, buflen);
	buf[buflen - 1] = '\0';
	return 0;
}
#endif

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

