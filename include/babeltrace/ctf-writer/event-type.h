#ifndef _BABELTRACE_CTF_EVENT_TYPES_H
#define _BABELTRACE_CTF_EVENT_TYPES_H

/*
 * BabelTrace - CTF Event Types
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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_event_class;
struct bt_ctf_event;
struct bt_ctf_field_type;

enum bt_ctf_integer_base {
	BT_CTF_INTEGER_BASE_DECIMAL = 0,
	BT_CTF_INTEGER_BASE_HEXADECIMAL,
	BT_CTF_INTEGER_BASE_OCTAL,
	BT_CTF_INTEGER_BASE_BINARY
};

/* Creates an alias to this type if alias_name is not null */
extern struct bt_ctf_field_type *bt_ctf_field_type_integer_create(int size);

extern int bt_ctf_field_type_integer_set_signed(
		struct bt_ctf_field_type *integer, int is_signed);

/*
 * Base used for pretty-printing output, defaults to decimal
 * BT_CTF_INTEGER_BASE_DECIMAL
 */
extern int bt_ctf_field_type_integer_set_base(
		struct bt_ctf_field_type *integer,
		enum bt_ctf_integer_base base);

/*
 * An integer encoding may be set to signal that the integer must be printed as
 * a textcharacter, default BT_CTF_STRING_ENCODING_NONE
 */
extern int bt_ctf_field_type_integer_set_encoding(
		struct bt_ctf_field_type *integer,
		enum bt_ctf_string_encoding encoding);

extern struct bt_ctf_field_type *bt_ctf_field_type_enumeration_create(
		struct bt_ctf_field_type *integer_type);

extern int bt_ctf_field_type_enumeration_add_mapping(
		struct bt_ctf_field_type *enumeration, const char *string,
		int64 range_start, int64 range_end);

extern struct bt_ctf_field_type *bt_ctf_field_type_floating_point_create(void);

extern int bt_ctf_field_type_floating_point_set_exponent_digit(
		struct bt_ctf_field_type *floating_point,
		int exponent_digit);

extern int bt_ctf_field_type_floating_point_set_mantissa_digit(
		struct bt_ctf_field_type *floating_point,
		int mantissa_digit);

extern struct bt_ctf_field_type *bt_ctf_field_type_structure_create(void);

extern int bt_ctf_field_type_structure_add_field(
		struct bt_ctf_field_type *structure,
		struct bt_ctf_field_type *field_type,
		const char *field_name);

extern struct bt_ctf_field_type *bt_ctf_field_type_variant_create(void);

/* Set the field indicating the variant's tag (type selector) */
extern int bt_ctf_field_type_variant_set_tag(struct bt_ctf_field_type *variant,
		struct bt_ctf_field_type *tag_enumeration);

extern int bt_ctf_field_type_variant_add_field(
		struct bt_ctf_field_type *variant,
		struct bt_ctf_field_type *field_type,
		const char *field_name);

extern struct bt_ctf_field_type *bt_ctf_field_type_array_create(
		struct bt_ctf_field_type *element_type,
		int length);

extern struct bt_ctf_field_type *bt_ctf_field_type_sequence_create(
		struct bt_ctf_field_type *element_type,
		struct bt_ctf_field_type *length_type);

extern struct bt_ctf_field_type *bt_ctf_field_type_string_create(void);

extern int bt_ctf_field_type_string_set_encoding(
		struct bt_ctf_field_type *string,
		enum bt_ctf_string_encoding encoding);

extern int bt_ctf_field_type_set_alignment(struct bt_ctf_field_type *type,
		int alignment);

extern int bt_ctf_field_type_set_byte_order(struct bt_ctf_field_type *type,
		enum bt_ctf_byte_order byte_order);

extern void bt_ctf_field_type_release(struct bt_ctf_field_type *type);

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_CTF_EVENT_TYPES_H */
