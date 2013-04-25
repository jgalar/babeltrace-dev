#ifndef _BABELTRACE_TRACE_DESCRIPTOR_H
#define _BABELTRACE_TRACE_DESCRIPTOR_H

/*
 * BabelTrace
 *
 * Trace Descriptor Header
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

#include <babeltrace/iterator.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_trace_descriptor;

/*
 * bt_trace_descriptor_get_stream_pos_iter: Get an iterator on the
 * trace_descriptor's stream_pos.
 *
 * Return: An iterator on a collection of pointers to struct bt_stream_pos
 */
extern struct bt_ptr_iter *bt_trace_descriptor_get_stream_pos_iter(
	const struct bt_trace_descriptor *descriptor);

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_TRACE_DESCRIPTOR_H */
