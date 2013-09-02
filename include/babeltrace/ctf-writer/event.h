#ifndef _BABELTRACE_CTF_EVENT_H
#define _BABELTRACE_CTF_EVENT_H

/*
 * BabelTrace - CTF Event
 *
 * CTF Writer
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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_event_class;
struct bt_ctf_event;
struct bt_ctf_field;
struct bt_ctf_field_type;

extern struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name);

extern int bt_ctf_event_class_add_field(struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *type,
		const char *name);

extern void bt_ctf_event_class_get(struct bt_ctf_event_class *event_class);

extern void bt_ctf_event_class_put(struct bt_ctf_event_class *event_class);

extern struct bt_ctf_event *bt_ctf_event_create(
		struct bt_ctf_event_class *event_class);

extern int bt_ctf_event_set_payload(struct bt_ctf_event *event,
		const char *name,
		struct bt_ctf_field *value);

extern struct bt_ctf_field *bt_ctf_event_get_payload(struct bt_ctf_event *event,
		const char *name);

extern void bt_ctf_event_get(struct bt_ctf_event *event);

extern void bt_ctf_event_put(struct bt_ctf_event *event);

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_CTF_EVENT_H */
