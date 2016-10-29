/*
 * text.c
 *
 * Babeltrace CTF Text Output Plugin
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

#include <babeltrace/plugin/plugin-macros.h>
#include <babeltrace/plugin/component.h>
#include <babeltrace/plugin/sink.h>
#include <babeltrace/plugin/notification/notification.h>
#include <babeltrace/plugin/notification/iterator.h>
#include <babeltrace/plugin/notification/event.h>
#include <babeltrace/values.h>
#include <babeltrace/compiler.h>
#include <stdio.h>
#include <stdbool.h>
#include <glib.h>
#include "text.h"

#define PLUGIN_NAME	"text"

static
const char *plugin_options[] = {
	"output-path",
	"debug-info-dir",
	"debug-info-target-prefix",
	"debug-info-full-path",
	"no-delta",
	"clock-cycles",
	"clock-seconds",
	"clock-date",
	"clock-gmt",
	"name-payload",
	"name-context",
	"name-scope",
	"name-header",
	"name-all",
	"name-none",
	"field-trace",
	"field-trace:hostname",
	"field-trace:domain",
	"field-trace:procname",
	"field-trace:vpid",
	"field-loglevel",
	"field-emf",
	"field-callsite",
	"field-all",
};

static
const char *loglevel_str [] = {
	[LOGLEVEL_EMERG] =		"TRACE_EMERG",
	[LOGLEVEL_ALERT] =		"TRACE_ALERT",
	[LOGLEVEL_CRIT] =		"TRACE_CRIT",
	[LOGLEVEL_ERR] =		"TRACE_ERR",
	[LOGLEVEL_WARNING] =		"TRACE_WARNING",
	[LOGLEVEL_NOTICE] =		"TRACE_NOTICE",
	[LOGLEVEL_INFO] =		"TRACE_INFO",
	[LOGLEVEL_DEBUG_SYSTEM] =	"TRACE_DEBUG_SYSTEM",
	[LOGLEVEL_DEBUG_PROGRAM] =	"TRACE_DEBUG_PROGRAM",
	[LOGLEVEL_DEBUG_PROCESS] =	"TRACE_DEBUG_PROCESS",
	[LOGLEVEL_DEBUG_MODULE] =	"TRACE_DEBUG_MODULE",
	[LOGLEVEL_DEBUG_UNIT] =		"TRACE_DEBUG_UNIT",
	[LOGLEVEL_DEBUG_FUNCTION] =	"TRACE_DEBUG_FUNCTION",
	[LOGLEVEL_DEBUG_LINE] =		"TRACE_DEBUG_LINE",
	[LOGLEVEL_DEBUG] =		"TRACE_DEBUG",
};

static
void destroy_text_data(struct text_component *text)
{
	(void) g_string_free(text->string, TRUE);
	g_free(text->options.output_path);
	g_free(text->options.debug_info_dir);
	g_free(text->options.debug_info_target_prefix);
	g_free(text);
}

static
struct text_component *create_text(void)
{
	struct text_component *text;

	text = g_new0(struct text_component, 1);
	if (!text) {
		goto end;
	}
	text->string = g_string_new("");
	if (!text->string) {
		goto error;
	}
end:
	return text;

error:
	g_free(text);
	return NULL;
}

static
void destroy_text(struct bt_component *component)
{
	void *data = bt_component_get_private_data(component);

	destroy_text_data(data);
}

static
enum bt_component_status handle_notification(struct text_component *text,
		struct bt_notification *notification)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!text) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	switch (bt_notification_get_type(notification)) {
	case BT_NOTIFICATION_TYPE_PACKET_START:
		puts("<packet>");
		break;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		puts("</packet>");
		break;
	case BT_NOTIFICATION_TYPE_EVENT:
	{
		struct bt_ctf_event *event = bt_notification_event_get_event(
				notification);

		if (!event) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		ret = text_print_event(text, event);
		bt_put(event);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
		break;
	}
	case BT_NOTIFICATION_TYPE_STREAM_END:
		puts("</stream>");
		break;
	default:
		puts("Unhandled notification type");
	}
end:
	return ret;
}

static
enum bt_component_status run(struct bt_component *component)
{
	enum bt_component_status ret;
	struct bt_notification *notification = NULL;
	struct bt_notification_iterator *it;
	struct text_component *text = bt_component_get_private_data(component);

	ret = bt_component_sink_get_input_iterator(component, 0, &it);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	if (!text->processed_first_event) {
		ret = bt_notification_iterator_next(it);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
	} else {
		text->processed_first_event = true;
	}

	notification = bt_notification_iterator_get_notification(it);
	if (!notification) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	ret = handle_notification(text, notification);
end:
	bt_put(it);
	bt_put(notification);
	return ret;
}

static
enum bt_component_status add_params_to_map(struct bt_value *plugin_opt_map)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	unsigned int i;

	for (i = 0; i < BT_ARRAY_SIZE(plugin_options); i++) {
		const char *key = plugin_options[i];
		enum bt_value_status status;

		status = bt_value_map_insert(plugin_opt_map, key, bt_value_null);
		switch (status) {
		case BT_VALUE_STATUS_OK:
			break;
		default:
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
	}
end:
	return ret;
}

static
bool check_param_exists(const char *key, struct bt_value *object, void *data)
{
	struct text_component *text = data;
	struct bt_value *plugin_opt_map = text->plugin_opt_map;

	if (!bt_value_map_get(plugin_opt_map, key)) {
		fprintf(text->err,
			"[warning] Parameter \"%s\" unknown to \"%s\" plugin\n",
			key, PLUGIN_NAME);
	}
	return true;
}

static
enum bt_component_status apply_one_string(const char *key,
		struct bt_value *params,
		char **option)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_value *value = NULL;
	enum bt_value_status status;
	const char *str;

	value = bt_value_map_get(params, key);
	if (!value) {
		goto end;
	}
	if (bt_value_is_null(value)) {
		goto end;
	}
	status = bt_value_string_get(value, &str);
	switch (status) {
	case BT_VALUE_STATUS_OK:
		break;
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	*option = g_strdup(str);
end:
	bt_put(value);
	return ret;
}

static
enum bt_component_status apply_one_bool(const char *key,
		struct bt_value *params,
		bool *option,
		bool *found)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_value *value = NULL;
	enum bt_value_status status;

	value = bt_value_map_get(params, key);
	if (!value) {
		goto end;
	}
	status = bt_value_bool_get(value, option);
	switch (status) {
	case BT_VALUE_STATUS_OK:
		break;
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (found) {
		*found = true;
	}
end:
	bt_put(value);
	return ret;
}

static
enum bt_component_status apply_params(struct text_component *text,
		struct bt_value *params)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	enum bt_value_status status;
	bool value, found, found_all, found_none;

	text->plugin_opt_map = bt_value_map_create();
	if (!text->plugin_opt_map) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	ret = add_params_to_map(text->plugin_opt_map);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	/* Report unknown parameters. */
	status = bt_value_map_foreach(params, check_param_exists, text);
	switch (status) {
	case BT_VALUE_STATUS_OK:
		break;
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	/* Known parameters. */
	ret = apply_one_string("output-path",
			params,
			&text->options.output_path);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = apply_one_string("debug-info-dir",
			params,
			&text->options.debug_info_dir);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = apply_one_string("debug-info-target-prefix",
			params,
			&text->options.debug_info_target_prefix);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	value = false;		/* Default. */
	ret = apply_one_bool("debug-info-full-path", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.debug_info_full_path = value;

	value = false;		/* Default. */
	ret = apply_one_bool("no-delta", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_delta_field = !value;	/* Reverse logic. */

	value = false;		/* Default. */
	ret = apply_one_bool("clock-cycles", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_timestamp_cycles = value;

	value = false;		/* Default. */
	ret = apply_one_bool("clock-seconds", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.clock_seconds = value;

	value = false;		/* Default. */
	ret = apply_one_bool("clock-date", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.clock_date = value;

	value = false;		/* Default. */
	ret = apply_one_bool("clock-gmt", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.clock_gmt = value;

	/* Names. */

	found = false;
	value = false;
	ret = apply_one_bool("name-payload", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_payload_field_names = value;

	value = false;
	ret = apply_one_bool("name-context", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_context_field_names = value;

	value = false;
	ret = apply_one_bool("name-header", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_header_field_names = value;

	value = false;
	ret = apply_one_bool("name-scope", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_scope_field_names = value;

	if (!found) {
		/* Apply defaults. */
		text->options.print_payload_field_names = true;
		text->options.print_context_field_names = true;
		text->options.print_header_field_names = false;
		text->options.print_scope_field_names = false;
	}

	found_all = false;
	value = false;		/* Default. */
	ret = apply_one_bool("name-all", params, &value, &found_all);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found_all && found) {
		fprintf(text->err, "[error] Found incompatible \"name-all\" and specific name arguments.\n");
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (value) {
		text->options.print_payload_field_names = true;
		text->options.print_context_field_names = true;
		text->options.print_header_field_names = true;
		text->options.print_scope_field_names = true;
	}

	found_none = false;
	value = false;		/* Default. */
	ret = apply_one_bool("name-none", params, &value, &found_none);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found_none && found) {
		fprintf(text->err, "[error] Found incompatible \"name-none\" and specific name arguments.\n");
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (found_none && found_all) {
		fprintf(text->err, "[error] Found incompatible \"name-all\" and \"name-none\" arguments.\n");
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (value) {
		text->options.print_payload_field_names = false;
		text->options.print_context_field_names = false;
		text->options.print_header_field_names = false;
		text->options.print_scope_field_names = false;
	}

	found = false;
	value = false;
	ret = apply_one_bool("field-trace", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_trace_field = value;

	value = false;
	ret = apply_one_bool("field-trace:hostname", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_trace_hostname_field = value;

	value = false;
	ret = apply_one_bool("field-trace:domain", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_trace_domain_field = value;

	value = false;
	ret = apply_one_bool("field-trace:procname", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_trace_procname_field = value;

	value = false;
	ret = apply_one_bool("field-trace:vpid", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_trace_vpid_field = value;

	value = false;
	ret = apply_one_bool("field-loglevel", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_loglevel_field = value;

	value = false;
	ret = apply_one_bool("field-emf", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_emf_field = value;

	value = false;
	ret = apply_one_bool("field-emf", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_emf_field = value;

	value = false;
	ret = apply_one_bool("field-callsite", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_callsite_field = value;

	if (!found) {
		/* Apply defaults. */
		text->options.print_trace_field = false;
		text->options.print_trace_hostname_field = true;
		text->options.print_trace_domain_field = false;
		text->options.print_trace_procname_field = true;
		text->options.print_trace_vpid_field = true;
		text->options.print_loglevel_field = false;
		text->options.print_emf_field = false;
		text->options.print_emf_field = false;
		text->options.print_callsite_field = false;
	}

	found_all = false;
	value = false;		/* Default. */
	ret = apply_one_bool("field-all", params, &value, &found_all);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found && found_all) {
		fprintf(text->err, "[error] Found incompatible \"field-all\" and specific field arguments.\n");
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (value) {
		text->options.print_trace_field = true;
		text->options.print_trace_hostname_field = true;
		text->options.print_trace_domain_field = true;
		text->options.print_trace_procname_field = true;
		text->options.print_trace_vpid_field = true;
		text->options.print_loglevel_field = true;
		text->options.print_emf_field = true;
		text->options.print_emf_field = true;
		text->options.print_callsite_field = true;
	}
end:
	bt_put(text->plugin_opt_map);
	text->plugin_opt_map = NULL;
	return ret;
}

static
enum bt_component_status text_component_init(
		struct bt_component *component, struct bt_value *params)
{
	enum bt_component_status ret;
	struct text_component *text = create_text();

	if (!text) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	text->out = stdout;
	text->err = stderr;

	ret = apply_params(text, params);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = bt_component_set_destroy_cb(component,
			destroy_text);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = bt_component_set_private_data(component, text);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = bt_component_sink_set_consume_cb(component,
			run);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}
end:
	return ret;
error:
	destroy_text_data(text);
	return ret;
}

/* Initialize plug-in entry points. */
BT_PLUGIN_NAME("text");
BT_PLUGIN_DESCRIPTION("Babeltrace text output plug-in.");
BT_PLUGIN_AUTHOR("Jérémie Galarneau");
BT_PLUGIN_LICENSE("MIT");

BT_PLUGIN_COMPONENT_CLASSES_BEGIN
BT_PLUGIN_SINK_COMPONENT_CLASS_ENTRY(PLUGIN_NAME,
		"Formats CTF-IR to text. Formerly known as ctf-text.",
		text_component_init)
BT_PLUGIN_COMPONENT_CLASSES_END
