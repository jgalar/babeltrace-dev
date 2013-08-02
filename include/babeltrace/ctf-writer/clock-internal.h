#ifndef _BABELTRACE_CTF_WRITER_CLOCK_INTERNAL_H
#define _BABELTRACE_CTF_WRITER_CLOCK_INTERNAL_H

/*
 * BabelTrace - CTF Writer
 *
 * CTF Writer Clock
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
#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <glib.h>
#include <uuid/uuid.h>

struct bt_ctf_clock {
	struct bt_ctf_ref ref_count;
	GString *name;
	GString *description;
	uint64_t frequency, precision, offset_s, offset, time;
	uuid_t uuid;
	int absolute;
	/*
	 * A clock's properties can't be modified once it is added to a stream
	 * class.
	 */
	int locked;
};

BT_HIDDEN
void bt_ctf_clock_lock(struct bt_ctf_clock *clock);

BT_HIDDEN
void bt_ctf_clock_serialize(struct bt_ctf_clock *clock,
		struct metadata_context *context);

#endif /* _BABELTRACE_CTF_WRITER_CLOCK_INTERNAL_H */
