/*
 * event-fields.c
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

#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-writer/event-fields-internal.h>
#include <babeltrace/ctf-writer/event-types-internal.h>
#include <babeltrace/compiler.h>

static struct bt_ctf_field *bt_ctf_field_integer_create(
		struct bt_ctf_field_type *);
static struct bt_ctf_field *bt_ctf_field_enumeration_create(
		struct bt_ctf_field_type *);
static struct bt_ctf_field *bt_ctf_field_floating_point_create(
		struct bt_ctf_field_type *);
static struct bt_ctf_field *bt_ctf_field_structure_create(
		struct bt_ctf_field_type *);
static struct bt_ctf_field *bt_ctf_field_variant_create(
		struct bt_ctf_field_type *);
static struct bt_ctf_field *bt_ctf_field_array_create(
		struct bt_ctf_field_type *);
static struct bt_ctf_field *bt_ctf_field_sequence_create(
		struct bt_ctf_field_type *);
static struct bt_ctf_field *bt_ctf_field_string_create(
		struct bt_ctf_field_type *);

static void bt_ctf_field_destroy(struct bt_ctf_ref *);
static void bt_ctf_field_integer_destroy(struct bt_ctf_field *);
static void bt_ctf_field_enumeration_destroy(struct bt_ctf_field *);
static void bt_ctf_field_floating_point_destroy(struct bt_ctf_field *);
static void bt_ctf_field_structure_destroy(struct bt_ctf_field *);
static void bt_ctf_field_variant_destroy(struct bt_ctf_field *);
static void bt_ctf_field_array_destroy(struct bt_ctf_field *);
static void bt_ctf_field_sequence_destroy(struct bt_ctf_field *);
static void bt_ctf_field_string_destroy(struct bt_ctf_field *);

static struct bt_ctf_field *(*field_create_funcs[])(
	struct bt_ctf_field_type *) =
{
	[BT_CTF_FIELD_TYPE_ID_INTEGER] = bt_ctf_field_integer_create,
	[BT_CTF_FIELD_TYPE_ID_ENUMERATION] = bt_ctf_field_enumeration_create,
	[BT_CTF_FIELD_TYPE_ID_FLOATING_POINT] =
		bt_ctf_field_floating_point_create,
	[BT_CTF_FIELD_TYPE_ID_STRUCTURE] = bt_ctf_field_structure_create,
	[BT_CTF_FIELD_TYPE_ID_VARIANT] = bt_ctf_field_variant_create,
	[BT_CTF_FIELD_TYPE_ID_ARRAY] = bt_ctf_field_array_create,
	[BT_CTF_FIELD_TYPE_ID_SEQUENCE] = bt_ctf_field_sequence_create,
	[BT_CTF_FIELD_TYPE_ID_STRING] = bt_ctf_field_string_create
};

static void (*field_destroy_funcs[])(struct bt_ctf_field *) =
{
	[BT_CTF_FIELD_TYPE_ID_INTEGER] = bt_ctf_field_integer_destroy,
	[BT_CTF_FIELD_TYPE_ID_ENUMERATION] = bt_ctf_field_enumeration_destroy,
	[BT_CTF_FIELD_TYPE_ID_FLOATING_POINT] =
		bt_ctf_field_floating_point_destroy,
	[BT_CTF_FIELD_TYPE_ID_STRUCTURE] = bt_ctf_field_structure_destroy,
	[BT_CTF_FIELD_TYPE_ID_VARIANT] = bt_ctf_field_variant_destroy,
	[BT_CTF_FIELD_TYPE_ID_ARRAY] = bt_ctf_field_array_destroy,
	[BT_CTF_FIELD_TYPE_ID_SEQUENCE] = bt_ctf_field_sequence_destroy,
	[BT_CTF_FIELD_TYPE_ID_STRING] = bt_ctf_field_string_destroy
};

struct bt_ctf_field *bt_ctf_field_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field *field = NULL;
	if (!type) {
		goto error;
	}

	enum bt_ctf_field_type_id type_id = bt_ctf_field_type_get_type_id(type);
	if (type_id <= BT_CTF_FIELD_TYPE_ID_UNKNOWN ||
		type_id >= NR_BT_CTF_FIELD_TYPE_ID_TYPES) {
		goto error;
	}

	field = field_create_funcs[type_id](type);
	if (!field) {
		goto error;
	}
	/* The type's declaration can't change after this point */
	bt_ctf_field_type_lock(type);

	bt_ctf_field_type_get(type);
	bt_ctf_ref_init(&field->ref_count, bt_ctf_field_destroy);
	field->type = type;
