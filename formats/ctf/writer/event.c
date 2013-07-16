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
#include <babeltrace/ctf-writer/event-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/compiler.h>

static void bt_ctf_event_class_destroy(struct bt_ctf_ref *ref);
static void bt_ctf_event_destroy(struct bt_ctf_ref *ref);
static void bt_ctf_event_class_lock(struct bt_ctf_event_class *event_class);
static void destroy_field_type_entry(struct field_type_entry *entry);
static void destroy_field_entry(struct field_entry *entry);

struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name)
{
	struct bt_ctf_event_class *event_class = NULL;
	if (!name) {
		goto end;
	}

	event_class = g_new0(struct bt_ctf_event_class, 1);
	if (!event_class) {
		goto end;
	}

	bt_ctf_ref_init(&event_class->ref_count, bt_ctf_event_class_destroy);
	event_class->name = g_quark_from_string(name);
	event_class->field_name_to_index = g_hash_table_new(NULL, NULL);
	event_class->fields = g_ptr_array_new_with_free_func(
		(GDestroyNotify)destroy_field_type_entry);
end:
	return event_class;
}

int bt_ctf_event_class_add_field(struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *type,
		const char *name)
{
	int ret = -1;
	if (!event_class || !type || !name || validate_identifier(name) ||
		event_class->locked) {
		goto end;
	}

	GQuark name_quark = g_quark_from_string(name);
	if (g_hash_table_contains(event_class->field_name_to_index,
			GUINT_TO_POINTER(name_quark))) {
		goto end;
	}

	struct field_type_entry *entry = g_new0(struct field_type_entry, 1);
	if (!entry) {
		goto end;
	}

	entry->name = name_quark;
	entry->field_type = type;
	bt_ctf_field_type_get(type);
	g_hash_table_insert(event_class->field_name_to_index,
		(gpointer) (unsigned long) entry->name,
		(gpointer) (unsigned long) event_class->fields->len);
	g_ptr_array_add(event_class->fields, entry);
	ret = 0;
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
	event->field_name_to_index = event_class->field_name_to_index;
	event->fields = g_ptr_array_new_full(event_class->fields->len,
		(GDestroyNotify)destroy_field_entry);
	g_ptr_array_set_size(event->fields, event_class->fields->len);
end:
	return event;
}

int bt_ctf_event_set_payload(struct bt_ctf_event *event,
		const char *name,
		struct bt_ctf_field *value)
{
	int ret = -1;
	if (!event || !name || !value || validate_identifier(name)) {
		goto end;
	}

	GQuark name_quark = g_quark_from_string(name);
	gpointer result;
	if (!g_hash_table_lookup_extended(event->field_name_to_index,
			GUINT_TO_POINTER(name_quark), NULL, &result)) {
		goto end;
	}

	size_t index = (size_t)GPOINTER_TO_UINT(result);
	struct field_type_entry *type_entry =
		event->event_class->fields->pdata[index];
	if (value->type !=
		type_entry->field_type) {
		goto end;
	}

	struct field_entry *entry = g_new0(struct field_entry, 1);
	if (!entry) {
		goto end;
	}

	entry->name = name_quark;
	entry->field = value;
	bt_ctf_field_get(value);
	ret = 0;
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
	g_ptr_array_free(event_class->fields, TRUE);
	g_hash_table_destroy(event_class->field_name_to_index);
	g_free(event_class);
}

void bt_ctf_event_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}
	struct bt_ctf_event *event = container_of(ref, struct bt_ctf_event,
		ref_count);
	g_ptr_array_free(event->fields, TRUE);
	g_hash_table_destroy(event->field_name_to_index);
	bt_ctf_event_class_put(event->event_class);
	g_free(event);
}

void bt_ctf_event_class_lock(struct bt_ctf_event_class *event_class)
{
	event_class->locked = 1;
}

void destroy_field_type_entry(struct field_type_entry *entry)
{
	if (!entry) {
		return;
	}
	
	bt_ctf_field_type_put(entry->field_type);
	g_free(entry);
}

void destroy_field_entry(struct field_entry *entry)
{
	if (!entry) {
		return;
	}

	bt_ctf_field_put(entry->field);
	g_free(entry);
}
