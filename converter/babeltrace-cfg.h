#ifndef BABELTRACE_CONVERTER_CFG_H
#define BABELTRACE_CONVERTER_CFG_H

/*
 * Babeltrace trace converter - configuration
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
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

#include <stdlib.h>
#include <babeltrace/values.h>
#include <glib.h>

struct bt_cfg_component {
	GString *plugin_name;
	GString *component_name;
	struct bt_value *params;
};

enum legacy_input_format {
	LEGACY_INPUT_FORMAT_NONE = 0,
	LEGACY_INPUT_FORMAT_CTF,
	LEGACY_INPUT_FORMAT_LTTNG_LIVE,
};

enum legacy_output_format {
	LEGACY_OUTPUT_FORMAT_NONE = 0,
	LEGACY_OUTPUT_FORMAT_TEXT,
	LEGACY_OUTPUT_FORMAT_CTF_METADATA,
	LEGACY_OUTPUT_FORMAT_DUMMY,
};

struct bt_cfg {
	bool debug;
	bool verbose;
	bool do_list;
	bool force_correlate;
	enum legacy_input_format legacy_input_format;
	enum legacy_output_format legacy_output_format;
	struct bt_value *legacy_input_base_params;
	struct bt_value *legacy_output_base_params;
	struct bt_value *legacy_input_paths;
	struct bt_value *plugin_paths;
	GPtrArray *sources;
	GPtrArray *sinks;
};

static inline
struct bt_cfg_component *bt_cfg_get_cfg_component(GPtrArray *array, size_t index)
{
	return g_ptr_array_index(array, index);
}

void bt_cfg_destroy(struct bt_cfg *bt_cfg);
struct bt_cfg *bt_cfg_from_args(int argc, char *argv[], int *exit_code);

#endif /* BABELTRACE_CONVERTER_CFG_H */