error:
	return field;
}

void bt_ctf_field_get(struct bt_ctf_field *field)
{
	if (field) {
		bt_ctf_ref_get(&field->ref_count);
	}
}

void bt_ctf_field_put(struct bt_ctf_field *field)
{
	if (field) {
		bt_ctf_ref_put(&field->ref_count);
	}
}

int bt_ctf_field_sequence_set_length(struct bt_ctf_field *field,
		struct bt_ctf_field *length_field)
{
	int ret = -1;
	if (!field || !length_field) {
		goto end;
	}
	if (bt_ctf_field_type_get_type_id(length_field->type) !=
		BT_CTF_FIELD_TYPE_ID_INTEGER) {
		goto end;
	}

	struct bt_ctf_field_type_integer *length_type = container_of(
		length_field->type, struct bt_ctf_field_type_integer, parent);
	if (length_type->_signed) {
		goto end;
	}

	struct bt_ctf_field_integer *length = container_of(length_field,
		struct bt_ctf_field_integer, parent);
	uint64_t sequence_length = length->payload._unsigned;
	struct bt_ctf_field_sequence *sequence = container_of(field,
		struct bt_ctf_field_sequence, parent);
	if (sequence->elements) {
		g_ptr_array_free(sequence->elements, TRUE);
		bt_ctf_field_put(sequence->length);
	}

	sequence->elements = g_ptr_array_new_full((size_t)sequence_length,
		(GDestroyNotify)bt_ctf_field_put);
	if (!sequence->elements) {
		goto end;
	}

	g_ptr_array_set_size(sequence->elements, (size_t)sequence_length);
	ret = 0;
	bt_ctf_field_get(length_field);
	sequence->length = length_field;
end:
	return ret;
}

struct bt_ctf_field *bt_ctf_field_structure_get_field(
		struct bt_ctf_field *field, const char *name)
{
	struct bt_ctf_field *new_field = NULL;
	if (!field || !name ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_STRUCTURE) {
		goto end;
	}

	GQuark field_quark = g_quark_from_string(name);
	struct bt_ctf_field_structure *structure = container_of(field,
		struct bt_ctf_field_structure, parent);
	struct bt_ctf_field_type_structure *structure_type = container_of(
		field->type, struct bt_ctf_field_type_structure, parent);
	struct bt_ctf_field_type *field_type =
		bt_ctf_field_type_structure_get_type(structure_type, name);
	size_t index = (size_t)g_hash_table_lookup(
		structure->field_name_to_index, GUINT_TO_POINTER(field_quark));
	if (structure->fields->pdata[index]) {
		new_field = structure->fields->pdata[index];
		goto end;
	}

	new_field = bt_ctf_field_create(field_type);
	if (!new_field) {
		goto end;
	}

	bt_ctf_field_get(new_field);
	structure->fields->pdata[index] = new_field;
end:
	return new_field;
}

