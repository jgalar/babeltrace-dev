/*
 * Value object <-> JSON
 *
 * Babeltrace Library
 *
 * Copyright (c) 2016 EfficiOS Inc. and Linux Foundation
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

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <babeltrace/values.h>
#include <babeltrace/yajl.h>
#include <glib.h>

struct ctx {
	GPtrArray *stack;
	struct bt_value *root_value;
	bool got_error;
};

struct stack_frame {
	struct bt_value *parent;
	GString *last_map_key;
};

static
void stack_destroy_frame(void *data)
{
	struct stack_frame *frame = data;

	if (!data) {
		return;
	}

	assert(frame);
	bt_put(frame->parent);

	if (frame->last_map_key) {
		g_string_free(frame->last_map_key, TRUE);
	}

	g_free(frame);
}

static
void stack_destroy(GPtrArray *stack)
{
	if (!stack) {
		return;
	}

	g_ptr_array_free(stack, TRUE);
}

static
GPtrArray *stack_create(void)
{
	return g_ptr_array_new_with_free_func(stack_destroy_frame);
}

static
size_t stack_size(GPtrArray *stack)
{
	return stack->len;
}

static
int stack_push(GPtrArray *stack, struct bt_value *parent)
{
	int ret = 0;
	struct stack_frame *frame;

	frame = g_new0(struct stack_frame, 1);
	if (!frame) {
		ret = -1;
		goto end;
	}

	frame->last_map_key = g_string_new(NULL);
	if (!frame->last_map_key) {
		g_free(frame);
		frame = NULL;
		ret = -1;
		goto end;
	}

	frame->parent = bt_get(parent);
	g_ptr_array_add(stack, frame);

end:
	return ret;
}

static
void stack_pop(GPtrArray *stack)
{
	assert(stack_size(stack) > 0);
	g_ptr_array_remove_index(stack, stack_size(stack) - 1);
}

static
struct bt_value *stack_peek_parent(GPtrArray *stack)
{
	struct stack_frame *frame;
	assert(stack_size(stack) > 0);

	frame = g_ptr_array_index(stack, stack_size(stack) - 1);
	assert(frame);

	return frame->parent;
}

static
const char *stack_peek_last_map_key(GPtrArray *stack)
{
	struct stack_frame *frame;
	const char *last_map_key = NULL;

	assert(stack_size(stack) > 0);
	frame = g_ptr_array_index(stack, stack_size(stack) - 1);
	assert(frame);
	if (!frame->last_map_key) {
		goto end;
	}

	last_map_key = frame->last_map_key->str;

end:
	return last_map_key;
}

static
void stack_peek_set_last_map_key(GPtrArray *stack, const char *key,
		bt_yajl_size_t key_len)
{
	struct stack_frame *frame;

	assert(stack_size(stack) > 0);
	frame = g_ptr_array_index(stack, stack_size(stack) - 1);
	assert(frame);
	g_string_truncate(frame->last_map_key, 0);
	g_string_append_len(frame->last_map_key, key, key_len);
}

static
int stack_peek_insert_into_parent(GPtrArray *stack, struct bt_value *value)
{
	int ret = 0;
	struct bt_value *parent;
	enum bt_value_status status;

	if (stack_size(stack) == 0) {
		/* No parent: no place to insert this value */
		goto end;
	}

	parent = stack_peek_parent(stack);

	if (bt_value_is_array(parent)) {
		status = bt_value_array_append(parent, value);
		if (status != BT_VALUE_STATUS_OK) {
			ret = -1;
			goto end;
		}
	} else if (bt_value_is_map(parent)) {
		const char *last_map_key =
			stack_peek_last_map_key(stack);

		if (!last_map_key) {
			ret = -1;
			goto end;
		}

		status = bt_value_map_insert(parent, last_map_key, value);
		if (status != BT_VALUE_STATUS_OK) {
			ret = -1;
			goto end;
		}
	} else {
		assert(false);
	}

