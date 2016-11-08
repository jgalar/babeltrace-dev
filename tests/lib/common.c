/*
 * common.c
 *
 * Lib BabelTrace - Common function to all tests
 *
 * Copyright 2012 - Yannick Brosseau <yannick.brosseau@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <babeltrace/context.h>
#include <babeltrace/iterator.h>
#include <ftw.h>

#define RMDIR_NFDOPEN 8

struct bt_context *create_context_with_path(const char *path)
{
	struct bt_context *ctx;
	int ret;

	ctx = bt_context_create();
	if (!ctx) {
		return NULL;
	}

	ret = bt_context_add_trace(ctx, path, "ctf", NULL, NULL, NULL);
	if (ret < 0) {
		bt_context_put(ctx);
		return NULL;
	}
	return ctx;
}

static
int nftw_recursive_rmdir(const char *file, const struct stat *sb, int flag,
		struct FTW *s)
{
	switch (flag) {
	case FTW_F:
		unlink(file);
		break;
	case FTW_DP:
		rmdir(file);
		break;
	}

	return 0;
}

void recursive_rmdir(const char *path)
{
	int nftw_flags = FTW_PHYS | FTW_DEPTH;

	nftw(path, nftw_recursive_rmdir, RMDIR_NFDOPEN, nftw_flags);
}
