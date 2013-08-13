/*
 * clock.c
 *
 * Babeltrace CTF Writer
 *
 * Copyright 2013 EfficiOS Inc. and Linux Foundation
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/compiler.h>
#include <inttypes.h>

static void bt_ctf_clock_destroy(struct bt_ctf_ref *ref);

struct bt_ctf_clock *bt_ctf_clock_create(const char *name)
{
	struct bt_ctf_clock *clock = NULL;
	if (validate_identifier(name)) {
		goto error;
	}

	clock = g_new0(struct bt_ctf_clock, 1);
	if (!clock) {
		goto error;
	}

	clock->name = g_string_new(name);
	if (!clock->name) {
		goto error_destroy;
	}

	clock->precision = 1;
	uuid_generate(clock->uuid);
	bt_ctf_ref_init(&clock->ref_count, bt_ctf_clock_destroy);
	return clock;

error_destroy:
	bt_ctf_clock_destroy(&clock->ref_count);
error:
	clock = NULL;
	return clock;
}

const char *bt_ctf_clock_get_name(struct bt_ctf_clock *clock)
{
	assert(clock && clock->name);
	return clock->name->str;
}

const char *bt_ctf_clock_get_description(struct bt_ctf_clock *clock)
{
	assert(clock && clock->description);
	return clock->description->str;
}

int bt_ctf_clock_set_description(struct bt_ctf_clock *clock, const char *desc)
{
	int ret = -1;
	if (!clock || !desc || clock->locked) {
		goto end;
	}

	clock->description = g_string_new(desc);
	ret = clock->description ? 0 : ret;
end:
	return ret;
}

uint64_t bt_ctf_clock_get_frequency(struct bt_ctf_clock *clock)
{
	return clock ? clock->frequency : 0;
}

int bt_ctf_clock_set_frequency(struct bt_ctf_clock *clock, uint64_t freq)
{
	int ret = 0;
	if (!clock || clock->locked) {
		ret = -1;
		goto end;
	}

	clock->frequency = freq;
end:
	return ret;
}

uint64_t bt_ctf_clock_get_precision(struct bt_ctf_clock *clock)
{
	return clock ? clock->precision : 0;
}

int bt_ctf_clock_set_precision(struct bt_ctf_clock *clock, uint64_t precision)
{
	int ret = 0;
	if (!clock || clock->locked) {
		ret = -1;
		goto end;
	}
	clock->precision = precision;
end:
	return ret;
}

uint64_t bt_ctf_clock_get_offset_s(struct bt_ctf_clock *clock)
{
	return clock ? clock->offset_s : 0;
}

int bt_ctf_clock_set_offset_s(struct bt_ctf_clock *clock, uint64_t offset_s)
{
	int ret = 0;
	if (!clock || clock->locked) {
		ret = -1;
		goto end;
	}
	clock->offset_s = offset_s;
end:
	return ret;
}

uint64_t bt_ctf_clock_get_offset(struct bt_ctf_clock *clock)
{
	return clock ? clock->offset : 0;
}

int bt_ctf_clock_set_offset(struct bt_ctf_clock *clock, uint64_t offset)
{
	int ret = 0;
	if (!clock || clock->locked) {
		ret = -1;
		goto end;
	}
	clock->offset = offset;
end:
	return ret;
}

int bt_ctf_clock_is_absolute(struct bt_ctf_clock *clock)
{
	return clock ? clock->absolute : -1;
}

int bt_ctf_clock_set_is_absolute(struct bt_ctf_clock *clock, int is_absolute)
{
	int ret = 0;
	if (!clock || clock->locked) {
		ret = -1;
		goto end;
	}
	clock->absolute = is_absolute ? 1 : 0;
end:
	return ret;
}

int bt_ctf_clock_set_time(struct bt_ctf_clock *clock, uint64_t time)
{
	int ret = 0;
	if (!clock || time < clock->time) {
		return -1;
	}
	clock->time = time;
	return ret;
}

void bt_ctf_clock_get(struct bt_ctf_clock *clock)
{
	if (!clock) {
		return;
	}
	bt_ctf_ref_get(&clock->ref_count);
}

void bt_ctf_clock_put(struct bt_ctf_clock *clock)
{
	if (!clock) {
		return;
	}
	bt_ctf_ref_put(&clock->ref_count);
}

void bt_ctf_clock_lock(struct bt_ctf_clock *clock)
{
	if (!clock) {
		return;
	}
	clock->locked = 1;
}

void bt_ctf_clock_serialize(struct bt_ctf_clock *clock,
		struct metadata_context *context)
{
	if (!clock || !context) {
		return;
	}

	unsigned char *uuid = clock->uuid;
	g_string_append(context->string, "clock {\n");
	g_string_append_printf(context->string, "\tname = %s;\n",
		clock->name->str);
	g_string_append_printf(context->string,
		"\tuuid = \"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\";\n",
		uuid[0], uuid[1], uuid[2], uuid[3],
		uuid[4], uuid[5], uuid[6], uuid[7],
		uuid[8], uuid[9], uuid[10], uuid[11],
		uuid[12], uuid[13], uuid[14], uuid[15]);
	g_string_append_printf(context->string, "\tdescription = \"%s\";\n",
		clock->description->str);
	g_string_append_printf(context->string, "\tfreq = %"PRId64";\n",
		clock->frequency);
	g_string_append_printf(context->string, "\tprecision = %"PRId64";\n",
		clock->precision);
	g_string_append_printf(context->string, "\toffset_s = %"PRId64";\n",
		clock->offset_s);
	g_string_append_printf(context->string, "\toffset = %"PRId64";\n",
		clock->offset);
	g_string_append_printf(context->string, "\tabsolute = %s;\n",
		clock->absolute ? "TRUE" : "FALSE");
	g_string_append(context->string, "};\n\n");
}

static void bt_ctf_clock_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}

	struct bt_ctf_clock *clock = container_of(ref, struct bt_ctf_clock,
		ref_count);
	if (clock->name) {
		g_string_free(clock->name, TRUE);
	}

	if (clock->description) {
		g_string_free(clock->description, TRUE);
	}

	g_free(clock);
}