struct bt_ctf_field *bt_ctf_field_array_get_field(struct bt_ctf_field *field,
		uint64_t index)
{
	struct bt_ctf_field *new_field = NULL;
	if (!field || bt_ctf_field_type_get_type_id(field->type) !=
		BT_CTF_FIELD_TYPE_ID_ARRAY) {
		goto end;
	}

	struct bt_ctf_field_array *array = container_of(field,
		struct bt_ctf_field_array, parent);
	if (index >= array->elements->len) {
		goto end;
	}

	struct bt_ctf_field_type_array *array_type = container_of(field->type,
		struct bt_ctf_field_type_array, parent);
	struct bt_ctf_field_type *field_type =
		bt_ctf_field_type_array_get_element_type(array_type);
	if (array->elements->pdata[(size_t)index]) {
		new_field = array->elements->pdata[(size_t)index];
		goto end;
	}

	new_field = bt_ctf_field_create(field_type);
	bt_ctf_field_get(new_field);
	array->elements->pdata[(size_t)index] = new_field;
end:
	return new_field;
}

struct bt_ctf_field *bt_ctf_field_sequence_get_field(struct bt_ctf_field *field,
		uint64_t index)
{
	struct bt_ctf_field *new_field = NULL;
	if (!field || bt_ctf_field_type_get_type_id(field->type) !=
		BT_CTF_FIELD_TYPE_ID_SEQUENCE) {
		goto end;
	}

	struct bt_ctf_field_sequence *sequence = container_of(field,
		struct bt_ctf_field_sequence, parent);
	if (!sequence->elements || sequence->elements->len <= index) {
		goto end;
	}

	struct bt_ctf_field_type_sequence *sequence_type = container_of(
		field->type, struct bt_ctf_field_type_sequence, parent);
	struct bt_ctf_field_type *field_type =
		bt_ctf_field_type_sequence_get_element_type(sequence_type);
	if (sequence->elements->pdata[(size_t)index]) {
		new_field = sequence->elements->pdata[(size_t)index];
		goto end;
	}

	new_field = bt_ctf_field_create(field_type);
	bt_ctf_field_get(new_field);
	sequence->elements->pdata[(size_t)index] = new_field;
end:
	return new_field;
}

struct bt_ctf_field *bt_ctf_field_variant_get_field(struct bt_ctf_field *field,
		struct bt_ctf_field *tag_field)
{
	struct bt_ctf_field *new_field = NULL;
	if (!field || !tag_field ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_VARIANT ||
		bt_ctf_field_type_get_type_id(tag_field->type) !=
			BT_CTF_FIELD_TYPE_ID_ENUMERATION) {
		goto end;
	}

	struct bt_ctf_field_variant *variant = container_of(field,
		struct bt_ctf_field_variant, parent);
	struct bt_ctf_field_type_variant *variant_type = container_of(
		field->type, struct bt_ctf_field_type_variant, parent);
	struct bt_ctf_field_enumeration *tag_enum = container_of(tag_field,
		struct bt_ctf_field_enumeration, parent);
	struct bt_ctf_field_type_enumeration *tag_type = container_of(
		tag_field->type, struct bt_ctf_field_type_enumeration, parent);
	struct bt_ctf_field_type *field_type =
		bt_ctf_field_type_variant_get_type(variant_type, tag_type,
			tag_enum->payload);
	if (!field_type) {
		goto end;
	}

	new_field = bt_ctf_field_create(field_type);
	if (!new_field) {
		goto end;
	}

	bt_ctf_field_put(variant->tag);
	bt_ctf_field_put(variant->payload);
	bt_ctf_field_get(new_field);
	bt_ctf_field_get(tag_field);
	variant->tag = tag_field;
	variant->payload = new_field;
end:
	return new_field;
}

int bt_ctf_field_signed_integer_set_value(struct bt_ctf_field *field,
		int64_t value)
{
	int ret = -1;
	if (!field ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_INTEGER) {
		goto end;
	}

	struct bt_ctf_field_integer *integer = container_of(field,
		struct bt_ctf_field_integer, parent);
	struct bt_ctf_field_type_integer *integer_type = container_of(
		field->type, struct bt_ctf_field_type_integer, parent);
	if (!integer_type->_signed) {
		goto end;
	}

	unsigned int size = integer_type->size;
	int64_t min_value = -(1 << (size - 1));
	int64_t max_value = (1 << (size - 1)) - 1;
	if (value < min_value || value > max_value) {
		goto end;
	}
	integer->payload._signed = value;
	ret = 0;
end:
	return ret;
}

