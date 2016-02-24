#ifndef _BABELTRACE_YAJL_H
#define _BABELTRACE_YAJL_H

/*
 * Babeltrace - yajl shim to support yajl 1.x and 2.x
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stddef.h>
#include <yajl/yajl_parse.h>

#if (BABELTRACE_YAJL_VERSION == 1)
typedef unsigned int bt_yajl_size_t;
typedef long bt_yajl_int_value_t;

static inline
yajl_handle bt_yajl_alloc(const yajl_callbacks *callbacks, void *ctx)
{
	yajl_parser_config config = { 0 };

	config.allowComments = 1;

	/* config values are copied by yajl_alloc() */
	return yajl_alloc(callbacks, &config, NULL, ctx);
}

static inline
yajl_status bt_yajl_complete_parse(yajl_handle handle)
{
	return yajl_parse_complete(handle);
}

static inline
bool bt_yajl_parse_success(yajl_status status)
{
	return status == yajl_status_ok ||
		status == yajl_status_insufficient_data;
}
#else
typedef size_t bt_yajl_size_t;
typedef long long bt_yajl_int_value_t;

static inline
yajl_handle bt_yajl_alloc(const yajl_callbacks *callbacks, void *ctx)
{
	yajl_handle handle = NULL;
	int ret;

	handle = yajl_alloc(callbacks, NULL, ctx);
	if (!handle) {
		goto end;
	}

	ret = yajl_config(handle, yajl_allow_comments, 1);
	if (!ret) {
		yajl_free(handle);
		handle = NULL;
		goto end;
	}

end:
	return handle;
}

static inline
yajl_status bt_yajl_complete_parse(yajl_handle handle)
{
	return yajl_complete_parse(handle);
}

static inline
bool bt_yajl_parse_success(yajl_status status)
{
	return status == yajl_status_ok;
}
#endif /* (BABELTRACE_YAJL_VERSION == 1) */

static inline
yajl_status bt_yajl_parse(yajl_handle handle, const unsigned char *json_text,
		bt_yajl_size_t json_text_len)
{
	return yajl_parse(handle, json_text, json_text_len);
}

static inline
void by_yajl_free(yajl_handle handle)
{
	if (!handle) {
		return;
	}

	yajl_free(handle);
}

static inline
bt_yajl_size_t bt_yajl_get_bytes_consumed(yajl_handle handle)
{
	return yajl_get_bytes_consumed(handle);
}

#endif /* _BABELTRACE_YAJL_H */
