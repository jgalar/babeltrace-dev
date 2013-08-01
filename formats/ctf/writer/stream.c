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
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-writer/stream-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/compiler.h>

static void bt_ctf_stream_destroy(struct bt_ctf_ref *ref);
static void bt_ctf_stream_class_destroy(struct bt_ctf_ref *ref);

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
	int ret = stream_class->id_set;
	if (stream_class->id_set) {
		goto end;
	}

	stream_class->id = id;
	stream_class->id_set = 1;
end:
	return ret;
}

static void bt_ctf_stream_class_destroy(struct bt_ctf_ref *ref)
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
	bt_ctf_stream_class_put(stream_class);
	stream->stream_class = stream_class;
	bt_ctf_stream_class_lock(stream_class);
end:
	return stream;
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
	return -1;
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

static void bt_ctf_stream_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}
	struct bt_ctf_stream *stream = container_of(ref,
		struct bt_ctf_stream, ref_count);
	bt_ctf_stream_class_put(stream->stream_class);
	g_free(stream);
}
