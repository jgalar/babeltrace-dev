/*
 * event.c
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

#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-writer/event-fields-internal.h>
#include <babeltrace/ctf-writer/event-types-internal.h>
#include <babeltrace/ctf-writer/event-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/compiler.h>

static void bt_ctf_event_class_destroy(struct bt_ctf_ref *ref);
static void bt_ctf_event_destroy(struct bt_ctf_ref *ref);

struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name)
{
	struct bt_ctf_event_class *event_class = NULL;
	if (validate_identifier(name)) {
		goto end;
	}

	event_class = g_new0(struct bt_ctf_event_class, 1);
	if (!event_class) {
		goto end;
	}

	bt_ctf_ref_init(&event_class->ref_count, bt_ctf_event_class_destroy);
	event_class->name = g_quark_from_string(name);
end:
	return event_class;
}

int bt_ctf_event_class_add_field(struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *type,
		const char *name)
{
	int ret = -1;
	if (!event_class || !type || validate_identifier(name) ||
		event_class->locked) {
		goto end;
	}

	if (!event_class->fields) {
		event_class->fields = bt_ctf_field_type_structure_create();
		if (!event_class->fields) {
			goto end;
		}
	}

	ret = bt_ctf_field_type_structure_add_field(event_class->fields,
		type, name);
end:
	return ret;
}

void bt_ctf_event_class_get(struct bt_ctf_event_class *event_class)
{
	if (!event_class) {
		return;
	}
	bt_ctf_ref_get(&event_class->ref_count);
}

void bt_ctf_event_class_put(struct bt_ctf_event_class *event_class)
{
	if (!event_class) {
		return;
	}
	bt_ctf_ref_put(&event_class->ref_count);
}

struct bt_ctf_event *bt_ctf_event_create(struct bt_ctf_event_class *event_class)
{
	struct bt_ctf_event *event = NULL;
	if (!event_class) {
		goto end;
	}

	event = g_new0(struct bt_ctf_event, 1);
	if (!event) {
		goto end;
	}

	bt_ctf_ref_init(&event->ref_count, bt_ctf_event_destroy);
	bt_ctf_event_class_get(event_class);
	bt_ctf_event_class_lock(event_class);
	event->event_class = event_class;
	event->context_payload = bt_ctf_field_create(event_class->context);
	event->fields_payload = bt_ctf_field_create(event_class->fields);
end:
	return event;
}

int bt_ctf_event_set_payload(struct bt_ctf_event *event,
		const char *name,
		struct bt_ctf_field *value)
{
	int ret = -1;
	if (!event || !value || validate_identifier(name)) {
		goto end;
	}

	ret = bt_ctf_field_structure_set_field(event->fields_payload,
		name, value);
end:
	return ret;
}

void bt_ctf_event_get(struct bt_ctf_event *event)
{
	if (!event) {
		return;
	}
	bt_ctf_ref_get(&event->ref_count);
}

void bt_ctf_event_put(struct bt_ctf_event *event)
{
	if (!event) {
		return;
	}
	bt_ctf_ref_put(&event->ref_count);
}

void bt_ctf_event_class_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}
	struct bt_ctf_event_class *event_class = container_of(ref,
		struct bt_ctf_event_class, ref_count);
	bt_ctf_field_type_put(event_class->context);
	bt_ctf_field_type_put(event_class->fields);
	g_free(event_class);
}

void bt_ctf_event_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}

	struct bt_ctf_event *event = container_of(ref, struct bt_ctf_event,
		ref_count);
	bt_ctf_event_class_put(event->event_class);
	bt_ctf_field_put(event->context_payload);
	bt_ctf_field_put(event->fields_payload);
	g_free(event);
}

void bt_ctf_event_class_lock(struct bt_ctf_event_class *event_class)
{
	event_class->locked = 1;
	bt_ctf_field_type_lock(event_class->context);
	bt_ctf_field_type_lock(event_class->fields);

}

int bt_ctf_event_class_set_id(struct bt_ctf_event_class *event_class,
		uint32_t id)
{
	int ret = event_class->id_set;
	if (event_class->id_set) {
		goto end;
	}

	event_class->id = id;
	event_class->id_set = 1;
end:
	return ret;
}
