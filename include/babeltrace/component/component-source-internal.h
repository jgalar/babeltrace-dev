#ifndef BABELTRACE_COMPONENT_SOURCE_INTERNAL_H
#define BABELTRACE_COMPONENT_SOURCE_INTERNAL_H

/*
 * BabelTrace - Source Component internal
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#include <babeltrace/component/component-internal.h>
#include <babeltrace/component/component-class-internal.h>

struct bt_value;

struct bt_component_source {
	struct bt_component parent;
};

/**
 * Allocate a source component.
 *
 * @param class			Component class
 * @param params		A dictionary of component parameters
 * @returns			A source component instance
 */
BT_HIDDEN
struct bt_component *bt_component_source_create(
		struct bt_component_class *class, struct bt_value *params);

/**
 * Validate a source component.
 *
 * @param component		Source component instance to validate
 * @returns			One of #bt_component_status
 */
BT_HIDDEN
enum bt_component_status bt_component_source_validate(
		struct bt_component *component);

#endif /* BABELTRACE_COMPONENT_SOURCE_INTERNAL_H */
