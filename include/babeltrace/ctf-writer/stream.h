#ifndef _BABELTRACE_CTF_STREAM_H
#define _BABELTRACE_CTF_STREAM_H

/*
 * BabelTrace - CTF Event Stream
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
struct bt_ctf_stream_class;
struct bt_ctf_stream;
struct bt_ctf_clock;

extern struct bt_ctf_stream_class *bt_ctf_stream_class_create(void);

extern int bt_ctf_stream_class_set_clock(struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_clock *clock);

/* Add an event class to the stream */
extern int bt_ctf_stream_class_add_event_class(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_event_class *event_class);

extern void bt_ctf_stream_class_get(struct bt_ctf_stream_class *stream_class);

extern void bt_ctf_stream_class_put(struct bt_ctf_stream_class *stream_class);


extern void bt_ctf_stream_create(struct bt_ctf_stream_class *stream_class);

/*
 * Insert event_count discarded/dropped events in stream before the next
 * event
 */
extern void bt_ctf_stream_push_discarded_events(struct bt_ctf_stream *stream,
		uint64_t event_count);

/* The event's timestamp will be its associated clock's value */
extern int bt_ctf_stream_push_event(struct bt_ctf_stream *stream,
		struct bt_ctf_event *event);

/* 
 * Close the current packet. The next event pushed will be placed in a new
 * packet.
 */
extern int bt_ctf_stream_flush(struct bt_ctf_stream *stream);

extern void bt_ctf_stream_get(struct bt_ctf_stream *stream);

extern void bt_ctf_stream_put(struct bt_ctf_stream *stream);

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_CTF_STREAM_H */
