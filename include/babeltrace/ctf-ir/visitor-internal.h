#ifndef BABELTRACE_CTF_IR_VISITOR_INTERNAL_H
#define BABELTRACE_CTF_IR_VISITOR_INTERNAL_H

/*
 * BabelTrace - CTF IR: Visitor internal
 *
 * Copyright 2015 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

struct bt_ctf_trace;
struct bt_ctf_field_type;
struct bt_ctf_visitor;

/*
 * Return 0 on success, negative value on error, positive value to stop traversal
 * while indicating success.
 */
typedef int (*bt_ctf_visitor_func)(struct bt_ctf_visitor *visitor,
		struct bt_ctf_field_type *type, const char *name);

struct bt_ctf_visitor {
	bt_ctf_visitor_func visit;

	struct bt_ctf_visitor_context {
		struct bt_ctf_trace *trace; /* weak pointer*/
		struct bt_ctf_stream_class *stream_class; /* weak pointer */
		struct bt_ctf_event_class *event_class; /* weak pointer */
		GString *absolute_path; /* e.g. stream.packet.header. */
	} context;
};

#endif /* BABELTRACE_CTF_IR_VISITOR_INTERNAL_H */
