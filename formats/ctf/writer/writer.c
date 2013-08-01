/*
 * writer.c
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

#include <babeltrace/ctf-writer/writer.h>
#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/ctf-writer/stream-internal.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/compiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

static void environment_variable_destroy(struct environment_variable *var);
static void bt_ctf_writer_destroy(struct bt_ctf_ref *ref);

static const char * reserved_keywords_str[] = {"align", "callsite" "const",
	"char", "clock", "double", "enum", "env", "event", "floating_point",
	"float", "integer", "int", "long", "short", "signed", "stream",
	"string", "struct", "trace", "typealias", "typedef", "unsigned",
	"variant", "void" "_Bool", "_Complex", "_Imaginary"};
static GHashTable *reserved_keywords_set;
static int init_done;
static int reserved_refcount;

struct bt_ctf_writer *bt_ctf_writer_create(const char *path)
{
	struct bt_ctf_writer *writer = g_new0(struct bt_ctf_writer, 1);
	if (!writer) {
		goto error;
	}
	writer->trace_dir_fd = -1;
	bt_ctf_ref_init(&writer->ref_count, bt_ctf_writer_destroy);

	if (path) {
		int ret;
		writer->path = g_string_new(path);
		if (!writer->path) {
			goto error_destroy;
		}
		ret = mkdir(path, S_IRWXU|S_IRWXG);
		if (ret && errno != EEXIST) {
			perror("mkdir");
			goto error_destroy;
		}
		writer->trace_dir = opendir(path);
		if (!writer->trace_dir) {
			perror("opendir");
			goto error_destroy;
		}
		writer->trace_dir_fd = dirfd(writer->trace_dir);
		if (writer->trace_dir_fd < 0) {
			perror("dirfd");
			goto error_destroy;
		}
	}
	writer->environment = g_ptr_array_new_with_free_func(
		(GDestroyNotify)environment_variable_destroy);
	writer->clocks = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_clock_put);
	writer->streams = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_stream_put);
	writer->stream_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_stream_class_put);
	if (!writer->environment || !writer->clocks ||
		!writer->stream_classes || !writer->streams) {
		goto error_destroy;
	}

	return writer;

error_destroy:
	bt_ctf_writer_destroy(&writer->ref_count);
	writer = NULL;
error:
	return writer;

}

static void bt_ctf_writer_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}
	struct bt_ctf_writer *writer = container_of(ref, struct bt_ctf_writer,
		ref_count);

	if (writer->path) {
		g_string_free(writer->path, TRUE);
	}

	if (writer->trace_dir_fd != -1) {
		close(writer->trace_dir_fd);
	}

	if (writer->trace_dir) {
		closedir(writer->trace_dir);
	}

	if (writer->environment) {
		g_ptr_array_free(writer->environment, TRUE);
	}

	if (writer->clocks) {
		g_ptr_array_free(writer->clocks, TRUE);
	}

	if (writer->streams) {
		g_ptr_array_free(writer->streams, TRUE);
	}

	if (writer->stream_classes) {
		g_ptr_array_free(writer->stream_classes, TRUE);
	}

	g_free(writer);
}

int bt_ctf_writer_add_stream(struct bt_ctf_writer *writer,
		struct bt_ctf_stream *stream)
{
	int ret = -1;
	if (!writer || !stream) {
		goto end;
	}

	int stream_class_found = 0;
	for (size_t i = 0; i < writer->stream_classes->len; i++) {
		if (writer->stream_classes->pdata[i] == stream->stream_class) {
			stream_class_found = 1;
		}
	}

	if (!stream_class_found) {
		if (bt_ctf_stream_class_set_id(stream->stream_class,
					       writer->next_stream_id++)) {
			goto end;
		}

		bt_ctf_stream_class_get(stream->stream_class);
		g_ptr_array_add(writer->stream_classes, stream->stream_class);
	}

	bt_ctf_stream_get(stream);
	g_ptr_array_add(writer->streams, stream);
	ret = 0;
end:
	return ret;
}

int bt_ctf_writer_add_environment_field(struct bt_ctf_writer *writer,
		const char *name,
		const char *value)
{
	if (!writer || !name || !value) {
		goto end;
	}
	struct environment_variable *var = g_new0(
		struct environment_variable, 1);
	if (!var) {
		goto end;
	}

	var->name = g_string_new(name);
	var->value = g_string_new(value);
	if (!var->name || !var->value) {
		goto error;
	}
	g_ptr_array_add(writer->environment, var);
	return 0;

error:
	g_free(var->name);
	g_free(var->value);
	g_free(var);
end:
	return -1;
}

int bt_ctf_writer_add_clock(struct bt_ctf_writer *writer,
		struct bt_ctf_clock *clock)
{
	int ret = 0;
	if (!writer || !clock) {
		ret = -1;
		goto end;
	}

	/* Check for duplicate clocks */
	struct search_query query = { .value = clock, .found = 0 };
	g_ptr_array_foreach(writer->clocks, value_exists, &query);
	if (query.found) {
		ret = -1;
		goto end;
	}
	bt_ctf_clock_get(clock);
	g_ptr_array_add(writer->clocks, clock);
end:
	return ret;
}

char *bt_ctf_writer_get_metadata_string(struct bt_ctf_writer *writer)
{
	/* TODO */
	return NULL;
}

int bt_ctf_writer_set_endianness(struct bt_ctf_writer *writer,
		enum bt_ctf_byte_order endianness)
{
	if (!writer || writer->locked ||
	    endianness < BT_CTF_BYTE_ORDER_NATIVE ||
	    endianness >= BT_CTF_BYTE_ORDER_END) {
		return -1;
	}
	/* This attribute can't change mid-trace */
	if (!writer->locked) {
		writer->endianness = endianness;
	}
	return 0;
}

void bt_ctf_writer_get(struct bt_ctf_writer *writer)
{
	if (!writer) {
		return;
	}
	bt_ctf_ref_get(&writer->ref_count);
}

void bt_ctf_writer_put(struct bt_ctf_writer *writer)
{
	if (!writer) {
		return;
	}
	bt_ctf_ref_put(&writer->ref_count);
}

int validate_identifier(const char *input_string)
{
	int ret = -1;
	char *string = NULL;
	if (!input_string || input_string[0] == '\0') {
		goto end;
	}

	string = strdup(input_string);
	if (!string) {
		goto end;
	}

	char *save_ptr, *token = strtok_r(string, " ", &save_ptr);
	while (token) {
		if (g_hash_table_contains(reserved_keywords_set,
			GINT_TO_POINTER(g_quark_from_string(token)))) {
			goto end;
		}
		token = strtok_r(NULL, " ", &save_ptr);
	}
	ret = 0;
end:
	free(string);
	return ret;
}

static void environment_variable_destroy(struct environment_variable *var)
{
	g_string_free(var->name, TRUE);
	g_string_free(var->value, TRUE);
	g_free(var);
}

static __attribute__((constructor))
void writer_init(void)
{
	reserved_refcount++;
	if (init_done) {
		return;
	}
	reserved_keywords_set = g_hash_table_new(g_direct_hash, g_direct_equal);

	const size_t reserved_keywords_count =
		sizeof(reserved_keywords_str) / sizeof(char *);
	for (size_t i = 0; i < reserved_keywords_count; i++) {
		g_hash_table_add(reserved_keywords_set,
		GINT_TO_POINTER(g_quark_from_string(reserved_keywords_str[i])));
	}
	init_done = 1;
}

static __attribute__((destructor))
void writer_finalize(void)
{
	if (--reserved_refcount == 0) {
		g_hash_table_destroy(reserved_keywords_set);
	}
}
