#ifndef BABELTRACE_COMPONENT_INPUT_INTERNAL_H
#define BABELTRACE_COMPONENT_INPUT_INTERNAL_H

/*
 * BabelTrace - Component Input
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

#include <babeltrace/babeltrace-internal.h>
#include <glib.h>
#include <stdbool.h>

struct component_input {
	/* Array of struct bt_notification_iterator pointers. */
	GPtrArray *iterators;
	unsigned int min_count;
	unsigned int max_count;
	bool validated;
};

BT_HIDDEN
int component_input_init(struct component_input *input);

BT_HIDDEN
int component_input_validate(struct component_input *input);

BT_HIDDEN
void component_input_fini(struct component_input *input);

#endif /* BABELTRACE_COMPONENT_INPUT_INTERNAL_H */