int bt_ctf_field_unsigned_integer_set_value(struct bt_ctf_field *field,
		uint64_t value)
{
	int ret = -1;
	if (!field ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_INTEGER) {
		goto end;
	}

	struct bt_ctf_field_integer *integer = container_of(field,
		struct bt_ctf_field_integer, parent);
	struct bt_ctf_field_type_integer *integer_type = container_of(
		field->type, struct bt_ctf_field_type_integer, parent);
	if (integer_type->_signed) {
		goto end;
	}

	unsigned int size = integer_type->size;
	int64_t max_value = (1 << size) - 1;
	if (value > max_value) {
		goto end;
	}
	integer->payload._signed = value;
	ret = 0;
end:
	return ret;
}

int bt_ctf_field_floating_point_set_value(struct bt_ctf_field *field,
		long double value)
{
	int ret = 0;
	if (!field ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_FLOATING_POINT) {
		ret = -1;
		goto end;
	}
	struct bt_ctf_field_floating_point *floating_point = container_of(
		field, struct bt_ctf_field_floating_point, parent);
	floating_point->payload = value;
end:
	return ret;
}

int bt_ctf_field_string_set_value(struct bt_ctf_field *field,
		const char *value)
{
	int ret = 0;
	if (!field || !value ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_STRING) {
		ret = -1;
		goto end;
	}
	struct bt_ctf_field_string *string = container_of(field,
		struct bt_ctf_field_string, parent);
	string->payload = g_string_new(value);
end:
	return ret;
}

struct bt_ctf_field *bt_ctf_field_integer_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_integer *integer = g_new0(
		struct bt_ctf_field_integer, 1);
	if (!integer) {
		return NULL;
	}
	return &integer->parent;
}

struct bt_ctf_field *bt_ctf_field_enumeration_create(
	struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_enumeration *enumeration = g_new0(
		struct bt_ctf_field_enumeration, 1);
	if (!enumeration) {
		return NULL;
	}
	return &enumeration->parent;
}

struct bt_ctf_field *bt_ctf_field_floating_point_create(
	struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_floating_point *floating_point = g_new0(
		struct bt_ctf_field_floating_point, 1);
	if (!floating_point) {
		return NULL;
	}
	return &floating_point->parent;
}

struct bt_ctf_field *bt_ctf_field_structure_create(
	struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_structure *structure_type = container_of(type,
		struct bt_ctf_field_type_structure, parent);
	struct bt_ctf_field_structure *structure = g_new0(
		struct bt_ctf_field_structure, 1);
	struct bt_ctf_field *field = NULL;
	if (!structure) {
		goto end;
	}

	structure->field_name_to_index = structure_type->field_name_to_index;
	structure->fields = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_field_put);
	g_ptr_array_set_size(structure->fields,
		g_hash_table_size(structure->field_name_to_index));
	field = &structure->parent;
end:
	return field;
}

struct bt_ctf_field *bt_ctf_field_variant_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_variant *variant = g_new0(
		struct bt_ctf_field_variant, 1);
	if (!variant) {
		return NULL;
	}
	return &variant->parent;
}

struct bt_ctf_field *bt_ctf_field_array_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_array *array = g_new0(
		struct bt_ctf_field_array, 1);
	if (!array || !type) {
		goto error;
	}

	struct bt_ctf_field_type_array *array_type = container_of(type,
		 struct bt_ctf_field_type_array, parent);
	unsigned int array_length = array_type->length;
	array->elements = g_ptr_array_new_full(array_length,
		(GDestroyNotify)bt_ctf_field_put);
	if (!array->elements) {
		goto error;
	}

	g_ptr_array_set_size(array->elements, array_length);
	return &array->parent;
