/* This file is only built under MINGW32 */

#include <windows.h>
#include <stdlib.h>

int mkstemp(char *template)
{
	char tempPath[MAX_PATH];
	char tmpname[MAX_PATH];
	HANDLE file;

	extern int _open_osfhandle(int *, int);

	/* Ignore the template. Use Windows calls to create a temporary file whose name begins with BBT */
	GetTempPath(MAX_PATH, tempPath);
	GetTempFileName(tempPath, "BBT", 0, tmpname);

	file = CreateFile(tmpname, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);

	return _open_osfhandle((int *)file, 0);
}
