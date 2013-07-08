/*
 * event-types.c
 *
 * Babeltrace CTF Writer
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
#include <babeltrace/ctf-writer/event-types-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/compiler.h>
#include <float.h>

struct range_overlap_query {
	int64_t range_start, range_end;
	int overlaps;
};

struct structure_name_query {
	GQuark name;
	int found;
};

static void bt_ctf_field_type_integer_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_type_enumeration_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_type_floating_point_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_type_structure_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_type_variant_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_type_array_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_type_sequence_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_type_string_destroy(struct bt_ctf_ref *);

static void (*type_destroy_funcs[])(struct bt_ctf_ref *) =
{
	[BT_CTF_FIELD_TYPE_ID_INTEGER] = bt_ctf_field_type_integer_destroy,
	[BT_CTF_FIELD_TYPE_ID_ENUMERATION] =
		bt_ctf_field_type_enumeration_destroy,
	[BT_CTF_FIELD_TYPE_ID_FLOATING_POINT] =
		bt_ctf_field_type_floating_point_destroy,
	[BT_CTF_FIELD_TYPE_ID_STRUCTURE] = bt_ctf_field_type_structure_destroy,
	[BT_CTF_FIELD_TYPE_ID_VARIANT] = bt_ctf_field_type_variant_destroy,
	[BT_CTF_FIELD_TYPE_ID_ARRAY] = bt_ctf_field_type_array_destroy,
	[BT_CTF_FIELD_TYPE_ID_SEQUENCE] = bt_ctf_field_type_sequence_destroy,
	[BT_CTF_FIELD_TYPE_ID_STRING] = bt_ctf_field_type_string_destroy
};


static void destroy_enumeration_mapping(gpointer elem)
{
	g_free(elem);
}

static void destroy_structure_field(gpointer elem)
{
	struct structure_field *field = elem;
	if (field->type) {
		bt_ctf_field_type_put(field->type);
	}
	g_free(field);
}

static void check_ranges_overlap(gpointer element, gpointer query)
{
	struct enumeration_mapping *mapping = element;
	struct range_overlap_query *range_query = query;
	range_query->overlaps |= mapping->range_start <= range_query->range_end
		&& range_query->range_start <= mapping->range_end;
}

static void find_structure_field_name(gpointer element, gpointer query)
{
	struct structure_field *field = element;
	struct structure_name_query *name_query = query;
	name_query->found |= name_query->name == field->name;
}

static void bt_ctf_field_type_init(struct bt_ctf_field_type *type)
{
	assert(type && type->field_type > BT_CTF_FIELD_TYPE_ID_UNKNOWN &&
		type->field_type < NR_BT_CTF_FIELD_TYPE_ID_TYPES);
	bt_ctf_ref_init(&type->ref_count, type_destroy_funcs[type->field_type]);
	type->endianness = BT_CTF_BYTE_ORDER_NATIVE;
	type->alignment = 1;
}

static int add_structure_field(GPtrArray *fields,
		struct bt_ctf_field_type *field_type,
		const char *field_name)
{
	int ret = -1;
	/* Make sure structure does not contain a field of the same name */
	GQuark name_quark = g_quark_from_string(field_name);
	struct structure_name_query query = { .name = name_quark, .found = 0 };
	g_ptr_array_foreach(fields, find_structure_field_name,
		&query);
	if (query.found) {
		goto end;
	}

	struct structure_field *field = g_new0(struct structure_field, 1);
	if (!field) {
		goto end;
	}

	bt_ctf_field_type_get(field_type);
	field->name = name_quark;
	field->type = field_type;
	g_ptr_array_add(fields, field);
	ret = 0;
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_integer_create(unsigned int size)
{
	struct bt_ctf_field_type_integer *integer =
		g_new0(struct bt_ctf_field_type_integer, 1);
	if (!integer) {
		return NULL;
	}

	integer->parent.field_type = BT_CTF_FIELD_TYPE_ID_INTEGER;
	integer->is_signed = 0;
	integer->size = size;
	integer->base = BT_CTF_INTEGER_BASE_DECIMAL;
	integer->encoding = BT_CTF_STRING_ENCODING_NONE;
	bt_ctf_field_type_init(&integer->parent);
	return &integer->parent;
}

int bt_ctf_field_type_integer_set_signed(struct bt_ctf_field_type *type,
		int is_signed)
{
	int ret = 0;
	if (!type || type->locked ||
		type->field_type != BT_CTF_FIELD_TYPE_ID_INTEGER) {
		ret = -1;
		goto end;
	}

	struct bt_ctf_field_type_integer *integer = container_of(type,
		struct bt_ctf_field_type_integer, parent);
	integer->is_signed = is_signed ? 1 : 0;
end:
	return ret;
}

