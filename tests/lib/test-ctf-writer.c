/*
 * test-ctf-writer.c
 *
 * CTF Writer test
 *
 * Copyright 2013 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _GNU_SOURCE
#include <babeltrace/ctf-writer/writer.h>
#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

#include "tap.h"

int main(int argc, char **argv)
{
	plan_no_plan();

	struct bt_ctf_writer *writer =
		bt_ctf_writer_create("/home/decapsuleur/EfficiOS/TestTrace");
	ok(writer, "bt_ctf_create succeeds in creating trace with path");

	/* Add environment context to the trace */
	char hostname[HOST_NAME_MAX];
	gethostname(hostname, HOST_NAME_MAX);
	ok(bt_ctf_writer_add_environment_field(writer, "host", hostname) == 0,
		"Add host (%s) environment field to writer instance",
		hostname);
	ok(bt_ctf_writer_add_environment_field(NULL, "test_field",
		"test_value"),
		"bt_ctf_writer_add_environment_field error with NULL writer");
	ok(bt_ctf_writer_add_environment_field(writer, NULL,
		"test_value"),
		"bt_ctf_writer_add_environment_field error with NULL field name");
	ok(bt_ctf_writer_add_environment_field(writer, "test_field",
		NULL),
		"bt_ctf_writer_add_environment_field error with NULL field value");

	struct utsname *name = malloc(sizeof(struct utsname));
	if (uname(name)) {
		perror("uname");
		return -1;
	}

	ok(bt_ctf_writer_add_environment_field(writer, "sysname", name->sysname)
		== 0, "Add sysname (%s) environment field to writer instance",
		name->sysname);
	ok(bt_ctf_writer_add_environment_field(writer, "nodename",
		name->nodename) == 0,
		"Add nodename (%s) environment field to writer instance",
		name->nodename);
	ok(bt_ctf_writer_add_environment_field(writer, "release", name->release)
		== 0, "Add release (%s) environment field to writer instance",
		name->release);
	ok(bt_ctf_writer_add_environment_field(writer, "version", name->version)
		== 0, "Add version (%s) environment field to writer instance",
		name->version);
	ok(bt_ctf_writer_add_environment_field(writer, "machine", name->machine)
		== 0, "Add machine (%s) environment field to writer istance",
		name->machine);
	free(name);

	/* Define a clock and add it to the trace */
	const char *clock_name = "test_clock";
	const char *clock_description = "This is a test clock";

	ok(bt_ctf_clock_create("signed") == NULL, "Illegal clock name rejected");
	struct bt_ctf_clock *clock = bt_ctf_clock_create(clock_name);
	ok(clock, "Clock created sucessfully");
	ok(strcmp(clock_name, bt_ctf_clock_get_name(clock)) == 0,
		"Check bt_ctf_clock_get_name return value");
	bt_ctf_clock_set_description(clock, clock_description);
	ok(strcmp(clock_description, bt_ctf_clock_get_description(clock)) == 0,
		"Check bt_ctf_clock_get_description return value");

	const uint64_t frequency = 1000000000;
	const uint64_t offset_s = 1351530929945824323;
	const uint64_t offset = 1234567;
	const uint64_t precision = 10;

	ok(bt_ctf_clock_set_frequency(clock, frequency) == 0,
		"Set clock frequency");
	ok(bt_ctf_clock_get_frequency(clock) == frequency,
		"Check bt_ctf_clock_get_frequency return value");
	ok(bt_ctf_clock_set_offset_s(clock, offset_s) == 0,
		"Set clock offset (seconds)");
	ok(bt_ctf_clock_get_offset_s(clock) == offset_s,
		"Check bt_ctf_clock_get_offset_s return value");
	ok(bt_ctf_clock_set_offset(clock, offset) == 0, "Set clock offset");
	ok(bt_ctf_clock_get_offset(clock) == offset,
		"Check bt_ctf_clock_get_offset return value");
	ok(bt_ctf_clock_set_precision(clock, precision) == 0,
		"Set clock precision");
	ok(bt_ctf_clock_get_precision(clock) == precision,
		"Check bt_ctf_clock_get_precision return value");
	ok(bt_ctf_clock_set_is_absolute(clock, 0xFF) == 0,
		"Set clock absolute property");
	ok(bt_ctf_clock_is_absolute(clock),
		"Check bt_ctf_clock_is_absolute return value");

	ok(bt_ctf_writer_add_clock(writer, clock) == 0,
		"Add clock to writer instance");
	ok(bt_ctf_writer_add_clock(writer, clock),
		"Verify a clock can't be added twice to a writer instance");

	/* Define a stream class and field types */
	struct bt_ctf_stream_class *stream_class = bt_ctf_stream_class_create();
	ok(stream_class, "Create stream class");
	ok(bt_ctf_stream_class_set_clock(stream_class, clock) == 0,
		"Set a stream class' clock");

	struct bt_ctf_field_type *uint_12_type =
		bt_ctf_field_type_integer_create(12);
	ok(uint_12_type, "Create an unsigned integer type");
	ok(bt_ctf_field_type_integer_set_base(uint_12_type,
		BT_CTF_INTEGER_BASE_BINARY) == 0,
		"Set integer type's base as binary");
	ok(bt_ctf_field_type_integer_set_base(uint_12_type,
		BT_CTF_INTEGER_BASE_DECIMAL) == 0,
		"Set integer type's base as decimal");
	ok(bt_ctf_field_type_integer_set_base(uint_12_type,
		BT_CTF_INTEGER_BASE_UNKNOWN),
		"Reject integer type's base set as unknown");
	ok(bt_ctf_field_type_integer_set_base(uint_12_type,
		BT_CTF_INTEGER_BASE_OCTAL) == 0,
		"Set integer type's base as octal");
	ok(bt_ctf_field_type_integer_set_base(uint_12_type,
		BT_CTF_INTEGER_BASE_HEXADECIMAL) == 0,
		"Set integer type's base as hexadecimal");
	ok(bt_ctf_field_type_integer_set_base(uint_12_type, 457417),
		"Reject unknown integer base value");
	ok(bt_ctf_field_type_integer_set_signed(uint_12_type, 952835) == 0,
		"Set integer type signedness to signed");
	ok(bt_ctf_field_type_integer_set_signed(uint_12_type, 0) == 0,
		"Set integer type signedness to unsigned");

	struct bt_ctf_field_type *int_16_type =
		bt_ctf_field_type_integer_create(16);
	bt_ctf_field_type_integer_set_signed(int_16_type, 1);

	ok(bt_ctf_event_class_create("clock") == NULL,
		"Reject creation of an event class with an illegal name");
	struct bt_ctf_event_class *simple_event_class =
		bt_ctf_event_class_create("Simple Event");
	ok(simple_event_class, "Create an event class");
	ok(bt_ctf_event_class_add_field(simple_event_class, uint_12_type, ""),
		"Reject addition of a field with an empty name to an event");
	ok(bt_ctf_event_class_add_field(simple_event_class, NULL, "an_integer"),
		"Reject addition of a field with a NULL type to an event");
	ok(bt_ctf_event_class_add_field(simple_event_class, uint_12_type,
		"int"),
		"Reject addition of a type with an illegal name to an event");
	ok(bt_ctf_event_class_add_field(simple_event_class, uint_12_type,
		"uint_12") == 0,
		"Add field of type unsigned integer to an event");
	ok(bt_ctf_event_class_add_field(simple_event_class, int_16_type,
		"int_16") == 0, "Add field of type signed integer to an event");

	/* Add event class to the stream class */
	ok(bt_ctf_stream_class_add_event_class(stream_class, NULL),
		"Reject addition of NULL event class to a stream class");
	ok(bt_ctf_stream_class_add_event_class(stream_class,
		simple_event_class) == 0, "Add an event class to stream class");

	/* Instanciate a stream and an event */
	struct bt_ctf_stream *stream1 = bt_ctf_stream_create(stream_class);
	ok(stream1, "Instanciate a stream class");
	/* Should fail after instanciating a stream (locked)*/
	ok(bt_ctf_stream_class_set_clock(stream_class, clock),
		"Changes to a stream class that was already instanciated fail");

	struct bt_ctf_event *simple_event =
		bt_ctf_event_create(simple_event_class);
	ok(simple_event, "Instanciate an event class");

	struct bt_ctf_field *int_16 = bt_ctf_field_create(int_16_type);
	ok(int_16, "Instanciate a signed 16-bit integer");
	struct bt_ctf_field *uint_12 = bt_ctf_field_create(uint_12_type);
	ok(uint_12, "Instanciate an unsigned 12-bit integer");
	/* Can't modify types after instanciating them */
	ok(bt_ctf_field_type_integer_set_base(uint_12_type,
		BT_CTF_INTEGER_BASE_DECIMAL),
		"Check an integer type' base can't be modified after instanciation");
	ok(bt_ctf_field_type_integer_set_signed(uint_12_type, 0),
		"Check an integer type's signedness can't be modified after instanciation");

	/* Check signed property is checked */
	ok(bt_ctf_field_signed_integer_set_value(uint_12, -52),
		"Check bt_ctf_field_signed_integer_set_value is not allowed on an unsigned integer");
	ok(bt_ctf_field_unsigned_integer_set_value(int_16, 42),
		"Check bt_ctf_field_unsigned_integer_set_value is not allowed on a signed integer");

	/* Check the overflow is properly tested for */
	ok(bt_ctf_field_signed_integer_set_value(int_16, -32768) == 0,
		"Check -32768 is allowed for a signed 16-bit integer");
	ok(bt_ctf_field_signed_integer_set_value(int_16, 32767) == 0,
		"Check 32767 is allowed for a signed 16-bit integer");
	ok(bt_ctf_field_signed_integer_set_value(int_16, 32768),
		"Check 32768 is not allowed for a signed 16-bit integer");
	ok(bt_ctf_field_signed_integer_set_value(int_16, -32769),
		"Check -32769 is not allowed for a signed 16-bit integer");
	ok(bt_ctf_field_signed_integer_set_value(int_16, -42) == 0,
		"Check -42 is allowed for a signed 16-bit integer");

	ok(bt_ctf_field_unsigned_integer_set_value(uint_12, 4095) == 0,
		"Check 4095 is allowed for an unsigned 12-bit integer");
	ok(bt_ctf_field_unsigned_integer_set_value(uint_12, 4096),
		"Check 4096 is not allowed for a unsigned 12-bit integer");
	ok(bt_ctf_field_unsigned_integer_set_value(uint_12, 0) == 0,
		"Check 0 is allowed for an unsigned 12-bit integer");
	bt_ctf_field_unsigned_integer_set_value(uint_12, 1295);

	/* Set event payload */
	ok(bt_ctf_event_set_payload(simple_event, "uint_12", uint_12) == 0,
		"Set an event field payload");
	ok(bt_ctf_event_set_payload(simple_event, "uint_12", uint_12) == 0,
		"Change an event's existing payload");
	bt_ctf_event_set_payload(simple_event, "int_16", int_16);
	ok(bt_ctf_event_set_payload(simple_event, "int_16", uint_12),
		"Reject event payloads of incorrect type");

	char *metadata_string = bt_ctf_writer_get_metadata_string(writer);
	ok(metadata_string, "Get metadata string");

	bt_ctf_field_put(uint_12);
	bt_ctf_clock_put(clock);
	bt_ctf_field_put(int_16);
	bt_ctf_stream_class_put(stream_class);
	bt_ctf_event_class_put(simple_event_class);
	bt_ctf_field_type_put(uint_12_type);
	bt_ctf_field_type_put(int_16_type);
	bt_ctf_event_put(simple_event);
	bt_ctf_writer_put(writer);
	free(metadata_string);

	return 0;
}