error:
	g_free(array);
	return NULL;
}

struct bt_ctf_field *bt_ctf_field_sequence_create(
	struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_sequence *sequence = g_new0(
		struct bt_ctf_field_sequence, 1);
	if (!sequence) {
		return NULL;
	}
	return &sequence->parent;
}

struct bt_ctf_field *bt_ctf_field_string_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_string *string = g_new0(
		struct bt_ctf_field_string, 1);
	if (!string) {
		return NULL;
	}
	return &string->parent;
}

void bt_ctf_field_destroy(struct bt_ctf_ref *ref)
{
	if (!ref) {
		return;
	}
	struct bt_ctf_field *field = container_of(ref, struct bt_ctf_field,
		ref_count);
	struct bt_ctf_field_type *type = field->type;
	enum bt_ctf_field_type_id type_id = bt_ctf_field_type_get_type_id(type);
	if (type_id <= BT_CTF_FIELD_TYPE_ID_UNKNOWN ||
		type_id >= NR_BT_CTF_FIELD_TYPE_ID_TYPES) {
		return;
	}

	field_destroy_funcs[type_id](field);
	if (type) {
		bt_ctf_field_type_put(type);
	}
}

void bt_ctf_field_integer_destroy(struct bt_ctf_field *field)
{
	if (!field) {
		return;
	}
	struct bt_ctf_field_integer *integer = container_of(field,
		struct bt_ctf_field_integer, parent);
	g_free(integer);
}

void bt_ctf_field_enumeration_destroy(struct bt_ctf_field *field)
{
	if (!field) {
		return;
	}
	struct bt_ctf_field_enumeration *enumeration = container_of(field,
		struct bt_ctf_field_enumeration, parent);
	g_free(enumeration);
}

void bt_ctf_field_floating_point_destroy(struct bt_ctf_field *field)
{
	if (!field) {
		return;
	}
	struct bt_ctf_field_floating_point *floating_point = container_of(field,
		struct bt_ctf_field_floating_point, parent);
	g_free(floating_point);
}

void bt_ctf_field_structure_destroy(struct bt_ctf_field *field)
{
	if (!field) {
		return;
	}
	struct bt_ctf_field_structure *structure = container_of(field,
		struct bt_ctf_field_structure, parent);
	g_ptr_array_free(structure->fields, TRUE);
	g_free(structure);
}

void bt_ctf_field_variant_destroy(struct bt_ctf_field *field)
{
	if (!field) {
		return;
	}
	struct bt_ctf_field_variant *variant = container_of(field,
		struct bt_ctf_field_variant, parent);
	bt_ctf_field_put(variant->tag);
	bt_ctf_field_put(variant->payload);
	g_free(variant);
}

void bt_ctf_field_array_destroy(struct bt_ctf_field *field)
{
	if (!field) {
		return;
	}
	struct bt_ctf_field_array *array = container_of(field,
		struct bt_ctf_field_array, parent);
	g_ptr_array_free(array->elements, TRUE);
	g_free(array);
}

void bt_ctf_field_sequence_destroy(struct bt_ctf_field *field)
{
	if (!field) {
		return;
	}
	struct bt_ctf_field_sequence *sequence = container_of(field,
		struct bt_ctf_field_sequence, parent);
	g_ptr_array_free(sequence->elements, TRUE);
	bt_ctf_field_put(sequence->length);
	g_free(sequence);
}

void bt_ctf_field_string_destroy(struct bt_ctf_field *field)
{
	if (!field) {
		return;
	}
	struct bt_ctf_field_string *string = container_of(field,
		struct bt_ctf_field_string, parent);
	g_string_free(string->payload, TRUE);
	g_free(string);
}
