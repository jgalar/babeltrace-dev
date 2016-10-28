#ifndef BABELTRACE_PLUGIN_TEXT_H
#define BABELTRACE_PLUGIN_TEXT_H

/*
 * BabelTrace - CTF Text Output Plug-in
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <stdbool.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/plugin/component.h>

enum loglevel {
        LOGLEVEL_EMERG                  = 0,
        LOGLEVEL_ALERT                  = 1,
        LOGLEVEL_CRIT                   = 2,
        LOGLEVEL_ERR                    = 3,
        LOGLEVEL_WARNING                = 4,
        LOGLEVEL_NOTICE                 = 5,
        LOGLEVEL_INFO                   = 6,
        LOGLEVEL_DEBUG_SYSTEM           = 7,
        LOGLEVEL_DEBUG_PROGRAM          = 8,
        LOGLEVEL_DEBUG_PROCESS          = 9,
        LOGLEVEL_DEBUG_MODULE           = 10,
        LOGLEVEL_DEBUG_UNIT             = 11,
        LOGLEVEL_DEBUG_FUNCTION         = 12,
        LOGLEVEL_DEBUG_LINE             = 13,
        LOGLEVEL_DEBUG                  = 14,
};

struct text_options {
	bool print_scope_field_names : 1;
	bool print_header_field_names : 1;
	bool print_context_field_names : 1;
	bool print_payload_field_names : 1;
	bool print_delta_field : 1;
	bool print_loglevel_field : 1;
	bool print_trace_field : 1;
	bool print_trace_domain_field : 1;
	bool print_trace_procname_field : 1;
	bool print_trace_vpid_field : 1;
	bool print_trace_hostname_field : 1;
	bool print_timestamp_cycles : 1;
	bool no_size_limit : 1;
};

struct text_component {
	struct text_options options;
	FILE *out, *err;
	bool processed_first_event;
	uint64_t last_real_timestamp;
	int depth;	/* nesting, used for tabulation alignment. */
	GString *string;
};

BT_HIDDEN
enum bt_component_status text_print_event(struct text_component *text,
		struct bt_ctf_event *event);

#endif /* BABELTRACE_PLUGIN_TEXT_H */