int bt_ctf_field_type_integer_set_base(struct bt_ctf_field_type *type,
		enum bt_ctf_integer_base base)
{
	int ret = 0;
	if (!type || type->locked ||
		type->field_type != BT_CTF_FIELD_TYPE_ID_INTEGER ||
		base <= BT_CTF_INTEGER_BASE_UNKNOWN ||
		base >= NR_BT_CTF_INTEGER_BASE_TYPES) {
		ret = -1;
		goto end;
	}

	struct bt_ctf_field_type_integer *integer = container_of(type,
		struct bt_ctf_field_type_integer, parent);
	integer->base = base;
end:
	return ret;
}

int bt_ctf_field_type_integer_set_encoding(struct bt_ctf_field_type *type,
		enum bt_ctf_string_encoding encoding)
{
	int ret = 0;
	if (!type || type->locked ||
		type->field_type != BT_CTF_FIELD_TYPE_ID_INTEGER ||
		encoding < BT_CTF_STRING_ENCODING_NONE ||
		encoding >= NR_BT_CTF_STRING_ENCODING_TYPES) {
		ret = -1;
		goto end;
	}

	struct bt_ctf_field_type_integer *integer = container_of(type,
		struct bt_ctf_field_type_integer, parent);
	integer->encoding = encoding;
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_enumeration_create(
		struct bt_ctf_field_type *integer_container_type)
{
	struct bt_ctf_field_type_enumeration *enumeration = NULL;
	if (!integer_container_type ) {
		goto error;
	}

	enumeration = g_new0(struct bt_ctf_field_type_enumeration, 1);
	if (!enumeration) {
		goto error;
	}

