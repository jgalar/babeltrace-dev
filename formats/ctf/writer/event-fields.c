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

static int bt_ctf_field_generic_validate(struct bt_ctf_field *field);
static int bt_ctf_field_structure_validate(struct bt_ctf_field *field);
static int bt_ctf_field_variant_validate(struct bt_ctf_field *field);
static int bt_ctf_field_enumeration_validate(struct bt_ctf_field *field);
static int bt_ctf_field_array_validate(struct bt_ctf_field *field);
static int bt_ctf_field_sequence_validate(struct bt_ctf_field *field);

static int bt_ctf_field_integer_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);
static int bt_ctf_field_enumeration_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);
static int bt_ctf_field_floating_point_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);
static int bt_ctf_field_structure_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);
static int bt_ctf_field_variant_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);
static int bt_ctf_field_array_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);
static int bt_ctf_field_sequence_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);
static int bt_ctf_field_string_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);

static struct bt_ctf_field *(*field_create_funcs[])(
	struct bt_ctf_field_type *) =
{
	[CTF_TYPE_INTEGER] = bt_ctf_field_integer_create,
	[CTF_TYPE_ENUM] = bt_ctf_field_enumeration_create,
	[CTF_TYPE_FLOAT] =
		bt_ctf_field_floating_point_create,
	[CTF_TYPE_STRUCT] = bt_ctf_field_structure_create,
	[CTF_TYPE_VARIANT] = bt_ctf_field_variant_create,
	[CTF_TYPE_ARRAY] = bt_ctf_field_array_create,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_sequence_create,
	[CTF_TYPE_STRING] = bt_ctf_field_string_create
};

static void (*field_destroy_funcs[])(struct bt_ctf_field *) =
{
	[CTF_TYPE_INTEGER] = bt_ctf_field_integer_destroy,
	[CTF_TYPE_ENUM] = bt_ctf_field_enumeration_destroy,
	[CTF_TYPE_FLOAT] =
		bt_ctf_field_floating_point_destroy,
	[CTF_TYPE_STRUCT] = bt_ctf_field_structure_destroy,
	[CTF_TYPE_VARIANT] = bt_ctf_field_variant_destroy,
	[CTF_TYPE_ARRAY] = bt_ctf_field_array_destroy,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_sequence_destroy,
	[CTF_TYPE_STRING] = bt_ctf_field_string_destroy
};

static int (*field_validate_funcs[])(struct bt_ctf_field *) =
{
	[CTF_TYPE_INTEGER] = bt_ctf_field_generic_validate,
	[CTF_TYPE_ENUM] = bt_ctf_field_enumeration_validate,
	[CTF_TYPE_FLOAT] = bt_ctf_field_generic_validate,
	[CTF_TYPE_STRUCT] = bt_ctf_field_structure_validate,
	[CTF_TYPE_VARIANT] = bt_ctf_field_variant_validate,
	[CTF_TYPE_ARRAY] = bt_ctf_field_array_validate,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_sequence_validate,
	[CTF_TYPE_STRING] = bt_ctf_field_generic_validate
};

static int (*field_serialize_funcs[])(struct bt_ctf_field *,
		struct ctf_stream_pos *) =
{
	[CTF_TYPE_INTEGER] = bt_ctf_field_integer_serialize,
	[CTF_TYPE_ENUM] = bt_ctf_field_enumeration_serialize,
	[CTF_TYPE_FLOAT] =
		bt_ctf_field_floating_point_serialize,
	[CTF_TYPE_STRUCT] = bt_ctf_field_structure_serialize,
	[CTF_TYPE_VARIANT] = bt_ctf_field_variant_serialize,
	[CTF_TYPE_ARRAY] = bt_ctf_field_array_serialize,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_sequence_serialize,
	[CTF_TYPE_STRING] = bt_ctf_field_string_serialize
};

