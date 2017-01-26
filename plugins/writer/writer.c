/*
 * writer.c
 *
 * Babeltrace CTF Writer Output Plugin
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/plugin/plugin-dev.h>
#include <babeltrace/component/component.h>
#include <babeltrace/component/component-sink.h>
#include <babeltrace/component/notification/notification.h>
#include <babeltrace/component/notification/iterator.h>
#include <babeltrace/component/notification/event.h>
#include <babeltrace/component/notification/packet.h>
#include <plugins-common.h>
#include <stdio.h>
#include <stdbool.h>
#include <glib.h>
#include "writer.h"

static
void destroy_writer_component_data(struct writer_component *writer_component)
{
	g_hash_table_destroy(writer_component->stream_map);
	g_hash_table_destroy(writer_component->stream_class_map);
	g_hash_table_destroy(writer_component->trace_map);
	g_string_free(writer_component->base_path, true);
	g_string_free(writer_component->trace_name_base, true);
}

static
void destroy_writer_component(struct bt_component *component)
{
	struct writer_component *writer_component = (struct writer_component *)
		bt_component_get_private_data(component);

	destroy_writer_component_data(writer_component);
	g_free(writer_component);
}

static
void unref_stream_class(struct bt_ctf_stream_class *writer_stream_class)
{
	BT_PUT(writer_stream_class);
	g_free(writer_stream_class);
}

static
void unref_stream(struct bt_ctf_stream_class *writer_stream)
{
	BT_PUT(writer_stream);
	g_free(writer_stream);
}

static
void unref_trace(struct bt_ctf_writer *writer)
{
	BT_PUT(writer);
	g_free(writer);
}

static
struct writer_component *create_writer_component(void)
{
	struct writer_component *writer_component;

	writer_component = g_new0(struct writer_component, 1);
	if (!writer_component) {
		goto end;
	}

	writer_component->err = stderr;
	writer_component->trace_id = 0;
	writer_component->trace_name_base = g_string_new("trace");
	if (!writer_component->trace_name_base) {
		g_free(writer_component);
		writer_component = NULL;
		goto end;
	}

	/*
	 * Reader to writer corresponding structures.
	 */
	writer_component->trace_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_trace);
	writer_component->stream_class_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_stream_class);
	writer_component->stream_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_stream);

end:
	return writer_component;
}

static
enum bt_component_status handle_notification(
		struct writer_component *writer_component,
		struct bt_notification *notification)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!writer_component) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	switch (bt_notification_get_type(notification)) {
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
	{
		struct bt_ctf_packet *packet =
			bt_notification_packet_begin_get_packet(notification);

		if (!packet) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}

		ret = writer_new_packet(writer_component, packet);
		bt_put(packet);
		break;
	}
	case BT_NOTIFICATION_TYPE_PACKET_END:
	{
		struct bt_ctf_packet *packet =
			bt_notification_packet_end_get_packet(notification);

		if (!packet) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		ret = writer_close_packet(writer_component, packet);
		bt_put(packet);
		break;
	}
	case BT_NOTIFICATION_TYPE_EVENT:
	{
		struct bt_ctf_event *event = bt_notification_event_get_event(
				notification);

		if (!event) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		ret = BT_COMPONENT_STATUS_OK;
		ret = writer_output_event(writer_component, event);
		bt_put(event);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
		break;
	}
	case BT_NOTIFICATION_TYPE_STREAM_END:
		break;
	default:
		puts("Unhandled notification type");
	}
end:
	return ret;
}

static
enum bt_component_status run(struct bt_component *component)
{
	enum bt_component_status ret;
	struct bt_notification *notification = NULL;
	struct bt_notification_iterator *it;
	struct writer_component *writer_component =
		bt_component_get_private_data(component);

	ret = bt_component_sink_get_input_iterator(component, 0, &it);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	notification = bt_notification_iterator_get_notification(it);
	if (!notification) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	ret = bt_notification_iterator_next(it);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = handle_notification(writer_component, notification);
end:
	bt_put(it);
	bt_put(notification);
	return ret;
}

static
enum bt_component_status writer_component_init(
	struct bt_component *component, struct bt_value *params,
	UNUSED_VAR void *init_method_data)
{
	enum bt_component_status ret;
	enum bt_value_status value_ret;
	struct writer_component *writer_component = create_writer_component();
	struct bt_value *value = NULL;
	const char *path;

	if (!writer_component) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	value = bt_value_map_get(params, "path");
	if (!value || bt_value_is_null(value) || !bt_value_is_string(value)) {
		fprintf(writer_component->err,
				"[error] output path parameter required\n");
		ret = BT_COMPONENT_STATUS_INVALID;
		goto error;
	}

	value_ret = bt_value_string_get(value, &path);
	if (value_ret != BT_VALUE_STATUS_OK) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto error;
	}

	writer_component->base_path = g_string_new(path);
	if (!writer_component) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto error;
	}

	ret = bt_component_set_private_data(component, writer_component);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

end:
	return ret;
error:
	destroy_writer_component_data(writer_component);
	g_free(writer_component);
	return ret;
}

/* Initialize plug-in entry points. */
BT_PLUGIN(writer);
BT_PLUGIN_DESCRIPTION("Babeltrace CTF-Writer output plug-in.");
BT_PLUGIN_AUTHOR("Jérémie Galarneau");
BT_PLUGIN_LICENSE("MIT");
BT_PLUGIN_SINK_COMPONENT_CLASS(writer, run);
BT_PLUGIN_SINK_COMPONENT_CLASS_INIT_METHOD(writer, writer_component_init);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESTROY_METHOD(writer, destroy_writer_component);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(writer, "Formats CTF-IR to CTF.");
