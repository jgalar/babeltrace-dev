#ifndef _BABELTRACE_CTF_WRITER_EVENT_TYPES_INTERNAL_H
#define _BABELTRACE_CTF_WRITER_EVENT_TYPES_INTERNAL_H

/*
 * BabelTrace - CTF Writer
 *
 * CTF Writer Event Types
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

#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/ref-internal.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-writer/writer.h>
#include <babeltrace/babeltrace-internal.h>
#include <glib.h>

enum bt_ctf_field_type_id {
	BT_CTF_FIELD_TYPE_ID_UNKNOWN = 0,
	BT_CTF_FIELD_TYPE_ID_INTEGER,
	BT_CTF_FIELD_TYPE_ID_ENUMERATION,
	BT_CTF_FIELD_TYPE_ID_FLOATING_POINT,
	BT_CTF_FIELD_TYPE_ID_STRUCTURE,
	BT_CTF_FIELD_TYPE_ID_VARIANT,
	BT_CTF_FIELD_TYPE_ID_ARRAY,
	BT_CTF_FIELD_TYPE_ID_SEQUENCE,
	BT_CTF_FIELD_TYPE_ID_STRING,
	NR_BT_CTF_FIELD_TYPE_ID_TYPES
};

struct bt_ctf_field_type {
	struct bt_ctf_ref ref_count;
	enum bt_ctf_field_type_id field_type;
	enum bt_ctf_byte_order endianness;
	unsigned int alignment;
	/*
	 * A type can't be modified once it is added to an event or after a
	 * a field has been instanciated from it.
	 */
	int locked;
};

struct bt_ctf_field_type_integer {
	struct bt_ctf_field_type parent;
	int is_signed;
	unsigned int size;
	enum bt_ctf_integer_base base;
	enum bt_ctf_string_encoding encoding;
};

struct enumeration_mapping {
	int64_t range_start, range_end;
	GQuark string;
};

struct bt_ctf_field_type_enumeration {
	struct bt_ctf_field_type parent;
	struct bt_ctf_field_type *container;
	GPtrArray *entries; /* Array of pointers to struct enum_mapping */
};

struct bt_ctf_field_type_floating_point {
	struct bt_ctf_field_type parent;
	unsigned int exponent_digit;
	unsigned int mantissa_digit;
};

struct structure_field {
	GQuark name;
	struct bt_ctf_field_type *type;
};

struct bt_ctf_field_type_structure {
	struct bt_ctf_field_type parent;
	GPtrArray *fields; /* Array of pointers to struct structure_field */
};

struct bt_ctf_field_type_variant {
	struct bt_ctf_field_type parent;
	GQuark tag_name;
	GPtrArray *fields; /* Array of pointers to struct structure_field */
};

struct bt_ctf_field_type_array {
	struct bt_ctf_field_type parent;
	struct bt_ctf_field_type *element_type;
	unsigned int length; /* Number of elements */
};

struct bt_ctf_field_type_sequence {
	struct bt_ctf_field_type parent;
	struct bt_ctf_field_type *element_type;
	GQuark length_field_name;
};

struct bt_ctf_field_type_string {
	struct bt_ctf_field_type parent;
	enum bt_ctf_string_encoding encoding;
};

BT_HIDDEN
void bt_ctf_field_type_lock(struct bt_ctf_field_type *type);

BT_HIDDEN
enum bt_ctf_field_type_id bt_ctf_field_type_get_type_id(
		struct bt_ctf_field_type *type);

#endif /* _BABELTRACE_CTF_WRITER_EVENT_TYPES_INTERNAL_H */