struct bt_ctf_field *bt_ctf_field_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field *field = NULL;
	enum ctf_type_id type_id;

	if (!type) {
		goto error;
	}

	type_id = bt_ctf_field_type_get_type_id(type);
	if (type_id <= CTF_TYPE_UNKNOWN ||
		type_id >= NR_CTF_TYPES) {
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
	struct bt_ctf_field_type_integer *length_type;
	struct bt_ctf_field_integer *length;
	struct bt_ctf_field_sequence *sequence;
	uint64_t sequence_length;

	if (!field || !length_field) {
		goto end;
	}
	if (bt_ctf_field_type_get_type_id(length_field->type) !=
		CTF_TYPE_INTEGER) {
		goto end;
	}

	length_type = container_of(length_field->type,
		struct bt_ctf_field_type_integer, parent);
	if (length_type->declaration.signedness) {
		goto end;
	}

	length = container_of(length_field, struct bt_ctf_field_integer,
		parent);
	sequence_length = length->definition.value._unsigned;
	sequence = container_of(field, struct bt_ctf_field_sequence, parent);
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
	GQuark field_quark;
	struct bt_ctf_field_structure *structure;
	struct bt_ctf_field_type_structure *structure_type;
	struct bt_ctf_field_type *field_type;
	size_t index;

	if (!field || !name ||
		bt_ctf_field_type_get_type_id(field->type) !=
			CTF_TYPE_STRUCT) {
		goto end;
	}

	field_quark = g_quark_from_string(name);
	structure = container_of(field, struct bt_ctf_field_structure, parent);
	structure_type = container_of(field->type,
		struct bt_ctf_field_type_structure, parent);
	field_type = bt_ctf_field_type_structure_get_type(structure_type, name);
	index = (size_t)g_hash_table_lookup(structure->field_name_to_index,
		GUINT_TO_POINTER(field_quark));
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

int bt_ctf_field_structure_set_field(struct bt_ctf_field *field,
		const char *name, struct bt_ctf_field *value)
{
	int ret = 0;
	GQuark field_quark;
	struct bt_ctf_field_structure *structure;
	struct bt_ctf_field_type_structure *structure_type;
	struct bt_ctf_field_type *expected_field_type;
	size_t index;

	if (!field || !name ||
		bt_ctf_field_type_get_type_id(field->type) !=
			CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	field_quark = g_quark_from_string(name);
	structure = container_of(field, struct bt_ctf_field_structure, parent);
	structure_type = container_of(field->type,
		struct bt_ctf_field_type_structure, parent);
	expected_field_type = bt_ctf_field_type_structure_get_type(
		structure_type, name);
	index = (size_t)g_hash_table_lookup(structure->field_name_to_index,
		GUINT_TO_POINTER(field_quark));

	/*
	 * Make sure value is of the appropriate type. This is currently
	 * compared pointer-wise, but we should have equality operators
	 */
	if (expected_field_type != value->type) {
		ret = -1;
		goto end;
	}

	if (structure->fields->pdata[index]) {
		bt_ctf_field_put(structure->fields->pdata[index]);
	}

	structure->fields->pdata[index] = value;
	bt_ctf_field_get(value);
end:
	return ret;
}

struct bt_ctf_field *bt_ctf_field_array_get_field(struct bt_ctf_field *field,
		uint64_t index)
{
	struct bt_ctf_field *new_field = NULL;
	struct bt_ctf_field_array *array;
	struct bt_ctf_field_type_array *array_type;
	struct bt_ctf_field_type *field_type;

	if (!field || bt_ctf_field_type_get_type_id(field->type) !=
		CTF_TYPE_ARRAY) {
		goto end;
	}

	array = container_of(field, struct bt_ctf_field_array, parent);
	if (index >= array->elements->len) {
		goto end;
	}

	array_type = container_of(field->type, struct bt_ctf_field_type_array,
		parent);
	field_type = bt_ctf_field_type_array_get_element_type(array_type);
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
	struct bt_ctf_field_sequence *sequence;
	struct bt_ctf_field_type_sequence *sequence_type;
	struct bt_ctf_field_type *field_type;

	if (!field || bt_ctf_field_type_get_type_id(field->type) !=
		CTF_TYPE_SEQUENCE) {
		goto end;
	}

	sequence = container_of(field, struct bt_ctf_field_sequence, parent);
	if (!sequence->elements || sequence->elements->len <= index) {
		goto end;
	}

	sequence_type = container_of(field->type,
		struct bt_ctf_field_type_sequence, parent);
	field_type = bt_ctf_field_type_sequence_get_element_type(sequence_type);
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
	struct bt_ctf_field_variant *variant;
	struct bt_ctf_field_type_variant *variant_type;
	struct bt_ctf_field_enumeration *tag_enum;
	struct bt_ctf_field_type *field_type;
	struct bt_ctf_field_integer *tag_enum_integer;
	int64_t tag_enum_value;

	if (!field || !tag_field || !tag_field->payload_set ||
		bt_ctf_field_type_get_type_id(field->type) !=
			CTF_TYPE_VARIANT ||
		bt_ctf_field_type_get_type_id(tag_field->type) !=
			CTF_TYPE_ENUM) {
		goto end;
	}

	variant = container_of(field, struct bt_ctf_field_variant, parent);
	variant_type = container_of(field->type,
		struct bt_ctf_field_type_variant, parent);
	tag_enum = container_of(tag_field, struct bt_ctf_field_enumeration,
		parent);
	tag_enum_integer = container_of(tag_enum->payload,
		struct bt_ctf_field_integer, parent);

	tag_enum_value = tag_enum_integer->definition.value._signed;
	field_type = bt_ctf_field_type_variant_get_field_type(variant_type,
		tag_enum_value);
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

struct bt_ctf_field *bt_ctf_field_enumeration_get_container(
	struct bt_ctf_field *field)
{
	struct bt_ctf_field *container = NULL;
	struct bt_ctf_field_enumeration *enumeration;

	if (!field) {
		goto end;
	}

	enumeration = container_of(field, struct bt_ctf_field_enumeration,
		parent);
	if (!enumeration->payload) {
		struct bt_ctf_field_type_enumeration *enumeration_type =
			container_of(field->type,
			struct bt_ctf_field_type_enumeration, parent);
		enumeration->payload =
			bt_ctf_field_create(enumeration_type->container);
	}

	container = enumeration->payload;
	bt_ctf_field_get(container);
end:
	return container;
}

int bt_ctf_field_signed_integer_set_value(struct bt_ctf_field *field,
		int64_t value)
{
	int ret = -1;
	struct bt_ctf_field_integer *integer;
	struct bt_ctf_field_type_integer *integer_type;
	unsigned int size;
	int64_t min_value, max_value;

	if (!field ||
		bt_ctf_field_type_get_type_id(field->type) !=
			CTF_TYPE_INTEGER) {
		goto end;
	}

	integer = container_of(field, struct bt_ctf_field_integer, parent);
	integer_type = container_of(field->type,
		struct bt_ctf_field_type_integer, parent);
	if (!integer_type->declaration.signedness) {
		goto end;
	}

	size = integer_type->declaration.len;
	min_value = -((int64_t)1 << (size - 1));
	max_value = ((int64_t)1 << (size - 1)) - 1;
	if (value < min_value || value > max_value) {
		goto end;
	}

	integer->definition.value._signed = value;
	integer->parent.payload_set = 1;
	ret = 0;
end:
	return ret;
}

int bt_ctf_field_unsigned_integer_set_value(struct bt_ctf_field *field,
		uint64_t value)
{
	int ret = -1;
	struct bt_ctf_field_integer *integer;
	struct bt_ctf_field_type_integer *integer_type;
	unsigned int size;
	uint64_t max_value;

	if (!field ||
		bt_ctf_field_type_get_type_id(field->type) !=
			CTF_TYPE_INTEGER) {
		goto end;
	}

	integer = container_of(field, struct bt_ctf_field_integer, parent);
	integer_type = container_of(field->type,
		struct bt_ctf_field_type_integer, parent);
	if (integer_type->declaration.signedness) {
		goto end;
	}

	size = integer_type->declaration.len;
	max_value = ((uint64_t)1 << size) - 1;
	if (value > max_value) {
		goto end;
	}
	integer->definition.value._signed = value;
	ret = 0;
	integer->parent.payload_set = 1;
end:
	return ret;
}

int bt_ctf_field_floating_point_set_value(struct bt_ctf_field *field,
		long double value)
{
	int ret = 0;
	struct bt_ctf_field_floating_point *floating_point;

	if (!field ||
		bt_ctf_field_type_get_type_id(field->type) !=
			CTF_TYPE_FLOAT) {
		ret = -1;
		goto end;
	}
	floating_point = container_of(field, struct bt_ctf_field_floating_point,
		parent);
	floating_point->payload = value;
	floating_point->parent.payload_set = 1;
end:
	return ret;
}

int bt_ctf_field_string_set_value(struct bt_ctf_field *field,
		const char *value)
{
	int ret = 0;
	struct bt_ctf_field_string *string;

	if (!field || !value ||
		bt_ctf_field_type_get_type_id(field->type) !=
			CTF_TYPE_STRING) {
		ret = -1;
		goto end;
	}

	string = container_of(field, struct bt_ctf_field_string, parent);
	string->payload = g_string_new(value);
	string->parent.payload_set = 1;
end:
	return ret;
}

int bt_ctf_field_validate(struct bt_ctf_field *field)
{
	int ret = 0;
	enum ctf_type_id type_id;

	if (!field) {
		ret = -1;
		goto end;
	}

	type_id = bt_ctf_field_type_get_type_id(field->type);
	ret = field_validate_funcs[type_id](field);
end:
	return ret;
}

int bt_ctf_field_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
{
	int ret = 0;
	enum ctf_type_id type_id;

	if (!field) {
		ret = -1;
		goto end;
	}

	type_id = bt_ctf_field_type_get_type_id(field->type);
	ret = field_serialize_funcs[type_id](field, pos);
end:
	return ret;
}

struct bt_ctf_field *bt_ctf_field_integer_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_integer *integer_type = container_of(type,
		struct bt_ctf_field_type_integer, parent);
	struct bt_ctf_field_integer *integer = g_new0(
		struct bt_ctf_field_integer, 1);

	if (integer) {
		integer->definition.declaration = &integer_type->declaration;
	}

	return integer ? &integer->parent : NULL;
}

struct bt_ctf_field *bt_ctf_field_enumeration_create(
	struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_enumeration *enumeration = g_new0(
		struct bt_ctf_field_enumeration, 1);

	return enumeration ? &enumeration->parent : NULL;
}

struct bt_ctf_field *bt_ctf_field_floating_point_create(
	struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_floating_point *floating_point = g_new0(
		struct bt_ctf_field_floating_point, 1);

	return floating_point ? &floating_point->parent : NULL;
}

struct bt_ctf_field *bt_ctf_field_structure_create(
	struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_structure *structure_type = container_of(type,
		struct bt_ctf_field_type_structure, parent);
	struct bt_ctf_field_structure *structure = g_new0(
		struct bt_ctf_field_structure, 1);
	struct bt_ctf_field *field = NULL;

	if (!structure || !structure_type->fields->len) {
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
	return variant ? &variant->parent : NULL;
}

struct bt_ctf_field *bt_ctf_field_array_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_array *array = g_new0(struct bt_ctf_field_array, 1);
	struct bt_ctf_field_type_array *array_type;
	unsigned int array_length;

	if (!array || !type) {
		goto error;
	}

	array_type = container_of(type, struct bt_ctf_field_type_array, parent);
	array_length = array_type->length;
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
	return sequence ? &sequence->parent : NULL;
}

struct bt_ctf_field *bt_ctf_field_string_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_string *string = g_new0(
		struct bt_ctf_field_string, 1);
	return string ? &string->parent : NULL;
}

void bt_ctf_field_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_field *field;
	struct bt_ctf_field_type *type;
	enum ctf_type_id type_id;

	if (!ref) {
		return;
	}

	field = container_of(ref, struct bt_ctf_field, ref_count);
	type = field->type;
	type_id = bt_ctf_field_type_get_type_id(type);
	if (type_id <= CTF_TYPE_UNKNOWN ||
		type_id >= NR_CTF_TYPES) {
		return;
	}

	field_destroy_funcs[type_id](field);
	if (type) {
		bt_ctf_field_type_put(type);
	}
}