end:
	return ret;
}

static
int ctx_new_value(struct ctx *ctx, struct bt_value *value)
{
	int ret = 0;

	if (stack_size(ctx->stack) == 0) {
		/* No parent: set as root */
		bt_put(ctx->root_value);
		ctx->root_value = bt_get(value);
	} else {
		/* Insert into parent */
		ret = stack_peek_insert_into_parent(ctx->stack, value);
	}

	return ret;
}

static
int handle_null(void *data)
{
	int ret = 1;
	int new_value_ret;
	struct ctx *ctx = data;

	new_value_ret = ctx_new_value(ctx, bt_value_null);
	if (new_value_ret) {
		ret = 0;
		goto end;
	}

end:
	if (!ret) {
		ctx->got_error = true;
	}

	return ret;
}

static
int handle_boolean(void *data, int bool_val)
{
	int ret = 1;
	int new_value_ret;
	struct ctx *ctx = data;
	struct bt_value *value = NULL;

	value = bt_value_bool_create_init(bool_val);
	if (!value) {
		ret = 0;
		goto end;
	}

	new_value_ret = ctx_new_value(ctx, value);
	if (new_value_ret) {
		ret = 0;
		goto end;
	}

end:
	bt_put(value);

	if (!ret) {
		ctx->got_error = true;
	}

	return ret;
}

static
int parse_int64(const char *input, bt_yajl_size_t input_len, int64_t *val)
{
	int ret = -1;
	int pos;

	ret = sscanf(input, "%" PRId64 "%n", val, &pos);
	if (ret == 1 && pos == input_len) {
		ret = 0;
	}

	return ret;
}

static
int parse_double(const char *input, bt_yajl_size_t input_len, double *val)
{
	int ret;
	int pos;

	ret = sscanf(input, "%lf%n", val, &pos);
	if (ret == 1 && pos == input_len) {
		ret = 0;
	}

	return ret;
}

static
int handle_number(void *data, const char *number_val, bt_yajl_size_t number_len)
{
	int ret = 1;
	int new_value_ret;
	struct ctx *ctx = data;
	struct bt_value *value = NULL;
	int parse_ret;
	int64_t int_val;
	double dbl_val;
	char buf[64];

	/*
	 * We try to parse it as a 64-bit signed integer first, which
	 * is the maximum allowed by an integer value object anyway.
	 * Otherwise we try to parse it as a floating point number.
	 *
	 * The string number_val is not NULL-terminated, so we need to
	 * copy it to a temporary buffer before using sscanf().
	 */
	if (number_len >= sizeof(buf)) {
		ret = 0;
		goto end;
	}

	memcpy(buf, number_val, number_len);
	buf[number_len] = '\0';
	parse_ret = parse_int64(buf, number_len, &int_val);
	if (!parse_ret) {
		value = bt_value_integer_create_init(int_val);
		if (!value) {
			ret = 0;
			goto end;
		}
	} else {
		parse_ret = parse_double(buf, number_len, &dbl_val);
		if (parse_ret) {
			ret = 0;
			goto end;
		}

		value = bt_value_float_create_init(dbl_val);
		if (!value) {
			ret = 0;
			goto end;
		}
	}

	new_value_ret = ctx_new_value(ctx, value);
	if (new_value_ret) {
		ret = 0;
		goto end;
	}

end:
	bt_put(value);

	if (!ret) {
		ctx->got_error = true;
	}

	return ret;
}

static
int handle_string(void *data, const unsigned char *string_val,
		bt_yajl_size_t string_len)
{
	int ret = 1;
	int new_value_ret;
	struct ctx *ctx = data;
	struct bt_value *value = NULL;
	char *string;

	/* Convert to C string */
	string = g_new0(char, string_len + 1);
	if (!string) {
		ret = 0;
		goto end;
	}

	memcpy(string, string_val, string_len);

	/* Create value */
	value = bt_value_string_create_init(string);
	if (!value) {
		ret = 0;
		goto end;
	}

	new_value_ret = ctx_new_value(ctx, value);
	if (new_value_ret) {
		ret = 0;
		goto end;
	}

end:
	bt_put(value);
	g_free(string);

	if (!ret) {
		ctx->got_error = true;
	}

	return ret;
}

