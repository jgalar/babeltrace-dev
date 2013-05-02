#ifndef _BABELTRACE_INCLUDE_COMPAT_FTW_H
#define _BABELTRACE_INCLUDE_COMPAT_FTW_H

#ifndef __MINGW32__
#include <ftw.h>
#else

#include <windows.h>

struct FTW {
	int base;
	int level;
};


#define FTW_F 0
#define FTW_D 1
#define FTW_DNR 2
#define FTW_NS 3
#define FTW_SL 4

static inline
int nftw(const char *dirpath,
	int (*fn) (const char *fpath, const struct stat *sb,
		int typeflag, struct FTW *ftwbuf),
	int nopenfd, int flags)
{
	CHAR szSearch[MAX_PATH]         = {0};
	CHAR szDirectory[MAX_PATH]      = {0};
	HANDLE hFind                    = NULL;
	WIN32_FIND_DATAA FindFileData;

	sprintf(szSearch, "%s%s", dirpath, "\\*");
	hFind = FindFirstFileA(szSearch, &FindFileData);
	if(hFind == INVALID_HANDLE_VALUE)
	{
		/* print error */
		return -1;
	}
	do {

		if((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
		{
			/* this is a directory */
			/* skip '..' */
			if (strcmp(FindFileData.cFileName, "..") == 0)
				continue;

			/* return '.' but do not recurse into it */
			if (strcmp(FindFileData.cFileName, ".") == 0)
			{
				(*fn)(dirpath, 0, FTW_D, 0);
				continue;
			}

			/* for others, recurse into them */
			sprintf(szDirectory, "%s/%s", dirpath, FindFileData.cFileName);
			nftw(szDirectory, fn, nopenfd, flags);

		}
		else
		{
			/* for files, report but do not recurse */
			sprintf(szDirectory, "%s/%s", dirpath, FindFileData.cFileName);
			(*fn)(szDirectory, 0, FTW_F, 0);
		}
	} while(FindNextFileA(hFind, &FindFileData));

	FindClose(hFind);
	return 0;
}
#endif
#endif