	enumeration->parent.field_type = BT_CTF_FIELD_TYPE_ID_ENUMERATION;
	bt_ctf_field_type_get(integer_container_type);
	enumeration->container = integer_container_type;
	enumeration->entries = g_ptr_array_new_with_free_func(
		destroy_enumeration_mapping);
	bt_ctf_field_type_init(&enumeration->parent);
	return &enumeration->parent;
error:
	g_free(enumeration);
	return NULL;
}

int bt_ctf_field_type_enumeration_add_mapping(
		struct bt_ctf_field_type *type, const char *string,
		int64_t range_start, int64_t range_end)
{
	int ret = -1;
	if (!type || !string ||
		type->field_type != BT_CTF_FIELD_TYPE_ID_ENUMERATION ||
		type->locked ||
		range_end < range_start) {
		goto end;
	}

	struct bt_ctf_field_type_enumeration *enumeration = container_of(type,
		struct bt_ctf_field_type_enumeration, parent);

	/* Check that the range does not overlap with one already present */
	struct range_overlap_query query = {.range_start = range_start,
		.range_end = range_end, .overlaps = 0};
	g_ptr_array_foreach(enumeration->entries, check_ranges_overlap, &query);
	if (query.overlaps) {
		goto end;
	}
	if (validate_identifier(string)) {
		goto end;
	}

	struct enumeration_mapping *mapping =
		g_new(struct enumeration_mapping, 1);
	if (!mapping) {
		goto end;
	}

	*mapping = (struct enumeration_mapping){.range_start = range_start,
		.range_end = range_end,	.string = g_quark_from_string(string)};
	g_ptr_array_add(enumeration->entries, mapping);
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_floating_point_create(void)
{
	struct bt_ctf_field_type_floating_point *floating_point =
		g_new0(struct bt_ctf_field_type_floating_point, 1);
	if (!floating_point) {
		goto error;
	}
	floating_point->parent.field_type = BT_CTF_FIELD_TYPE_ID_FLOATING_POINT;
	floating_point->exponent_digit = 8;
	floating_point->mantissa_digit = 24;
	bt_ctf_field_type_init(&floating_point->parent);
	return &floating_point->parent;
error:
	g_free(floating_point);
	return NULL;
}

int bt_ctf_field_type_floating_point_set_exponent_digit(
		struct bt_ctf_field_type *type,
		unsigned int exponent_digit)
{
	int ret = -1;
	if (!type || type->locked ||
		type->field_type != BT_CTF_FIELD_TYPE_ID_FLOATING_POINT) {
		goto end;
	}
	struct bt_ctf_field_type_floating_point *floating_point = container_of(
		type, struct bt_ctf_field_type_floating_point, parent);
	if (exponent_digit != sizeof(float) *CHAR_BIT - FLT_MANT_DIG &&
		exponent_digit != sizeof(double) * CHAR_BIT - DBL_MANT_DIG &&
		sizeof(long double) * CHAR_BIT - LDBL_MANT_DIG) {
		goto end;
	}
	ret = 0;
	floating_point->exponent_digit = exponent_digit;
end:
	return ret;
}

int bt_ctf_field_type_floating_point_set_mantissa_digit(
		struct bt_ctf_field_type *type,
		unsigned int mantissa_digit)
{
	int ret = -1;
	if (!type || type->locked ||
		type->field_type != BT_CTF_FIELD_TYPE_ID_FLOATING_POINT) {
		goto end;
	}
	struct bt_ctf_field_type_floating_point *floating_point = container_of(
		type, struct bt_ctf_field_type_floating_point, parent);

	if (mantissa_digit != FLT_MANT_DIG && mantissa_digit != DBL_MANT_DIG &&
		mantissa_digit != LDBL_MANT_DIG) {
		goto end;
	}
	ret = 0;
	floating_point->mantissa_digit = mantissa_digit;
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_structure_create(void)
{
	struct bt_ctf_field_type_structure *structure =
		g_new0(struct bt_ctf_field_type_structure, 1);
	if (!structure) {
		goto error;
	}
	structure->parent.field_type = BT_CTF_FIELD_TYPE_ID_STRUCTURE;
	bt_ctf_field_type_init(&structure->parent);
	structure->fields = g_ptr_array_new_with_free_func(
		destroy_structure_field);
	return &structure->parent;
error:
	return NULL;
}

int bt_ctf_field_type_structure_add_field(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *field_type,
		const char *field_name)
{
	int ret = -1;
	if (!type || !field_type || !field_name || type->locked ||
		validate_identifier(field_name) ||
		type->field_type != BT_CTF_FIELD_TYPE_ID_STRUCTURE) {
		goto end;
	}

	struct bt_ctf_field_type_structure *structure = container_of(type,
		struct bt_ctf_field_type_structure, parent);
	if (add_structure_field(structure->fields, field_type, field_name)) {
		goto end;
	}
	ret = 0;
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_variant_create(const char *tag_name)
{
	struct bt_ctf_field_type_variant *variant = NULL;
	if (!tag_name || validate_identifier(tag_name)) {
		goto error;
	}

	variant = g_new0(struct bt_ctf_field_type_variant, 1);
	if (!variant) {
		goto error;
	}

	variant->parent.field_type = BT_CTF_FIELD_TYPE_ID_VARIANT;
	variant->tag_name = g_quark_from_string(tag_name);
	bt_ctf_field_type_init(&variant->parent);
	variant->fields = g_ptr_array_new_with_free_func(
		destroy_structure_field);
	return &variant->parent;
error:
	return NULL;
}

int bt_ctf_field_type_variant_add_field(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *field_type,
		const char *field_name)
{
	int ret = -1;
	if (!type || !field_type || !field_name || type->locked ||
		validate_identifier(field_name) ||
		type->field_type != BT_CTF_FIELD_TYPE_ID_VARIANT) {
		goto end;
	}

	struct bt_ctf_field_type_variant *variant = container_of(type,
		struct bt_ctf_field_type_variant, parent);
	if (add_structure_field(variant->fields, field_type, field_name)) {
		goto end;
	}
	ret = 0;
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_array_create(
		struct bt_ctf_field_type *element_type,
		unsigned int length)
{
	struct bt_ctf_field_type_array *array = NULL;
	if (!element_type || length == 0) {
		goto error;
	}

	array = g_new0(struct bt_ctf_field_type_array, 1);
	if (!array) {
		goto error;
	}

	array->parent.field_type = BT_CTF_FIELD_TYPE_ID_ARRAY;
	bt_ctf_field_type_init(&array->parent);
	bt_ctf_field_type_get(element_type);
	array->element_type = element_type;
	array->length = length;
	return &array->parent;
error:
	return NULL;
}

struct bt_ctf_field_type *bt_ctf_field_type_sequence_create(
		struct bt_ctf_field_type *element_type,
		const char *length_field_name)
{
	struct bt_ctf_field_type_sequence *sequence = NULL;
	if (!element_type || !length_field_name ||
		validate_identifier(length_field_name)) {
		goto error;
	}

	sequence = g_new0(struct bt_ctf_field_type_sequence, 1);
	if (!sequence) {
		goto error;
	}

	sequence->parent.field_type = BT_CTF_FIELD_TYPE_ID_SEQUENCE;
	bt_ctf_field_type_init(&sequence->parent);
	bt_ctf_field_type_get(element_type);
	sequence->element_type = element_type;
	sequence->length_field_name = g_quark_from_string(length_field_name);
	return &sequence->parent;
error:
	return NULL;
}

struct bt_ctf_field_type *bt_ctf_field_type_string_create(void)
{
	struct bt_ctf_field_type_string *string =
		g_new0(struct bt_ctf_field_type_string, 1);
	if (!string) {
		return NULL;
	}

	string->parent.field_type = BT_CTF_FIELD_TYPE_ID_STRING;
	bt_ctf_field_type_init(&string->parent);
	string->encoding = BT_CTF_STRING_ENCODING_UTF8;
	string->parent.alignment = 8;
	return &string->parent;
}

int bt_ctf_field_type_string_set_encoding(
		struct bt_ctf_field_type *type,
		enum bt_ctf_string_encoding encoding)
{
	int ret = 0;
	if (!type || type->field_type != BT_CTF_FIELD_TYPE_ID_STRING ||
		(encoding != BT_CTF_STRING_ENCODING_UTF8 &&
		encoding != BT_CTF_STRING_ENCODING_ASCII)) {
		ret = -1;
		goto end;
	}
	struct bt_ctf_field_type_string *string = container_of(type,
		struct bt_ctf_field_type_string, parent);
	string->encoding = encoding;
end:
	return ret;
}

int bt_ctf_field_type_set_alignment(struct bt_ctf_field_type *type,
		unsigned int alignment)
{
	int ret = -1;
	/* Alignment must be bit-aligned (1) or n-byte aligned */
	if (!type || type->locked || (alignment != 1 && alignment & 0x7)) {
		goto end;
	}
	if (type->field_type == BT_CTF_FIELD_TYPE_ID_STRING && alignment != 8) {
		goto end;
	}
	type->alignment = alignment;
end:
	return ret;
}

int bt_ctf_field_type_set_byte_order(struct bt_ctf_field_type *type,
		enum bt_ctf_byte_order byte_order)
{
	int ret = -1;
	if (!type || type->locked) {
		goto end;
	}
	type->endianness = byte_order;
end:
	return ret;
}

void bt_ctf_field_type_get(struct bt_ctf_field_type *type)
{
	if (!type) {
		return;
	}
	bt_ctf_ref_get(&type->ref_count);
}

void bt_ctf_field_type_put(struct bt_ctf_field_type *type)
{
	if (!type) {
		return;
	}
	bt_ctf_ref_put(&type->ref_count);
}

void bt_ctf_field_type_lock(struct bt_ctf_field_type *type)
{
	if (!type) {
		return;
	}
	type->locked = 1;
}

enum bt_ctf_field_type_id bt_ctf_field_type_get_type_id(
		struct bt_ctf_field_type *type)
{
	if (!type) {
		return BT_CTF_FIELD_TYPE_ID_UNKNOWN;
	}
	return type->field_type;
}

void bt_ctf_field_type_integer_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}
	struct bt_ctf_field_type_integer *integer = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_integer, parent);
	g_free(integer);
}

void bt_ctf_field_type_enumeration_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}
	struct bt_ctf_field_type_enumeration *enumeration = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_enumeration, parent);
	g_ptr_array_free(enumeration->entries, TRUE);
	bt_ctf_field_type_put(enumeration->container);
	g_free(enumeration);
}

void bt_ctf_field_type_floating_point_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}
	struct bt_ctf_field_type_floating_point *floating_point = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_floating_point, parent);
	g_free(floating_point);
}

void bt_ctf_field_type_structure_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}
	struct bt_ctf_field_type_structure *structure = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_structure, parent);
	g_ptr_array_free(structure->fields, TRUE);
	g_free(structure);
}

void bt_ctf_field_type_variant_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}
	struct bt_ctf_field_type_variant *variant = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_variant, parent);
	g_ptr_array_free(variant->fields, TRUE);
	g_free(variant);
}

void bt_ctf_field_type_array_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}
	struct bt_ctf_field_type_array *array = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_array, parent);
	bt_ctf_field_type_put(array->element_type);
	g_free(array);
}

void bt_ctf_field_type_sequence_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}
	struct bt_ctf_field_type_sequence *sequence = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_sequence, parent);
	bt_ctf_field_type_put(sequence->element_type);
	g_free(sequence);
}

void bt_ctf_field_type_string_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}
	struct bt_ctf_field_type_string *string = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_string, parent);
	g_free(string);
}
