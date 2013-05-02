#include <babeltrace/compat/string.h>

#ifdef __MINGW32__

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

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

size_t strnlen(const char *str, size_t maxlen)
{
	char * ptr;
	size_t len;
	ptr = (char *)str;
	len = 0;

	/* while there still are characters in the string */
	while(*ptr != '\0') {
		/* increment counter */
		len++;
		/* check that we do not exceed the given maximum */
		if (len == maxlen) {
			return len;
		}
		/* next character */
		ptr++;

	}
	return len;
}

ssize_t getline(char **buf, size_t *len, FILE *stream)
{
	int c = EOF;
	ssize_t count = (ssize_t)0;

	if( feof( stream ) || ferror( stream ) )
		return -1;

	for(;;) {
		/* read character from stream */
		c = fgetc(stream);
		if (c == EOF) {
			break;
		}
		if (c == '\n') {
			break;
		}

		/* store to *buf */
		/* check if it fits - space for character and NUL */
		if (count > *len - 2) {
			/* have to allocate more space */
			*len = *len + 100;
			*buf = realloc(*buf, *len);
			if (*buf == NULL) {
				errno = ENOMEM;
				return (ssize_t)-1;
			}
		}
		(*buf)[count] = c;
		count++;
	}
	(*buf)[count] = '\0';
	return (count == (ssize_t)0) ? (ssize_t)-1 : (ssize_t)0;
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

