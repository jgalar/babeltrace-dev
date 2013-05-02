/* This file is only built under MINGW32 */

#include <windows.h>
#include <stdlib.h>
#include <babeltrace/compat/uuid.h>

static void fix_uuid_endian(struct UUID * uuid)
{
	unsigned char * ptr;
	unsigned char tmp;
	#define SWAP(p, i, j) tmp=*(p+i);*(p+i)=*(p+j);*(p+j)=tmp;
	ptr = (unsigned char *)uuid;
	SWAP(ptr, 0, 3)
	SWAP(ptr, 1, 2)
	SWAP(ptr, 4, 5)
	SWAP(ptr, 6, 7)

}

int compat_uuid_generate(unsigned char *uuid_out)
{
	RPC_STATUS status;
	status = UuidCreate((struct UUID *)uuid_out);
	if (status == RPC_S_OK)
		return 0;
	else
		return -1;
}

int compat_uuid_unparse(const unsigned char *uuid_in, char *str_out)
{
	RPC_STATUS status;
	unsigned char *alloc_str;
	int ret;
	fix_uuid_endian(uuid_in);
	status = UuidToString((struct UUID *)uuid_in, &alloc_str);
	fix_uuid_endian(uuid_in);
	if (status == RPC_S_OK) {
		strcpy(str_out, alloc_str);
		ret = 0;
	} else {
		ret = -1;
	}
	RpcStringFree(alloc_str);
	return ret;
}

int compat_uuid_parse(const char *str_in, unsigned char *uuid_out)
{
	RPC_STATUS status;

	status = UuidFromString(str_in, (struct UUID *)uuid_out);
	fix_uuid_endian(uuid_out);

	if (status == RPC_S_OK)
		return 0;
	else
		return -1;
}

int compat_uuid_compare(const unsigned char *uuid_a,
		const unsigned char *uuid_b)
{
	RPC_STATUS status;
	int ret;

	if (UuidCompare((struct UUID *)uuid_a, (struct UUID *)uuid_b, &status) == 0)
		ret = 0;
	else
	{
		ret = -1;
	}
	return ret;

}
