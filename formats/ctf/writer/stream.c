/*
 * stream.c
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
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/event-internal.h>
#include <babeltrace/ctf-writer/event-types-internal.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-writer/stream-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/compiler.h>

static void bt_ctf_stream_destroy(struct bt_ctf_ref *ref);
static void bt_ctf_stream_class_destroy(struct bt_ctf_ref *ref);
static int init_event_header(struct bt_ctf_stream_class *stream_class);
static int init_packet_context(struct bt_ctf_stream_class *stream_class);

struct bt_ctf_stream_class *bt_ctf_stream_class_create(void)
{
	struct bt_ctf_stream_class *stream_class =  g_new0(struct
		bt_ctf_stream_class, 1);
	if (!stream_class) {
		goto error;
	}

	stream_class->event_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_event_class_put);
	if (!stream_class->event_classes) {
		goto error_destroy;
	}

	if (init_event_header(stream_class)) {
		goto error_destroy;
	}

	if (init_packet_context(stream_class)) {
		goto error_destroy;
	}

	bt_ctf_ref_init(&stream_class->ref_count, bt_ctf_stream_class_destroy);
	return stream_class;

error_destroy:
	bt_ctf_stream_class_destroy(&stream_class->ref_count);
	stream_class = NULL;
error:
	return stream_class;
}

int bt_ctf_stream_class_set_clock(struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_clock *clock)
{
	int ret = 0;
	if (!stream_class || !clock || stream_class->locked) {
		ret = -1;
		goto end;
	}

	if (stream_class->clock) {
		bt_ctf_clock_put(stream_class->clock);
	}

	stream_class->clock = clock;
	bt_ctf_clock_get(clock);
end:
	return ret;
}

int bt_ctf_stream_class_add_event_class(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_event_class *event_class)
{
	int ret = -1;
	if (!stream_class || !event_class) {
		goto end;
	}

	/* Check for duplicate event classes */
	struct search_query query =
		{ .value = event_class, .found = 0 };
	g_ptr_array_foreach(stream_class->event_classes, value_exists, &query);
	if (query.found) {
		goto end;
	}

	if (bt_ctf_event_class_set_id(event_class,
		stream_class->next_event_id++)) {
		/* The event is already associated to a stream class */
		goto end;
	}

	bt_ctf_event_class_get(event_class);
	g_ptr_array_add(stream_class->event_classes, event_class);
	ret = 0;
end:
	return ret;
}

void bt_ctf_stream_class_get(struct bt_ctf_stream_class *stream_class)
{
	if (!stream_class) {
		return;
	}
	bt_ctf_ref_get(&stream_class->ref_count);
}

void bt_ctf_stream_class_put(struct bt_ctf_stream_class *stream_class)
{
	if (!stream_class) {
		return;
	}
	bt_ctf_ref_put(&stream_class->ref_count);
}

void bt_ctf_stream_class_lock(struct bt_ctf_stream_class *stream_class)
{
	if (!stream_class) {
		return;
	}

	stream_class->locked = 1;
	bt_ctf_clock_lock(stream_class->clock);
	g_ptr_array_foreach(stream_class->event_classes,
		(GFunc)bt_ctf_event_class_lock, NULL);
}

int bt_ctf_stream_class_set_id(struct bt_ctf_stream_class *stream_class,
	uint32_t id)
{
	int ret = 0;
	if (stream_class->id_set && (id != stream_class->id)) {
		ret = -1;
		goto end;
	}

	stream_class->id = id;
	stream_class->id_set = 1;
end:
	return ret;
}

int bt_ctf_stream_class_serialize(struct bt_ctf_stream_class *stream_class,
		struct metadata_context *context)
{
	int ret = 0;
	g_string_assign(context->field_name, "");
	context->current_indentation_level = 1;
	if (!stream_class->id_set) {
		ret = -1;
		goto end;
	}

	g_string_append_printf(context->string,
		"stream {\n\tid = %u;\n\tevent.header := ", stream_class->id);
	ret = bt_ctf_field_type_serialize(stream_class->event_header, context);
	if (ret) {
		goto end;
	}

	g_string_append(context->string, ";\n\n\tpacket.context := ");
	ret = bt_ctf_field_type_serialize(stream_class->packet_context, context);
	if (ret) {
		goto end;
	}

	if (stream_class->event_context) {
		g_string_append(context->string, ";\n\n\tevent.context := ");
		ret = bt_ctf_field_type_serialize(stream_class->event_context, context);
		if (ret) {
			goto end;
		}
	}

	g_string_append(context->string, ";\n};\n\n");

	/* Assign this stream's ID to every event and serialize them */
	g_ptr_array_foreach(stream_class->event_classes,
		(GFunc) bt_ctf_event_class_set_stream_id,
		GUINT_TO_POINTER(stream_class->id));
	for (size_t i = 0; i < stream_class->event_classes->len; i++) {
		struct bt_ctf_event_class *event_class =
			stream_class->event_classes->pdata[i];
		ret = bt_ctf_event_class_set_stream_id(event_class,
			stream_class->id);
		if (ret) {
			goto end;
		}

		ret = bt_ctf_event_class_serialize(event_class, context);
		if (ret) {
			goto end;
		}
	}
end:
	context->current_indentation_level = 0;
	return ret;
}

void bt_ctf_stream_class_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}

	struct bt_ctf_stream_class *stream_class = container_of(
		ref, struct bt_ctf_stream_class, ref_count);
	bt_ctf_clock_put(stream_class->clock);

	if (stream_class->event_classes) {
		g_ptr_array_free(stream_class->event_classes, TRUE);
	}

	bt_ctf_field_type_put(stream_class->event_header);
	bt_ctf_field_type_put(stream_class->packet_context);
	bt_ctf_field_type_put(stream_class->event_context);
	g_free(stream_class);
}

