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
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/ctf-writer/event-types-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/ctf-writer/stream-internal.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/compiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#define RESERVED_IDENTIFIER_SIZE 128
#define RESERVED_METADATA_STRING_SIZE 4096

static void environment_variable_destroy(struct environment_variable *var);
static void bt_ctf_writer_destroy(struct bt_ctf_ref *ref);
static int init_packet_header_type(struct bt_ctf_writer *writer);

static const char * const reserved_keywords_str[] = {"align", "callsite",
	"const", "char", "clock", "double", "enum", "env", "event",
	"floating_point", "float", "integer", "int", "long", "short", "signed",
	"stream", "string", "struct", "trace", "typealias", "typedef",
	"unsigned", "variant", "void" "_Bool", "_Complex", "_Imaginary"};

static const unsigned int field_type_aliases_alignments[] = {
	[FIELD_TYPE_ALIAS_UINT5_T] = 1,
	[FIELD_TYPE_ALIAS_UINT8_T ... FIELD_TYPE_ALIAS_UINT16_T] = 8,
	[FIELD_TYPE_ALIAS_UINT27_T] = 1,
	[FIELD_TYPE_ALIAS_UINT32_T ... FIELD_TYPE_ALIAS_UINT64_T] = 8
};

static const unsigned int field_type_aliases_sizes[] = {
	[FIELD_TYPE_ALIAS_UINT5_T] = 5,
	[FIELD_TYPE_ALIAS_UINT8_T] = 8,
	[FIELD_TYPE_ALIAS_UINT16_T] = 16,
	[FIELD_TYPE_ALIAS_UINT27_T] = 27,
	[FIELD_TYPE_ALIAS_UINT32_T] = 32,
	[FIELD_TYPE_ALIAS_UINT64_T] = 64
};

