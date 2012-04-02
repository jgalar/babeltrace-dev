#ifndef _BABELTRACE_TRACE_HANDLE_H
#define _BABELTRACE_TRACE_HANDLE_H

/*
 * BabelTrace
 *
 * trace_handle header
 *
 * Copyright 2012 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *         Julien Desfossez <julien.desfossez@efficios.com>
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
 */

#include <stdint.h>

/*
 * trace_handle : unique identifier of a trace
 *
 * The trace_handle allows the user to manipulate a trace file directly.
 * It is a unique identifier representing a trace file.
 */
struct bt_trace_handle;
struct bt_ctf_event;

/*
 * bt_trace_handle_get_path : returns the path of a trace_handle.
 */
const char *bt_trace_handle_get_path(struct bt_context *ctx, int handle_id);

/*
 * bt_trace_handle_get_timestamp_begin : returns the beginning timestamp
 * of a trace.
 */
uint64_t bt_trace_handle_get_timestamp_begin(struct bt_context *ctx, int handle_id);

/*
 * bt_trace_handle_get_timestamp_end : returns the end timestamp of a
 * trace.
 */
uint64_t bt_trace_handle_get_timestamp_end(struct bt_context *ctx, int handle_id);

/*
 * bt_ctf_event_get_handle_id : get the handle id associated with an event
 *
 * Returns -1 on error
 */
int bt_ctf_event_get_handle_id(const struct bt_ctf_event *event);

#endif /* _BABELTRACE_TRACE_HANDLE_H */