void bt_ctf_field_integer_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_integer *integer;

	if (!field) {
		return;
	}

	integer = container_of(field, struct bt_ctf_field_integer, parent);
	g_free(integer);
}

void bt_ctf_field_enumeration_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_enumeration *enumeration;

	if (!field) {
		return;
	}

	enumeration = container_of(field, struct bt_ctf_field_enumeration,
		parent);
	bt_ctf_field_put(enumeration->payload);
	g_free(enumeration);
}

void bt_ctf_field_floating_point_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_floating_point *floating_point;

	if (!field) {
		return;
	}

	floating_point = container_of(field, struct bt_ctf_field_floating_point,
		parent);
	g_free(floating_point);
}

void bt_ctf_field_structure_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_structure *structure;

	if (!field) {
		return;
	}

	structure = container_of(field, struct bt_ctf_field_structure, parent);
	g_ptr_array_free(structure->fields, TRUE);
	g_free(structure);
}

void bt_ctf_field_variant_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_variant *variant;

	if (!field) {
		return;
	}

	variant = container_of(field, struct bt_ctf_field_variant, parent);
	bt_ctf_field_put(variant->tag);
	bt_ctf_field_put(variant->payload);
	g_free(variant);
}

void bt_ctf_field_array_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_array *array;

	if (!field) {
		return;
	}

	array = container_of(field, struct bt_ctf_field_array, parent);
	g_ptr_array_free(array->elements, TRUE);
	g_free(array);
}