struct bt_ctf_stream *bt_ctf_stream_create(
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_stream *stream = NULL;
	if (!stream_class) {
		goto end;
	}

	stream = g_new0(struct bt_ctf_stream, 1);
	if (!stream) {
		goto end;
	}

	bt_ctf_ref_init(&stream->ref_count, bt_ctf_stream_destroy);
	bt_ctf_stream_class_get(stream_class);
	stream->stream_class = stream_class;
	bt_ctf_stream_class_lock(stream_class);
end:
	return stream;
}

int bt_ctf_stream_set_flush_callback(struct bt_ctf_stream *stream,
	flush_func callback, void *data)
{
	int ret = stream ? 0 : -1;
	if (!stream) {
		goto end;
	}

	stream->flush.func = callback;
	stream->flush.data = data;
end:
	return ret;
}

void bt_ctf_stream_push_discarded_events(struct bt_ctf_stream *stream,
		uint64_t event_count)
{
	/* TODO */
}

int bt_ctf_stream_push_event(struct bt_ctf_stream *stream,
		struct bt_ctf_event *event)
{
	return -1;
}

int bt_ctf_stream_flush(struct bt_ctf_stream *stream)
{
	int ret = -1;
	if (!stream) {
		goto end;
	}

	if (stream->flush.func) {
		stream->flush.func(stream, stream->flush.data);
	}
end:
	return ret;
}

void bt_ctf_stream_get(struct bt_ctf_stream *stream)
{
	if (!stream) {
		return;
	}
	bt_ctf_ref_get(&stream->ref_count);
}

void bt_ctf_stream_put(struct bt_ctf_stream *stream)
{
	if (!stream) {
		return;
	}
	bt_ctf_ref_put(&stream->ref_count);
}

void bt_ctf_stream_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}

	struct bt_ctf_stream *stream = container_of(ref,
		struct bt_ctf_stream, ref_count);
	bt_ctf_stream_class_put(stream->stream_class);
	bt_ctf_field_put(stream->event_context_payload);
	g_free(stream);
}

int init_event_header(struct bt_ctf_stream_class *stream_class)
{
	int ret = 0;

	/* We create the large event header proposed in the CTF specification */
	struct bt_ctf_field_type *event_header =
		bt_ctf_field_type_structure_create();
	struct bt_ctf_field_type *_uint16_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT16_T);
	struct bt_ctf_field_type *_uint32_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT32_T);
	struct bt_ctf_field_type *_uint64_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT64_T);
	struct bt_ctf_field_type *v_variant =
		bt_ctf_field_type_variant_create("id");
	struct bt_ctf_field_type *id_enum =
		bt_ctf_field_type_enumeration_create(_uint16_t);
	struct bt_ctf_field_type *compact_structure =
		bt_ctf_field_type_structure_create();
	struct bt_ctf_field_type *extended_structure =
		bt_ctf_field_type_structure_create();

	if (!event_header || !v_variant || !id_enum) {
		ret = -1;
		goto end;
	}

	ret |= bt_ctf_field_type_enumeration_add_mapping(id_enum, "compact", 0,
		65534);
	ret |= bt_ctf_field_type_enumeration_add_mapping(id_enum, "extended",
		65535, 65535);
	ret |= bt_ctf_field_type_structure_add_field(event_header, id_enum,
		"id");
	ret |= bt_ctf_field_type_structure_add_field(compact_structure,
		_uint32_t, "timestamp");
	ret |= bt_ctf_field_type_structure_add_field(extended_structure,
		_uint32_t, "id");
	ret |= bt_ctf_field_type_structure_add_field(extended_structure,
		_uint64_t, "timestamp");
	ret |= bt_ctf_field_type_variant_add_field(v_variant,
		compact_structure, "compact");
	ret |= bt_ctf_field_type_variant_add_field(v_variant,
		extended_structure, "extended");
	ret |= bt_ctf_field_type_structure_add_field(event_header, v_variant,
		"v");

	if (ret) {
		bt_ctf_field_type_put(event_header);
		goto end;
	}
	stream_class->event_header = event_header;
end:
	bt_ctf_field_type_put(v_variant);
	bt_ctf_field_type_put(id_enum);
	bt_ctf_field_type_put(compact_structure);
	bt_ctf_field_type_put(extended_structure);
	return ret;
}

int init_packet_context(struct bt_ctf_stream_class *stream_class)
{
	int ret = 0;

	/* We create the packet context proposed in the CTF specification */
	struct bt_ctf_field_type *packet_context =
		bt_ctf_field_type_structure_create();
	struct bt_ctf_field_type *_uint32_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT32_T);
	struct bt_ctf_field_type *_uint64_t =
		get_field_type(FIELD_TYPE_ALIAS_UINT64_T);

	if (!packet_context) {
		ret = -1;
		goto end;
	}

	ret |= bt_ctf_field_type_structure_add_field(packet_context,
		_uint64_t, "timestamp_begin");
	ret |= bt_ctf_field_type_structure_add_field(packet_context,
		_uint64_t, "timestamp_end");
	ret |= bt_ctf_field_type_structure_add_field(packet_context,
		_uint64_t, "content_size");
	ret |= bt_ctf_field_type_structure_add_field(packet_context,
		_uint64_t, "packet_size");
	ret |= bt_ctf_field_type_structure_add_field(packet_context,
		_uint64_t, "events_discarded");
	ret |= bt_ctf_field_type_structure_add_field(packet_context,
		_uint32_t, "cpu_id");

	if (ret) {
		bt_ctf_field_type_put(packet_context);
		goto end;
	}
	stream_class->packet_context = packet_context;
end:
	return ret;
}