static GHashTable *reserved_keywords_set;
static GPtrArray *field_type_aliases;
static int init_done;
static int global_data_refcount;

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

	/* Generate a trace UUID */
	uuid_generate(writer->uuid);

	if (init_packet_header_type(writer)) {
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

	bt_ctf_field_type_put(writer->packet_header_type);
	bt_ctf_field_put(writer->packet_header);
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

const char *get_byte_order_string(enum bt_ctf_byte_order byte_order)
{
	switch (byte_order) {
	case BT_CTF_BYTE_ORDER_NATIVE:
		return G_BYTE_ORDER == G_LITTLE_ENDIAN ? "le" : "be";
	case BT_CTF_BYTE_ORDER_LITTLE_ENDIAN:
		return "le";
	case BT_CTF_BYTE_ORDER_BIG_ENDIAN:
	case BT_CTF_BYTE_ORDER_NETWORK:
		return "be";
	default:
		return "unknown";
	}
}

static void append_trace_metadata(struct bt_ctf_writer *writer,
		struct metadata_context *context)
{
	g_string_append(context->string, "trace {\n");

	g_string_append(context->string, "\tmajor = 1;\n");
	g_string_append(context->string, "\tminor = 8;\n");

	unsigned char *uuid = writer->uuid;
	g_string_append_printf(context->string,
		"\tuuid = \"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\";\n",
		uuid[0], uuid[1], uuid[2], uuid[3],
		uuid[4], uuid[5], uuid[6], uuid[7],
		uuid[8], uuid[9], uuid[10], uuid[11],
		uuid[12], uuid[13], uuid[14], uuid[15]);
	g_string_append_printf(context->string, "\tbyte_order = %s;\n",
		get_byte_order_string(writer->byte_order));

	g_string_append(context->string, "\tpacket.header := ");
	context->current_indentation_level++;
	g_string_assign(context->field_name, "");
	bt_ctf_field_type_serialize(writer->packet_header_type, context);
	context->current_indentation_level--;

	g_string_append(context->string, ";\n};\n\n");
}

static void append_env_field_metadata(struct environment_variable *var,
		struct metadata_context *context)
{
	g_string_append_printf(context->string, "\t%s = \"%s\";\n",
		var->name->str, var->value->str);
}

static void append_env_metadata(struct bt_ctf_writer *writer,
		struct metadata_context *context)
{
	if (writer->environment->len == 0) {
		return;
	}

	g_string_append(context->string, "env {\n");
	g_ptr_array_foreach(writer->environment,
		(GFunc)append_env_field_metadata, context);
	g_string_append(context->string, "};\n\n");
}

char *bt_ctf_writer_get_metadata_string(struct bt_ctf_writer *writer)
{
	char *metadata = NULL;
	struct metadata_context *context = g_new0(struct metadata_context, 1);
	if (!context) {
		goto end;
	}

	context->field_name = g_string_sized_new(RESERVED_IDENTIFIER_SIZE);
	context->string = g_string_sized_new(RESERVED_METADATA_STRING_SIZE);
	g_string_append(context->string, "/* CTF 1.8 */\n\n");
	append_trace_metadata(writer, context);
	append_env_metadata(writer, context);
	g_ptr_array_foreach(writer->clocks,
		(GFunc)bt_ctf_clock_serialize, context);

	for (size_t i = 0; i < writer->stream_classes->len; i++) {
		int err = bt_ctf_stream_class_serialize(
			writer->stream_classes->pdata[i], context);
		if (err) {
			goto error;
		}
	}

	metadata = context->string->str;
	g_string_free(context->string, FALSE);
end:
	return metadata;
error:
	g_string_free(context->string, TRUE);
	g_free(context);
	return metadata;
}

int bt_ctf_writer_set_byte_order(struct bt_ctf_writer *writer,
		enum bt_ctf_byte_order byte_order)
{
	if (!writer || writer->locked ||
	    byte_order < BT_CTF_BYTE_ORDER_NATIVE ||
	    byte_order >= BT_CTF_BYTE_ORDER_END) {
		return -1;
	}
	/* This attribute can't change mid-trace */
	if (!writer->locked) {
		writer->byte_order = byte_order;
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

struct bt_ctf_field_type *get_field_type(enum field_type_alias alias)
{
	if (alias >= FIELD_TYPE_ALIAS_END) {
		return NULL;
	}
	return field_type_aliases->pdata[alias];
}

int init_packet_header_type(struct bt_ctf_writer *writer)
{
	int ret = 0;

	struct bt_ctf_field_type *_uint32_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT32_T);
	struct bt_ctf_field_type *_uint8_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT8_T);
	struct bt_ctf_field_type *packet_header_type =
		bt_ctf_field_type_structure_create();
	struct bt_ctf_field_type *uuid_array_type =
		bt_ctf_field_type_array_create(_uint8_t, 16);

	if (!packet_header_type || !uuid_array_type) {
		ret = -1;
		goto end;
	}

	ret |= bt_ctf_field_type_structure_add_field(packet_header_type,
		_uint32_t, "magic");
	ret |= bt_ctf_field_type_structure_add_field(packet_header_type,
		uuid_array_type, "uuid");
	ret |= bt_ctf_field_type_structure_add_field(packet_header_type,
		_uint32_t, "stream_id");
	if (ret) {
		goto end;
	}

	writer->packet_header_type = packet_header_type;
	ret = 0;

end:
	bt_ctf_field_type_put(uuid_array_type);
	if (ret) {
		bt_ctf_field_type_put(packet_header_type);
	}

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
	global_data_refcount++;
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

	field_type_aliases = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_ctf_field_type_put);
	for (size_t i = 0; i < FIELD_TYPE_ALIAS_END; i++) {
		unsigned int alignment = field_type_aliases_alignments[i];
		unsigned int size = field_type_aliases_sizes[i];
		struct bt_ctf_field_type *field_type =
			bt_ctf_field_type_integer_create(size);
		bt_ctf_field_type_set_alignment(field_type, alignment);
		g_ptr_array_add(field_type_aliases, field_type);

		if (!field_type) {
			fprintf(stderr, "[error] field type alias initialization failed.\n");
		} else {
			/* These type definitions are immutable */
			bt_ctf_field_type_lock(field_type);
		}
	}

	init_done = 1;
}

static __attribute__((destructor))
void writer_finalize(void)
{
	if (--global_data_refcount == 0) {
		g_hash_table_destroy(reserved_keywords_set);
		g_ptr_array_free(field_type_aliases, TRUE);
	}
}
