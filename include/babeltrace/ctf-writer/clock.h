#ifndef _BABELTRACE_CTF_CLOCK_H
#define _BABELTRACE_CTF_CLOCK_H

/*
 * BabelTrace - CTF Clock
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

struct bt_ctf_clock;

extern struct bt_ctf_clock *bt_ctf_clock_create(const char *name);

extern void bt_ctf_clock_get(struct bt_ctf_clock *clock);

extern void bt_ctf_clock_put(struct bt_ctf_clock *clock);

extern int bt_ctf_clock_set_description(struct bt_ctf_clock *clock,
		const char *desc);

/* Frequency in Hz, defaults to 1000000000 Hz (1ns) */
extern int bt_ctf_clock_set_frequency(struct bt_ctf_clock *clock,
		uint64_t freq);

/* Precision in clock ticks, defaults to 1 */
extern int bt_ctf_clock_set_precision(struct bt_ctf_clock *clock,
		uint64_t precision);

/* Offset in seconds from POSIX.1 Epoch, 1970-01-01, defaults to 0 */
extern int bt_ctf_clock_set_offset_s(struct bt_ctf_clock *clock,
		uint64_t offset_s);

/* Offset in clock ticks from Epoch + offset_s, defaults to 0 */
extern int bt_ctf_clock_set_offset(struct bt_ctf_clock *clock,
		uint64_t offset);

/*
 * Absolute if the clock is a global reference across the trace's
 * other clocks. Defaults to FALSE.
 */
extern int bt_ctf_clock_set_is_absolute(struct bt_ctf_clock *clock,
		int is_absolute);

/*
 * Set the current time in nanoseconds since the clock's origin (offset
 * attributes)
 * The clock's value will be sampled as events are pushed to a stream.
 */
extern int bt_ctf_clock_set_time(struct bt_ctf_clock *clock, uint64_t time);

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_CTF_CLOCK_H */
