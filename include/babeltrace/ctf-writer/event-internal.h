#ifndef _BABELTRACE_CTF_WRITER_EVENT_INTERNAL_H
#define _BABELTRACE_CTF_WRITER_EVENT_INTERNAL_H

/*
 * BabelTrace - CTF Writer
 *
 * CTF Writer Event
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

#include <babeltrace/ctf-writer/ref-internal.h>
#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/babeltrace-internal.h>
#include <glib.h>

struct bt_ctf_event_class {
	struct bt_ctf_ref ref_count;
	GQuark name;
	int id_set;
	uint32_t id;
	int stream_id_set;
	uint32_t stream_id;
	/* Structure type containing the event's context */
	struct bt_ctf_field_type *context;
	/* Structure type containing the event's fields */
	struct bt_ctf_field_type *fields;
	int locked;
};

struct bt_ctf_event {
	struct bt_ctf_ref ref_count;
	struct bt_ctf_event_class *event_class;
	struct bt_ctf_field *context_payload;
	struct bt_ctf_field *fields_payload;
};

BT_HIDDEN
void bt_ctf_event_class_lock(struct bt_ctf_event_class *event_class);

BT_HIDDEN
int bt_ctf_event_class_set_id(struct bt_ctf_event_class *event_class,
		uint32_t id);

BT_HIDDEN
int bt_ctf_event_class_set_stream_id(struct bt_ctf_event_class *event_class,
		uint32_t id);

BT_HIDDEN
int bt_ctf_event_class_serialize(struct bt_ctf_event_class *event_class,
		struct metadata_context *context);

#endif /* _BABELTRACE_CTF_WRITER_EVENT_INTERNAL_H */
