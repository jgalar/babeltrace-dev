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
	GQuark mapping_name;
};

static void bt_ctf_field_type_integer_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_type_enumeration_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_type_floating_point_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_type_structure_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_type_variant_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_type_array_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_type_sequence_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_type_string_destroy(struct bt_ctf_ref *);

static void (* const type_destroy_funcs[])(struct bt_ctf_ref *) =
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

static void generic_field_type_lock(struct bt_ctf_field_type *);
static void bt_ctf_field_type_enumeration_lock(struct bt_ctf_field_type *);
static void bt_ctf_field_type_structure_lock(struct bt_ctf_field_type *);
static void bt_ctf_field_type_variant_lock(struct bt_ctf_field_type *);
static void bt_ctf_field_type_array_lock(struct bt_ctf_field_type *);
static void bt_ctf_field_type_sequence_lock(struct bt_ctf_field_type *);

static type_lock_func const type_lock_funcs[] =
{
	[BT_CTF_FIELD_TYPE_ID_INTEGER] = generic_field_type_lock,
	[BT_CTF_FIELD_TYPE_ID_ENUMERATION] = bt_ctf_field_type_enumeration_lock,
	[BT_CTF_FIELD_TYPE_ID_FLOATING_POINT] = generic_field_type_lock,
	[BT_CTF_FIELD_TYPE_ID_STRUCTURE] = bt_ctf_field_type_structure_lock,
	[BT_CTF_FIELD_TYPE_ID_VARIANT] = bt_ctf_field_type_variant_lock,
	[BT_CTF_FIELD_TYPE_ID_ARRAY] = bt_ctf_field_type_array_lock,
	[BT_CTF_FIELD_TYPE_ID_SEQUENCE] = bt_ctf_field_type_sequence_lock,
	[BT_CTF_FIELD_TYPE_ID_STRING] = generic_field_type_lock
};

static int bt_ctf_field_type_integer_serialize(struct bt_ctf_field_type *,
		struct metadata_context *);
static int bt_ctf_field_type_enumeration_serialize(struct bt_ctf_field_type *,
		struct metadata_context *);
static int bt_ctf_field_type_floating_point_serialize(
		struct bt_ctf_field_type *, struct metadata_context *);
static int bt_ctf_field_type_structure_serialize(struct bt_ctf_field_type *,
		struct metadata_context *);
static int bt_ctf_field_type_variant_serialize(struct bt_ctf_field_type *,
		struct metadata_context *);
static int bt_ctf_field_type_array_serialize(struct bt_ctf_field_type *,
		struct metadata_context *);
static int bt_ctf_field_type_sequence_serialize(struct bt_ctf_field_type *,
		struct metadata_context *);
static int bt_ctf_field_type_string_serialize(struct bt_ctf_field_type *,
		struct metadata_context *);

static type_serialize_func const type_serialize_funcs[] =
{
	[BT_CTF_FIELD_TYPE_ID_INTEGER] = bt_ctf_field_type_integer_serialize,
	[BT_CTF_FIELD_TYPE_ID_ENUMERATION] =
		bt_ctf_field_type_enumeration_serialize,
	[BT_CTF_FIELD_TYPE_ID_FLOATING_POINT] =
		bt_ctf_field_type_floating_point_serialize,
	[BT_CTF_FIELD_TYPE_ID_STRUCTURE] = bt_ctf_field_type_structure_serialize,
	[BT_CTF_FIELD_TYPE_ID_VARIANT] = bt_ctf_field_type_variant_serialize,
	[BT_CTF_FIELD_TYPE_ID_ARRAY] = bt_ctf_field_type_array_serialize,
	[BT_CTF_FIELD_TYPE_ID_SEQUENCE] = bt_ctf_field_type_sequence_serialize,
	[BT_CTF_FIELD_TYPE_ID_STRING] = bt_ctf_field_type_string_serialize
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
	struct range_overlap_query *overlap_query = query;
	if (mapping->range_start <= overlap_query->range_end
	    && overlap_query->range_start <= mapping->range_end) {
		overlap_query->overlaps = 1;
		overlap_query->mapping_name = mapping->string;
	}

	overlap_query->overlaps |=
		mapping->string == overlap_query->mapping_name;
}

static void bt_ctf_field_type_init(struct bt_ctf_field_type *type)
{
	assert(type && type->field_type > BT_CTF_FIELD_TYPE_ID_UNKNOWN &&
		type->field_type < NR_BT_CTF_FIELD_TYPE_ID_TYPES);
	bt_ctf_ref_init(&type->ref_count, type_destroy_funcs[type->field_type]);
	type->lock = type_lock_funcs[type->field_type];
	type->serialize = type_serialize_funcs[type->field_type];
	type->endianness = BT_CTF_BYTE_ORDER_NATIVE;
	type->alignment = 1;
}

