#ifndef _BABELTRACE_INCLUDE_COMPAT_MMAN_H
#define _BABELTRACE_INCLUDE_COMPAT_MMAN_H

#ifdef __MINGW32__

#define PROT_READ	0x1
#define PROT_WRITE	0x2
#define PROT_EXEC	0x4
#define PROT_NONE	0x0

#define MAP_SHARED	0x01
#define MAP_PRIVATE	0x02
#define MAP_FAILED	((void *) -1)

/* mmap for mingw32 */
void *mmap (void *ptr, long size, long prot, long type, long handle, long arg);
/* munmap for mingw32 */
long munmap (void *ptr, long size);

#else
#include <sys/mman.h>
#endif

#endif /* _BABELTRACE_INCLUDE_COMPAT_MMAN_H */
