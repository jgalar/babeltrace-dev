#ifndef _BABELTRACE_CTF_WRITER_H
#define _BABELTRACE_CTF_WRITER_H

/*
 * BabelTrace - CTF Writer
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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_writer;
struct bt_ctf_stream;
struct bt_ctf_clock;

enum bt_ctf_byte_order {
	BT_CTF_BYTE_ORDER_NATIVE = 0,
	BT_CTF_BYTE_ORDER_LITTLE_ENDIAN,
	BT_CTF_BYTE_ORDER_BIG_ENDIAN,
	BT_CTF_BYTE_ORDER_NETWORK,
	BT_CTF_BYTE_ORDER_END
};

extern struct bt_ctf_writer *bt_ctf_writer_create(const char *path);

extern void bt_ctf_writer_get(struct bt_ctf_writer *writer);

extern void bt_ctf_writer_put(struct bt_ctf_writer *writer);

extern int bt_ctf_writer_add_stream(struct bt_ctf_writer *writer,
		struct bt_ctf_stream *stream);

extern int bt_ctf_writer_add_environment_field(struct bt_ctf_writer *writer,
		const char *name,
		const char *value);

extern int bt_ctf_writer_add_clock(struct bt_ctf_writer *writer,
		struct bt_ctf_clock *clock);

extern char *bt_ctf_writer_get_metadata_string(struct bt_ctf_writer *writer);

/* Defaults to the host's native endianness (BT_CTF_BYTE_ORDER_NATIVE) */
extern int bt_ctf_writer_set_endianness(struct bt_ctf_writer *writer,
		enum bt_ctf_byte_order endianness);

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_CTF_WRITER_H */