static int add_structure_field(GPtrArray *fields,
		GHashTable *field_name_to_index,
		struct bt_ctf_field_type *field_type,
		const char *field_name)
{
	int ret = -1;
	/* Make sure structure does not contain a field of the same name */
	GQuark name_quark = g_quark_from_string(field_name);
	if (g_hash_table_contains(field_name_to_index,
			GUINT_TO_POINTER(name_quark))) {
		goto end;
	}

	struct structure_field *field = g_new0(struct structure_field, 1);
	if (!field) {
		goto end;
	}

	bt_ctf_field_type_get(field_type);
	field->name = name_quark;
	field->type = field_type;
	g_hash_table_insert(field_name_to_index,
		(gpointer) (unsigned long) name_quark,
		(gpointer) (unsigned long) fields->len);
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
	integer->_signed = 0;
	integer->size = size;
	integer->base = BT_CTF_INTEGER_BASE_DECIMAL;
	integer->encoding = BT_CTF_STRING_ENCODING_NONE;
	bt_ctf_field_type_init(&integer->parent);
	return &integer->parent;
}

int bt_ctf_field_type_integer_set_signed(struct bt_ctf_field_type *type,
		int is_signed)
{
	int ret = -1;
	if (!type || type->locked ||
		type->field_type != BT_CTF_FIELD_TYPE_ID_INTEGER) {
		goto end;
	}

	struct bt_ctf_field_type_integer *integer = container_of(type,
		struct bt_ctf_field_type_integer, parent);
	if (is_signed && integer->size <= 1) {
		goto end;
	}
	integer->_signed = is_signed ? 1 : 0;
	ret = 0;
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
	if (!type || type->field_type != BT_CTF_FIELD_TYPE_ID_ENUMERATION ||
		type->locked ||
		range_end < range_start) {
		goto end;
	}

	if (validate_identifier(string)) {
		goto end;
	}

	GQuark mapping_name = g_quark_from_string(string);
	struct bt_ctf_field_type_enumeration *enumeration = container_of(type,
		struct bt_ctf_field_type_enumeration, parent);

	/* Check that the range does not overlap with one already present */
	struct range_overlap_query query = {.range_start = range_start,
		.range_end = range_end, .mapping_name = mapping_name,
			.overlaps = 0};
	g_ptr_array_foreach(enumeration->entries, check_ranges_overlap, &query);
	if (query.overlaps) {
		goto end;
	}

	struct enumeration_mapping *mapping =
		g_new(struct enumeration_mapping, 1);
	if (!mapping) {
		goto end;
	}

	*mapping = (struct enumeration_mapping){.range_start = range_start,
		.range_end = range_end,	.string = mapping_name};
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
	if ((exponent_digit != sizeof(float) *CHAR_BIT - FLT_MANT_DIG) &&
		(exponent_digit != sizeof(double) * CHAR_BIT - DBL_MANT_DIG) &&
		(exponent_digit !=
			sizeof(long double) * CHAR_BIT - LDBL_MANT_DIG)) {
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
	structure->field_name_to_index = g_hash_table_new(NULL, NULL);
	return &structure->parent;
error:
	return NULL;
}

int bt_ctf_field_type_structure_add_field(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *field_type,
		const char *field_name)
{
	int ret = -1;
	if (!type || !field_type || type->locked ||
		validate_identifier(field_name) ||
		type->field_type != BT_CTF_FIELD_TYPE_ID_STRUCTURE) {
		goto end;
	}

	struct bt_ctf_field_type_structure *structure = container_of(type,
		struct bt_ctf_field_type_structure, parent);
	if (add_structure_field(structure->fields,
		structure->field_name_to_index, field_type, field_name)) {
		goto end;
	}
	ret = 0;
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_variant_create(const char *tag_name)
{
	struct bt_ctf_field_type_variant *variant = NULL;
	if (validate_identifier(tag_name)) {
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
	variant->field_name_to_index = g_hash_table_new(NULL, NULL);
	return &variant->parent;
error:
	return NULL;
}

int bt_ctf_field_type_variant_add_field(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *field_type,
		const char *field_name)
{
	int ret = -1;
	if (!type || !field_type || type->locked ||
		validate_identifier(field_name) ||
		type->field_type != BT_CTF_FIELD_TYPE_ID_VARIANT) {
		goto end;
	}

	struct bt_ctf_field_type_variant *variant = container_of(type,
		struct bt_ctf_field_type_variant, parent);
	if (add_structure_field(variant->fields, variant->field_name_to_index,
		field_type, field_name)) {
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
	if (!element_type || validate_identifier(length_field_name)) {
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
	ret = 0;
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
	type->lock(type);
}

enum bt_ctf_field_type_id bt_ctf_field_type_get_type_id(
		struct bt_ctf_field_type *type)
{
	if (!type) {
		return BT_CTF_FIELD_TYPE_ID_UNKNOWN;
	}
	return type->field_type;
}

struct bt_ctf_field_type *bt_ctf_field_type_structure_get_type(
		struct bt_ctf_field_type_structure *structure,
		const char *name)
{
	struct bt_ctf_field_type *type = NULL;
	GQuark name_quark = g_quark_try_string(name);
	if (!name_quark) {
		goto end;
	}

	gpointer result;
	if (!g_hash_table_lookup_extended(structure->field_name_to_index,
	      GUINT_TO_POINTER(name_quark), NULL, &result)) {
		goto end;
	}

	size_t index = GPOINTER_TO_UINT(result);
	if (index > structure->fields->len) {
		goto end;
	}

	struct structure_field *field = structure->fields->pdata[index];
	type = field->type;
end:
	return type;
}

struct bt_ctf_field_type *bt_ctf_field_type_array_get_element_type(
		struct bt_ctf_field_type_array *array)
{
	return array->element_type;
}

struct bt_ctf_field_type *bt_ctf_field_type_sequence_get_element_type(
		struct bt_ctf_field_type_sequence *sequence)
{
	return sequence->element_type;
}

struct bt_ctf_field_type *bt_ctf_field_type_variant_get_type(
		struct bt_ctf_field_type_variant *variant,
		struct bt_ctf_field_type_enumeration *enumeration,
		int64_t tag)
{
	struct bt_ctf_field_type *type = NULL;

	struct range_overlap_query query = {.range_start = tag,
		.range_end = tag, .mapping_name = 0, .overlaps = 0};
	g_ptr_array_foreach(enumeration->entries, check_ranges_overlap, &query);
	if (!query.overlaps) {
		goto end;
	}
	GQuark name_quark = query.mapping_name;

	/* May return 0 (a valid index); the quark must be checked */
	size_t index = (size_t) g_hash_table_lookup(
		variant->field_name_to_index,
		GUINT_TO_POINTER(name_quark));
	if (index > variant->fields->len) {
		goto end;
	}

	struct structure_field *field_entry = g_ptr_array_index(
		variant->fields, index);
	if (field_entry->name == name_quark) {
		type = field_entry->type;
	}
end:
	return type;
}

int bt_ctf_field_type_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	int ret;
	if (!type || !context) {
		ret = -1;
		goto end;
	}

	ret = type->serialize(type, context);
end:
	return ret;
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
	g_hash_table_destroy(structure->field_name_to_index);
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
	g_hash_table_destroy(variant->field_name_to_index);
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

void generic_field_type_lock(struct bt_ctf_field_type *type)
{
	type->locked = 1;
}

void bt_ctf_field_type_enumeration_lock(struct bt_ctf_field_type *type)
{
	generic_field_type_lock(type);
	struct bt_ctf_field_type_enumeration *enumeration_type = container_of(
		type, struct bt_ctf_field_type_enumeration, parent);
	bt_ctf_field_type_lock(enumeration_type->container);
}

static void lock_structure_field(struct structure_field *field)
{
	bt_ctf_field_type_lock(field->type);
}

void bt_ctf_field_type_structure_lock(struct bt_ctf_field_type *type)
{
	generic_field_type_lock(type);
	struct bt_ctf_field_type_structure *structure_type = container_of(
		type, struct bt_ctf_field_type_structure, parent);
	g_ptr_array_foreach(structure_type->fields, (GFunc)lock_structure_field,
		NULL);
}

void bt_ctf_field_type_variant_lock(struct bt_ctf_field_type *type)
{
	generic_field_type_lock(type);
	struct bt_ctf_field_type_variant *variant_type = container_of(
		type, struct bt_ctf_field_type_variant, parent);
	g_ptr_array_foreach(variant_type->fields, (GFunc)lock_structure_field,
		NULL);
}

void bt_ctf_field_type_array_lock(struct bt_ctf_field_type *type)
{
	generic_field_type_lock(type);
	struct bt_ctf_field_type_array *array_type = container_of(
		type, struct bt_ctf_field_type_array, parent);
	bt_ctf_field_type_lock(array_type->element_type);
}

void bt_ctf_field_type_sequence_lock(struct bt_ctf_field_type *type)
{
	generic_field_type_lock(type);
	struct bt_ctf_field_type_sequence *sequence_type = container_of(
		type, struct bt_ctf_field_type_sequence, parent);
	bt_ctf_field_type_lock(sequence_type->element_type);
}

int bt_ctf_field_type_integer_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	return -1;
}

int bt_ctf_field_type_enumeration_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	return -1;
}

int bt_ctf_field_type_floating_point_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	return -1;
}

int bt_ctf_field_type_structure_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	return -1;
}

int bt_ctf_field_type_variant_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	return -1;
}

int bt_ctf_field_type_array_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	return -1;
}

int bt_ctf_field_type_sequence_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	return -1;
}

int bt_ctf_field_type_string_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	return -1;
}