void bt_ctf_field_sequence_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_sequence *sequence;

	if (!field) {
		return;
	}

	sequence = container_of(field, struct bt_ctf_field_sequence, parent);
	g_ptr_array_free(sequence->elements, TRUE);
	bt_ctf_field_put(sequence->length);
	g_free(sequence);
}

void bt_ctf_field_string_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_string *string;
	if (!field) {
		return;
	}

	string = container_of(field, struct bt_ctf_field_string, parent);
	g_string_free(string->payload, TRUE);
	g_free(string);
}

int bt_ctf_field_generic_validate(struct bt_ctf_field *field)
{
	return !(field && field->payload_set);
}

int bt_ctf_field_enumeration_validate(struct bt_ctf_field *field)
{
	int ret;
	struct bt_ctf_field_enumeration *enumeration;

	if (!field) {
		ret = -1;
		goto end;
	}

	enumeration = container_of(field, struct bt_ctf_field_enumeration,
		parent);
	if (!enumeration->payload) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_validate(enumeration->payload);
end:
	return ret;
}

int bt_ctf_field_structure_validate(struct bt_ctf_field *field)
{
	int ret = 0;
	struct bt_ctf_field_structure * structure;

	if (!field) {
		ret = -1;
		goto end;
	}

	structure = container_of(field, struct bt_ctf_field_structure, parent);
	for (size_t i = 0; i < structure->fields->len; i++) {
		ret = bt_ctf_field_validate(structure->fields->pdata[i]);
		if (ret) {
			goto end;
		}
	}
end:
	return ret;
}