static
int handle_start_map(void *data)
{
	int ret = 1;
	int stack_ret;
	struct ctx *ctx = data;
	struct bt_value *value;

	value = bt_value_map_create();
	if (!value) {
		ret = 0;
		goto end;
	}

	stack_ret = stack_push(ctx->stack, value);
	if (stack_ret) {
		ret = 0;
		goto end;
	}

end:
	bt_put(value);

	if (!ret) {
		ctx->got_error = true;
	}

	return ret;
}

static
int handle_map_key(void *data, const unsigned char *key, bt_yajl_size_t key_len)
{
	int ret = 1;
	struct ctx *ctx = data;

	stack_peek_set_last_map_key(ctx->stack, (const char *) key, key_len);

	return ret;
}

static
int handle_end_array_or_map(void *data)
{
	int ret = 1;
	int new_value_ret;
	struct ctx *ctx = data;
	struct bt_value *value;

	/* Save current parent (current map) before popping */
	value = bt_get(stack_peek_parent(ctx->stack));
	stack_pop(ctx->stack);
	new_value_ret = ctx_new_value(ctx, value);
	if (new_value_ret) {
		ret = 0;
		goto end;
	}

end:
	bt_put(value);

	if (!ret) {
		ctx->got_error = true;
	}

	return ret;
}

static
int handle_start_array(void *data)
{
	int ret = 1;
	int stack_ret;
	struct ctx *ctx = data;
	struct bt_value *value;

	value = bt_value_array_create();
	if (!value) {
		ret = 0;
		goto end;
	}

	stack_ret = stack_push(ctx->stack, value);
	if (stack_ret) {
		ret = 0;
		goto end;
	}

end:
	bt_put(value);

	if (!ret) {
		ctx->got_error = true;
	}

	return ret;
}

static yajl_callbacks callbacks = {
	.yajl_null = handle_null,
	.yajl_boolean = handle_boolean,
	.yajl_integer = NULL,
	.yajl_double = NULL,
	.yajl_number = handle_number,
	.yajl_string = handle_string,
	.yajl_start_map = handle_start_map,
	.yajl_map_key = handle_map_key,
	.yajl_end_map = handle_end_array_or_map,
	.yajl_start_array = handle_start_array,
	.yajl_end_array = handle_end_array_or_map,
};

struct bt_value *bt_value_from_json(const char *json_string)
{
	yajl_handle handle = NULL;
	yajl_status status;
	size_t json_string_len;
	bt_yajl_size_t consumed_bytes;
	struct bt_value *value = NULL;
	struct ctx ctx = { 0 };

	if (!json_string) {
		goto end;
	}

	ctx.stack = stack_create();
	if (!ctx.stack) {
		goto end;
	}

	handle = bt_yajl_alloc(&callbacks, &ctx);
	if (!handle) {
		goto end;
	}

	json_string_len = strlen(json_string);
	status = bt_yajl_parse(handle, (const unsigned char *) json_string,
		json_string_len);
	if (!bt_yajl_parse_success(status)) {
		goto end;
	}

	consumed_bytes = bt_yajl_get_bytes_consumed(handle);
	if (consumed_bytes != json_string_len) {
		goto end;
	}

	status = bt_yajl_complete_parse(handle);
	if (status != yajl_status_ok) {
		goto end;
	}

	if (ctx.got_error) {
		goto end;
	}

	/* Success: move context's root value to returned value */
	BT_MOVE(value, ctx.root_value);

end:
	by_yajl_free(handle);
	stack_destroy(ctx.stack);

	if (!value) {
		bt_put(ctx.root_value);
	}

	return value;
}
