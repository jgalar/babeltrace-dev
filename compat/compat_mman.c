/* This file is only built under MINGW32 */

#include <windows.h>
#include <stdlib.h>
#include <babeltrace/compat/mman.h>

#define HAS_FLAG( BITSET, FLAG ) ( ( BITSET & FLAG ) == FLAG )

typedef struct
{
    HANDLE hFile;
    HANDLE hFileMappingObject;
    unsigned char *start_addr;
    long offset;
} mmap_data;

mmap_data *arr_mmap_data = NULL;
size_t arr_mmap_data_size = 0;
DWORD allocationGranularity = 0;

void mmap_data_add( mmap_data data )
{
    arr_mmap_data = (mmap_data *) realloc( arr_mmap_data, ( arr_mmap_data_size + 1 ) * sizeof( mmap_data ) );
    arr_mmap_data[ arr_mmap_data_size ] = data;
    arr_mmap_data_size += 1;
}

mmap_data *mmap_data_find_addr( unsigned char *start_addr )
{
    size_t i;
    for( i = 0; i < arr_mmap_data_size; ++i )
	if( arr_mmap_data[i].start_addr + arr_mmap_data[i].offset == start_addr )
	    return arr_mmap_data + i;

    return NULL;
}

/* mmap for mingw32 */
void *mmap( void *ptr, long size, long prot, long type, long handle, long arg )
{
    //__builtin_printf( "mmap: %p, %d, %d, %d, %d, %d\n", ptr, size, prot, type, handle, arg );
    mmap_data data_entry;
    long offset;

    if (allocationGranularity == 0)
    {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        allocationGranularity = sysinfo.dwAllocationGranularity;
    }

    if( prot == PROT_NONE || HAS_FLAG( prot, PROT_EXEC ) )
	return MAP_FAILED; /* not supported - fail safe */

    data_entry.hFile = (HANDLE) _get_osfhandle( handle );
    if( data_entry.hFile != INVALID_HANDLE_VALUE )
    {

	/* There is no 1:1 mapping, best effort strategy */
	DWORD flProtect = PAGE_READONLY;
	DWORD dwDesiredAccess = FILE_MAP_READ;

	if( HAS_FLAG( prot, PROT_WRITE ) )
	{
	    flProtect = PAGE_READWRITE;
	    if( HAS_FLAG( prot, PROT_READ ) )
		dwDesiredAccess = FILE_MAP_ALL_ACCESS;
	    else
		dwDesiredAccess = FILE_MAP_WRITE;
	}

	if( HAS_FLAG( type, MAP_PRIVATE ) )
	{
	    flProtect = PAGE_WRITECOPY;
	    dwDesiredAccess = FILE_MAP_COPY;
	}

	data_entry.hFileMappingObject = CreateFileMapping( data_entry.hFile, NULL, flProtect, 0, 0, NULL );
	if( data_entry.hFileMappingObject != NULL )
	{
            DWORD filesizelow;
            DWORD filesizehigh;
	    filesizelow = GetFileSize(data_entry.hFile, &filesizehigh);
	    offset = 0;
            while (arg > allocationGranularity)
            {
                if (filesizelow < allocationGranularity)
                {
			filesizehigh--;
		}
		filesizelow -= allocationGranularity;

                arg -= allocationGranularity;
                offset += allocationGranularity;
            }
            data_entry.offset = arg;
            size = size + arg;
            if ((size > filesizelow) && (filesizehigh == 0))
            {
                size = filesizelow;
            }
	    data_entry.start_addr = MapViewOfFile( data_entry.hFileMappingObject, dwDesiredAccess, 0, offset, size);
	    if( data_entry.start_addr != NULL )
	    {
		mmap_data_add( data_entry );
		return (void *)(data_entry.start_addr + arg);
	    }
	}

    }

    return MAP_FAILED;
}


/* munmap for mingw32 */
long munmap( void *ptr, long size )
{
    mmap_data *data_entry = mmap_data_find_addr( ptr );
    if( data_entry != NULL  && UnmapViewOfFile( data_entry->start_addr ) != 0 )
    {
	CloseHandle( data_entry->hFileMappingObject );
	data_entry->start_addr = NULL;
	return 0;
    }

    return -1;
}
