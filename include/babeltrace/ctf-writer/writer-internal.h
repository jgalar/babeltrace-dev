#ifndef _BABELTRACE_CTF_WRITER_INTERNAL_H
#define _BABELTRACE_CTF_WRITER_INTERNAL_H

/*
 * BabelTrace - CTF Writer
 *
 * CTF Writer Internal
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

#include <babeltrace/ctf-writer/ref-internal.h>
#include <babeltrace/ctf-writer/writer.h>
#include <babeltrace/babeltrace-internal.h>
#include <glib.h>
#include <dirent.h>
#include <sys/types.h>

struct bt_ctf_writer {
	struct bt_ctf_ref ref_count;
	int locked; /* Protects attributes that can't be changed mid-trace */
	GString *path;
	enum bt_ctf_byte_order endianness;
	DIR *trace_dir;
	int trace_dir_fd;
	GPtrArray *environment; /* Array of pointers to environment_variable */
	GPtrArray *clocks; /* Array of pointers to bt_ctf_clock */
	GPtrArray *streams; /* Array of pointers to bt_ctf_stream */
	GPtrArray *stream_classes; /* Array of pointers to bt_ctf_stream_class */
	uint32_t next_stream_id;
};

struct environment_variable {
	GString *name, *value;
};

struct metadata_context {
	GString *string;
	unsigned int indentation_level;
};

/* Checks that the string is not a reserved keyword */
BT_HIDDEN
int validate_identifier(const char *string);

#endif /* _BABELTRACE_CTF_WRITER_INTERNAL_H */
