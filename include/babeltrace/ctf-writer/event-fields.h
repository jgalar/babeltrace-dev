#ifndef _BABELTRACE_CTF_EVENT_FIELD_H
#define _BABELTRACE_CTF_EVENT_FIELD_H

/*
 * BabelTrace - CTF Event Field
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

#include <stdint.h>

struct bt_ctf_event_class;
struct bt_ctf_event;
struct bt_ctf_field;
struct bt_ctf_field_type;

extern struct bt_ctf_field *bt_ctf_field_create(
	struct bt_ctf_field_type *type);

extern void bt_ctf_field_get(struct bt_ctf_field *field);

extern void bt_ctf_field_put(struct bt_ctf_field *field);

/* Creates and returns a field linked to the structure */
extern struct bt_ctf_field *bt_ctf_field_structure_get_field(
		struct bt_ctf_field *structure, const char *name);

/* Creates and returns a field linked to the array */
extern int bt_ctf_field_array_get_field(struct bt_ctf_field *array,
		unsigned int index);


extern int bt_ctf_field_variant_set_value(struct bt_ctf_field *variant,
		struct bt_ctf_field *value);

extern int bt_ctf_field_signed_integer_set_value(struct bt_ctf_field *integer,
		int64_t value);

extern int bt_ctf_field_unsigned_integer_set_value(struct bt_ctf_field *integer,
		uint64_t value);

extern int bt_ctf_field_floating_point_set_value(
		struct bt_ctf_field *floating_point,
		long double value);

extern int bt_ctf_field_string_set_value(struct bt_ctf_field *string,
		const char *value);

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_CTF_EVENT_FIELD_H */