int bt_ctf_field_variant_validate(struct bt_ctf_field *field)
{
	int ret = 0;
	struct bt_ctf_field_variant *variant;

	if (!field) {
		ret = -1;
		goto end;
	}

	variant = container_of(field, struct bt_ctf_field_variant, parent);
	ret = bt_ctf_field_validate(variant->payload);
end:
	return ret;
}

int bt_ctf_field_array_validate(struct bt_ctf_field *field)
{
	int ret = 0;
	struct bt_ctf_field_array *array;

	if (!field) {
		ret = -1;
		goto end;
	}

	array = container_of(field, struct bt_ctf_field_array, parent);
	for (size_t i = 0; i < array->elements->len; i++) {
		ret = bt_ctf_field_validate(array->elements->pdata[i]);
		if (ret) {
			goto end;
		}
	}
end:
	return ret;
}

int bt_ctf_field_sequence_validate(struct bt_ctf_field *field)
{
	int ret = 0;
	struct bt_ctf_field_sequence *sequence;

	if (!field) {
		ret = -1;
		goto end;
	}

	sequence = container_of(field, struct bt_ctf_field_sequence, parent);
	for (size_t i = 0; i < sequence->elements->len; i++) {
		ret = bt_ctf_field_validate(sequence->elements->pdata[i]);
		if (ret) {
			goto end;
		}
	}
end:
	return ret;
}

int bt_ctf_field_integer_serialize(struct bt_ctf_field * field,
		struct ctf_stream_pos *pos)
{

	return -1;
}

int bt_ctf_field_enumeration_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
{
	return -1;
}

int bt_ctf_field_floating_point_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
{
	return -1;
}

int bt_ctf_field_structure_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
{
	int ret = 0;
	struct bt_ctf_field_structure *structure = container_of(
		field, struct bt_ctf_field_structure, parent);

	for (size_t i = 0; i < structure->fields->len; i++) {
		struct bt_ctf_field *field = g_ptr_array_index(
			structure->fields, i);
		ret = bt_ctf_field_serialize(field, pos);
		if (ret) {
			break;
		}
	}

	return ret;
}

int bt_ctf_field_variant_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
{
	return -1;
}

int bt_ctf_field_array_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
{
	return -1;
}

int bt_ctf_field_sequence_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
{
	return -1;
}

int bt_ctf_field_string_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
{
	return -1;
}
