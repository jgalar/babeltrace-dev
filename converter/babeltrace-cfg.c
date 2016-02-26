/*
 * Babeltrace trace converter - parameter parsing
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/values.h>
#include <popt.h>
#include <glib.h>
#include "babeltrace-cfg.h"

enum state_expecting {
	STATE_EXPECTING_MAP_KEY,
	STATE_EXPECTING_EQUAL,
	STATE_EXPECTING_VALUE,
	STATE_EXPECTING_VALUE_NUMBER_NEG,
	STATE_EXPECTING_COMMA,
};

struct state {
	enum state_expecting expecting;
	char *last_map_key;
	const char *arg;
	GString *ini_error;
};

static
void print_error_expecting(struct state *state, GScanner *scanner,
		const char *expecting)
{
	size_t i;
	g_string_append_printf(state->ini_error, "Expecting %s:\n", expecting);
	size_t pos;

	/* Only print error if there's one line */
	if (strchr(state->arg, '\n') != NULL || strlen(state->arg) == 0) {
		return;
	}

	g_string_append_printf(state->ini_error, "\n    %s\n", state->arg);
	pos = g_scanner_cur_position(scanner) + 4;

	if (!g_scanner_eof(scanner)) {
		pos--;
	}

	for (i = 0; i < pos; ++i) {
		g_string_append_printf(state->ini_error, " ");
	}

	g_string_append_printf(state->ini_error, "^\n\n");
}

static
int handle_state(struct state *state, GScanner *scanner,
		struct bt_value *params)
{
	int ret = 0;
	GTokenType token_type;
	struct bt_value *value = NULL;

	token_type = g_scanner_get_next_token(scanner);
	if (token_type == G_TOKEN_EOF) {
		if (state->expecting != STATE_EXPECTING_COMMA) {
			switch (state->expecting) {
			case STATE_EXPECTING_EQUAL:
				print_error_expecting(state, scanner, "'='");
				break;
			case STATE_EXPECTING_VALUE:
			case STATE_EXPECTING_VALUE_NUMBER_NEG:
				print_error_expecting(state, scanner, "value");
				break;
			case STATE_EXPECTING_MAP_KEY:
				print_error_expecting(state, scanner,
					"unquoted map key");
				break;
			default:
				break;
			}
			goto error;
		}

		/* We're done! */
		ret = 1;
		goto end;
	}

	switch (state->expecting) {
	case STATE_EXPECTING_MAP_KEY:
		if (token_type != G_TOKEN_IDENTIFIER) {
			print_error_expecting(state, scanner,
				"unquoted map key");
			goto error;
		}

		free(state->last_map_key);
		state->last_map_key = strdup(scanner->value.v_identifier);
		if (bt_value_map_has_key(params, state->last_map_key)) {
			g_string_append_printf(state->ini_error,
				"Duplicate parameter key: \"%s\"\n",
				state->last_map_key);
			goto error;
		}

		state->expecting = STATE_EXPECTING_EQUAL;
		goto end;
	case STATE_EXPECTING_EQUAL:
		if (token_type != G_TOKEN_CHAR) {
			print_error_expecting(state, scanner, "'='");
			goto error;
		}

		if (scanner->value.v_char != '=') {
			print_error_expecting(state, scanner, "'='");
			goto error;
		}

		state->expecting = STATE_EXPECTING_VALUE;
		goto end;
	case STATE_EXPECTING_VALUE:
	{
		switch (token_type) {
		case G_TOKEN_CHAR:
			if (scanner->value.v_char == '-') {
				/* Negative number */
				state->expecting =
					STATE_EXPECTING_VALUE_NUMBER_NEG;
				goto end;
			} else {
				print_error_expecting(state, scanner, "value");
				goto error;
			}
			break;
		case G_TOKEN_INT:
		{
			/* Positive integer */
			uint64_t int_val = scanner->value.v_int64;

			if (int_val > (1ULL << 63) - 1) {
				g_string_append_printf(state->ini_error,
					"Integer value %" PRIu64 " is outside the range of a 64-bit signed integer\n",
					int_val);
				goto error;
			}

			value = bt_value_integer_create_init(
				(int64_t) int_val);
			break;
		}
		case G_TOKEN_FLOAT:
			/* Positive floating point number */
			value = bt_value_float_create_init(
				scanner->value.v_float);
			break;
		case G_TOKEN_STRING:
			/* Quoted string */
			value = bt_value_string_create_init(
				scanner->value.v_string);
			break;
		case G_TOKEN_IDENTIFIER:
		{
			const char *id;

			/*
			 * Using symbols would be appropriate here,
			 * but said symbols are allowed as map key,
			 * so it's easier to consider everything an
			 * identifier.
			 *
			 * If one of the known symbols is not
			 * recognized here, then fall back to creating
			 * a string.
			 */
			id = scanner->value.v_identifier;

			if (!strcmp(id, "null") || !strcmp(id, "NULL") ||
					!strcmp(id, "nul")) {
				value = bt_value_null;
			} else if (!strcmp(id, "true") || !strcmp(id, "TRUE") ||
					!strcmp(id, "yes") ||
					!strcmp(id, "YES")) {
				value = bt_value_bool_create_init(true);
			} else if (!strcmp(id, "false") ||
					!strcmp(id, "FALSE") ||
					!strcmp(id, "no") ||
					!strcmp(id, "NO")) {
				value = bt_value_bool_create_init(false);
			} else {
				value = bt_value_string_create_init(id);
			}
			break;
		}
		default:
			break;
		}

		if (!value) {
			print_error_expecting(state, scanner, "value");
			goto error;
		}

		state->expecting = STATE_EXPECTING_COMMA;
		goto end;
	}
	case STATE_EXPECTING_VALUE_NUMBER_NEG:
	{
		switch (token_type) {
		case G_TOKEN_INT:
		{
			/* Negative integer */
			uint64_t int_val = scanner->value.v_int64;

			if (int_val > (1ULL << 63) - 1) {
				g_string_append_printf(state->ini_error,
					"Integer value -%" PRIu64 " is outside the range of a 64-bit signed integer\n",
					int_val);
				goto error;
			}

			value = bt_value_integer_create_init(
				-((int64_t) int_val));
			break;
		}
		case G_TOKEN_FLOAT:
			/* Negative floating point number */
			value = bt_value_float_create_init(
				-scanner->value.v_float);
			break;
		default:
			break;
		}

		if (!value) {
			print_error_expecting(state, scanner, "value");
			goto error;
		}

		state->expecting = STATE_EXPECTING_COMMA;
		goto end;
	}
	case STATE_EXPECTING_COMMA:
		if (token_type != G_TOKEN_CHAR) {
			print_error_expecting(state, scanner, "','");
			goto error;
		}

		if (scanner->value.v_char != ',') {
			print_error_expecting(state, scanner, "','");
			goto error;
		}

		state->expecting = STATE_EXPECTING_MAP_KEY;
		goto end;
	default:
		assert(false);
	}

error:
	/* Successful states should go to end explicitly */
	ret = -1;

end:
	if (value) {
		if (bt_value_map_insert(params, state->last_map_key, value)) {
			/* Only override return value on error */
			ret = -1;
		}
	}

	BT_PUT(value);
	return ret;
}

static
struct bt_value *bt_value_from_ini(const char *arg, GString *ini_error)
{
	GScannerConfig scanner_config = {
		.cset_skip_characters = " \t\n",
		.cset_identifier_first =
			G_CSET_a_2_z
			"_"
			G_CSET_A_2_Z,
		.cset_identifier_nth =
			G_CSET_a_2_z
			"_0123456789-.:"
			G_CSET_A_2_Z,
		.cpair_comment_single = "#\n",
		.case_sensitive = TRUE,
		.skip_comment_multi = TRUE,
		.skip_comment_single = TRUE,
		.scan_comment_multi = FALSE,
		.scan_identifier = TRUE,
		.scan_identifier_1char = TRUE,
		.scan_identifier_NULL = FALSE,
		.scan_symbols = TRUE,
		.scan_binary = TRUE,
		.scan_octal = TRUE,
		.scan_float = TRUE,
		.scan_hex = TRUE,
		.scan_hex_dollar = FALSE,
		.scan_string_sq = FALSE,
		.scan_string_dq = TRUE,
		.numbers_2_int = TRUE,
		.int_2_float = FALSE,
		.identifier_2_string = FALSE,
		.char_2_token = FALSE,
		.symbol_2_token = FALSE,
		.scope_0_fallback = FALSE,
		.store_int64 = TRUE,
	};
	GScanner *scanner;
	struct bt_value *params = NULL;
	struct state state = {
		.expecting = STATE_EXPECTING_MAP_KEY,
		.arg = arg,
		.ini_error = ini_error,
	};

	params = bt_value_map_create();
	if (!params) {
		goto error;
	}

	scanner = g_scanner_new(&scanner_config);
	if (!scanner) {
		goto error;
	}

	/* Let the scan begin */
	g_scanner_input_text(scanner, arg, strlen(arg));

	while (true) {
		int ret = handle_state(&state, scanner, params);

		if (ret < 0) {
			/* Error */
			goto error;
		} else if (ret > 0) {
			/* Done */
			break;
		}
	}

	goto end;

error:
	BT_PUT(params);

end:
	if (scanner) {
		g_scanner_destroy(scanner);
	}

	free(state.last_map_key);

	return params;
}

static
struct bt_value *bt_value_from_arg(const char *arg)
{
	struct bt_value *params = NULL;
	const char *colon;
	const char *params_string;
	GString *ini_error = NULL;

	/* Isolate parameters */
	colon = strchr(arg, ':');

	if (!colon) {
		/* No colon: empty parameters */
		params = bt_value_map_create();
		goto end;
	}

	params_string = colon + 1;
	ini_error = g_string_new(NULL);
	if (!ini_error) {
		goto end;
	}

	/* Try ini-style parsing first */
	params = bt_value_from_ini(params_string, ini_error);
	if (params) {
		goto end;
	}

	/* Fall back to JSON parsing */
	params = bt_value_from_json(params_string);
	if (!bt_value_is_map(params)) {
		BT_PUT(params);
		fprintf(stderr, "Invalid JSON or INI-style parameters\n");
		fprintf(stderr, "When trying to parse as INI-style:\n");
		fprintf(stderr, "%s", ini_error->str);
		goto end;
	}

end:
	if (ini_error) {
		g_string_free(ini_error, TRUE);
	}

	return params;
}

static
void get_plugin_component_names_from_arg(const char *arg, char **plugin,
		char **component)
{
	const char *dot;
	const char *colon;
	size_t plugin_len;
	size_t component_len;

	*plugin = NULL;
	*component = NULL;

	dot = strchr(arg, '.');
	if (!dot || dot == arg) {
		goto end;
	}

	colon = strchr(dot, ':');
	if (colon == dot) {
		goto end;
	}

	if (!colon) {
		colon = arg + strlen(arg);
	}

	plugin_len = dot - arg;
	component_len = colon - dot - 1;

	if (plugin_len == 0 || component_len == 0) {
		goto end;
	}

	*plugin = malloc(plugin_len + 1);
	if (!*plugin) {
		goto end;
	}

	(*plugin)[plugin_len] = '\0';
	memcpy(*plugin, arg, plugin_len);

	*component = malloc(component_len + 1);
	if (!*component) {
		goto end;
	}

	(*component)[component_len] = '\0';
	memcpy(*component, dot + 1, component_len);

end:
	return;
}

static
void show_version(FILE *fp)
{
	fprintf(fp, "Babeltrace v" VERSION "\n");
}

static
void show_legacy_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace [OPTIONS] INPUT...\n");
	fprintf(fp, "\n");
	fprintf(fp, "The following options are compatible with the Babeltrace 1.x options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "      --help-legacy            Show this help\n");
	fprintf(fp, "  -V, --version                Show version\n");
	fprintf(fp, "      --clock-force-correlate  Assume that clocks are inherently correlated\n");
	fprintf(fp, "                               across traces\n");
	fprintf(fp, "  -d, --debug                  Enable debug mode\n");
	fprintf(fp, "  -i, --input-format=FORMAT    Input trace format (default: ctf)\n");
	fprintf(fp, "  -l, --list                   List available formats\n");
	fprintf(fp, "  -o, --output-format=FORMAT   Output trace format (default: text)\n");
	fprintf(fp, "  -v, --verbose                Enable verbose output\n");
	fprintf(fp, "\n");
	fprintf(fp, "  Available input formats:  ctf, lttng-live, ctf-metadata\n");
	fprintf(fp, "  Available output formats: text, dummy\n");
	fprintf(fp, "\n");
	fprintf(fp, "Input plugins specific options:\n");
	fprintf(fp, "\n");
	fprintf(fp, "  INPUT...                     Input trace file(s), directory(ies), or URLs\n");
	fprintf(fp, "      --clock-offset=SEC       Set clock offset to SEC seconds\n");
	fprintf(fp, "      --clock-offset-ns=NS     Set clock offset to NS nanoseconds\n");
	fprintf(fp, "\n");
	fprintf(fp, "ctf-text output plugin specific options:\n");
	fprintf(fp, "  \n");
	fprintf(fp, "      --clock-cycles           Print timestamps in clock cycles\n");
	fprintf(fp, "      --clock-date             Print timestamp dates\n");
	fprintf(fp, "      --clock-gmt              Print timestamps in GMT time zone\n");
	fprintf(fp, "                               (default: local time zone)\n");
	fprintf(fp, "      --clock-seconds          Print the timestamps as [SEC.NS]\n");
	fprintf(fp, "                               (default format: [HH:MM:SS.NS])\n");
	fprintf(fp, "  -f, --fields=NAME[,NAME]...  Print additional fields:\n");
	fprintf(fp, "                                 all, trace, trace:hostname, trace:domain,\n");
	fprintf(fp, "                                 trace:procname, trace:vpid, loglevel, emf,\n");
	fprintf(fp, "                                 callsite\n");
	fprintf(fp, "                                 (default: trace:hostname, trace:procname,\n");
	fprintf(fp, "                                           trace:vpid)\n");
	fprintf(fp, "  -n, --names=NAME[,NAME]...   Print field names:\n");
	fprintf(fp, "                                 payload (or arg or args)\n");
	fprintf(fp, "                                 none, all, scope, header, context (or ctx)\n");
	fprintf(fp, "                                 (default: payload, context)\n");
	fprintf(fp, "      --no-delta               Do not print time delta between consecutive\n");
	fprintf(fp, "                               events\n");
	fprintf(fp, "  -w, --output=PATH            Write output to PATH (default: standard output)\n");
}

static
void show_usage(FILE *fp)
{
	fprintf(fp, "Usage: babeltrace [OPTIONS]\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -h  --help                        Show this help\n");
	fprintf(fp, "      --help-legacy                 Show Babeltrace 1.x legacy options\n");
	fprintf(fp, "  -d, --debug                       Enable debug mode\n");
	fprintf(fp, "  -i, --source=SOURCE               Add source plugin/component SOURCE and its\n");
	fprintf(fp, "                                    parameters to the active sources (may be\n");
	fprintf(fp, "                                    repeated; see the exact format below) \n");
	fprintf(fp, "  -l, --list                        List available plugins and their components\n");
	fprintf(fp, "  -i, --sink=SINK                   Add sink plugin/component SINK and its\n");
	fprintf(fp, "                                    parameters to the active sinks\n");
	fprintf(fp, "  -p, --plugin-path=PATH[:PATH]...  Set paths from which dynamic plugins can be\n");
	fprintf(fp, "                                    loaded to PATH\n");
	fprintf(fp, "  -v, --verbose                     Enable verbose output\n");
	fprintf(fp, "  -V, --version                     Show version\n");
	fprintf(fp, "\n");
	fprintf(fp, "SOURCE/SINK argument format:\n");
	fprintf(fp, "\n");
	fprintf(fp, "  One of:\n");
	fprintf(fp, "\n");
	fprintf(fp, "    PLUGIN.COMPONENT\n");
	fprintf(fp, "      Load component COMPONENT from plugin PLUGIN with its default parameters.\n");
	fprintf(fp, "\n");
	fprintf(fp, "    PLUGIN.COMPONENT:PARAM=VALUE[,PARAM=VALUE]...\n");
	fprintf(fp, "      Load component COMPONENT from plugin PLUGIN with the specified parameters.\n");
	fprintf(fp, "\n");
	fprintf(fp, "      The parameter string is a comma-separated list of PARAM=VALUE tokens,\n");
	fprintf(fp, "      where PARAM is the parameter name, and VALUE can be one of:\n");
	fprintf(fp, "\n");
	fprintf(fp, "        * \"null\", \"nul\", \"NULL\": null value\n");
	fprintf(fp, "        * \"true\", \"TRUE\", \"yes\", \"YES\": true boolean value\n");
	fprintf(fp, "        * \"false\", \"FALSE\", \"no\", \"NO\": false boolean value\n");
	fprintf(fp, "        * Binary (\"0b\" prefix), octal (\"0\" prefix), decimal, or\n");
	fprintf(fp, "          hexadecimal (\"0x\" prefix) integer fitting in a signed 64-bit integer\n");
	fprintf(fp, "        * Floating point number\n");
	fprintf(fp, "        * Unquoted string with no special characters, and not matching any of\n");
	fprintf(fp, "          the null, true boolean, and false boolean value symbols above\n");
	fprintf(fp, "        * Double-quoted string (accepts escaped characters)\n");
	fprintf(fp, "\n");
	fprintf(fp, "      Example:\n");
	fprintf(fp, "\n");
	fprintf(fp, "          plugin.comp:many=null, fresh=yes, condition=false, squirrel=-782329,\n");
	fprintf(fp, "                      observe=3.14, simple=beef, needs-quotes=\"some string\",\n");
	fprintf(fp, "                      escape-chars-are-allowed=\"this is a \\\" double quote\"\n");
	fprintf(fp, "\n");
	fprintf(fp, "    PLUGIN.COMPONENT:JSON\n");
	fprintf(fp, "      Load component COMPONENT from plugin PLUGIN, specifying the parameters\n");
	fprintf(fp, "      as a JSON object.\n");
	fprintf(fp, "\n");
	fprintf(fp, "      Example:\n");
	fprintf(fp, "\n");
	fprintf(fp, "          plugin.comp:{\n");
	fprintf(fp, "            \"chosen\": [2, 3, 6, 13, null, 19],\n");
	fprintf(fp, "            \"colorize\": {\n");
	fprintf(fp, "              \"db.connect\": \"#27ae60\",\n");
	fprintf(fp, "              \"db.drop\": \"#e74c3c\",\n");
	fprintf(fp, "              \"*\": null\n");
	fprintf(fp, "            }\n");
	fprintf(fp, "          }\n");
}

static
void bt_cfg_component_destroy(void *data)
{
	struct bt_cfg_component *bt_cfg_component = data;

	if (!bt_cfg_component) {
		return;
	}

	if (bt_cfg_component->plugin_name) {
		g_string_free(bt_cfg_component->plugin_name, TRUE);
	}

	if (bt_cfg_component->component_name) {
		g_string_free(bt_cfg_component->component_name, TRUE);
	}

	BT_PUT(bt_cfg_component->params);
	g_free(bt_cfg_component);
}

static
struct bt_cfg_component *bt_cfg_component_from_arg(const char *arg)
{
	struct bt_cfg_component *bt_cfg_component = NULL;
	char *plugin_name;
	char *component_name;
	struct bt_value *params = NULL;

	get_plugin_component_names_from_arg(arg, &plugin_name, &component_name);
	if (!plugin_name || !component_name) {
		fprintf(stderr, "Cannot get plugin or component name\n");
		goto error;
	}

	params = bt_value_from_arg(arg);
	if (!params) {
		fprintf(stderr, "Cannot parse parameters\n");
		goto error;
	}

	bt_cfg_component = g_new0(struct bt_cfg_component, 1);
	if (!bt_cfg_component) {
		goto error;
	}

	bt_cfg_component->plugin_name = g_string_new(plugin_name);
	if (!bt_cfg_component->plugin_name) {
		goto error;
	}

	bt_cfg_component->component_name = g_string_new(component_name);
	if (!bt_cfg_component->component_name) {
		goto error;
	}

	BT_MOVE(bt_cfg_component->params, params);
	goto end;

error:
	bt_cfg_component_destroy(bt_cfg_component);
	bt_cfg_component = NULL;

end:
	free(plugin_name);
	free(component_name);
	BT_PUT(params);

	return bt_cfg_component;
}

void bt_cfg_destroy(struct bt_cfg *bt_cfg)
{
	if (!bt_cfg) {
		return;
	}

	if (bt_cfg->sources) {
		g_ptr_array_free(bt_cfg->sources, TRUE);
	}

	if (bt_cfg->sinks) {
		g_ptr_array_free(bt_cfg->sinks, TRUE);
	}

	BT_PUT(bt_cfg->plugin_paths);
	BT_PUT(bt_cfg->legacy_input_base_params);
	BT_PUT(bt_cfg->legacy_output_base_params);
	BT_PUT(bt_cfg->legacy_input_paths);
	g_free(bt_cfg);
}

static
struct bt_value *plugin_paths_from_arg(const char *arg)
{
	struct bt_value *plugin_paths;
	const char *at = arg;
	const char *end = arg + strlen(arg);

	plugin_paths = bt_value_array_create();
	if (!plugin_paths) {
		goto error;
	}

	while (at < end) {
		int ret;
		GString *path;
		const char *next_colon;

		path = g_string_new(NULL);
		if (!path) {
			goto error;
		}

		next_colon = strchr(at, ':');
		if (next_colon == at || !next_colon) {
			next_colon = arg + strlen(arg);
		}

		g_string_append_len(path, at, next_colon - at);
		at = next_colon + 1;
		ret = bt_value_array_append_string(plugin_paths, path->str);
		g_string_free(path, TRUE);
		if (ret) {
			goto error;
		}
	}

	goto end;

error:
	BT_PUT(plugin_paths);

end:
	return plugin_paths;
}

static
GScanner *create_simple_scanner(void)
{
	GScannerConfig scanner_config = {
		.cset_skip_characters = " \t\n",
		.cset_identifier_first =
			G_CSET_a_2_z
			"_"
			G_CSET_A_2_Z,
		.cpair_comment_single = "#\n",
		.case_sensitive = TRUE,
		.skip_comment_multi = TRUE,
		.skip_comment_single = TRUE,
		.scan_comment_multi = FALSE,
		.scan_identifier = TRUE,
		.scan_identifier_1char = TRUE,
		.scan_identifier_NULL = FALSE,
		.identifier_2_string = FALSE,
		.char_2_token = TRUE,
	};

	return g_scanner_new(&scanner_config);
}

static
struct bt_value *names_from_arg(const char *arg)
{
	GScanner *scanner;
	struct bt_value *names = NULL;

	names = bt_value_array_create();
	if (!names) {
		goto error;
	}

	scanner = create_simple_scanner();
	if (!scanner) {
		goto error;
	}

	g_scanner_input_text(scanner, arg, strlen(arg));

	while (true) {
		GTokenType token_type = g_scanner_get_next_token(scanner);

		switch (token_type) {
		case G_TOKEN_IDENTIFIER:
		{
			const char *identifier = scanner->value.v_identifier;

			if (!strcmp(identifier, "payload") ||
					!strcmp(identifier, "args") ||
					!strcmp(identifier, "arg")) {
				if (bt_value_array_append_string(names,
						"payload")) {
					goto error;
				}
			} else if (!strcmp(identifier, "context") ||
					!strcmp(identifier, "ctx")) {
				if (bt_value_array_append_string(names,
						"context")) {
					goto error;
				}
			} else if (!strcmp(identifier, "none") ||
					!strcmp(identifier, "all") ||
					!strcmp(identifier, "scope") ||
					!strcmp(identifier, "header")) {
				if (bt_value_array_append_string(names,
						identifier)) {
					goto error;
				}
			} else {
				fprintf(stderr, "Unknown field name: \"%s\"\n",
					identifier);
				goto error;
			}
			break;
		}
		case G_TOKEN_COMMA:
			continue;
		case G_TOKEN_EOF:
			goto end;
		default:
			goto error;
		}
	}

	goto end;

error:
	BT_PUT(names);

end:
	g_scanner_destroy(scanner);

	return names;
}

static
struct bt_value *fields_from_arg(const char *arg)
{
	GScanner *scanner;
	struct bt_value *fields = NULL;

	fields = bt_value_array_create();
	if (!fields) {
		goto error;
	}

	scanner = create_simple_scanner();
	if (!scanner) {
		goto error;
	}

	g_scanner_input_text(scanner, arg, strlen(arg));

	while (true) {
		GTokenType token_type = g_scanner_get_next_token(scanner);

		switch (token_type) {
		case G_TOKEN_IDENTIFIER:
		{
			const char *identifier = scanner->value.v_identifier;

			if (!strcmp(identifier, "all") ||
					!strcmp(identifier, "trace") ||
					!strcmp(identifier, "trace:hostname") ||
					!strcmp(identifier, "trace:domain") ||
					!strcmp(identifier, "trace:procname") ||
					!strcmp(identifier, "trace:vpid") ||
					!strcmp(identifier, "loglevel") ||
					!strcmp(identifier, "emf") ||
					!strcmp(identifier, "callsite")) {
				if (bt_value_array_append_string(fields,
						identifier)) {
					goto error;
				}
			} else {
				fprintf(stderr, "Unknown field name: \"%s\"\n",
					identifier);
				goto error;
			}
			break;
		}
		case G_TOKEN_COMMA:
			continue;
		case G_TOKEN_EOF:
			goto end;
		default:
			goto error;
		}
	}

	goto end;

error:
	BT_PUT(fields);

end:
	g_scanner_destroy(scanner);

	return fields;
}

struct ctf_legacy_opts {
	int64_t offset_s;
	int64_t offset_ns;
	bool offset_s_is_set;
	bool offset_ns_is_set;
};

struct ctf_text_legacy_opts {
	GString *output;
	struct bt_value *names;
	struct bt_value *fields;
	bool no_delta;
	bool clock_cycles;
	bool clock_seconds;
	bool clock_date;
	bool clock_gmt;
	bool any_is_set;
};

static
struct bt_value *base_params_from_ctf_text_legacy_opts(
		struct ctf_text_legacy_opts *ctf_text_legacy_opts)
{
	struct bt_value *base_params;

	base_params = bt_value_map_create();
	if (!base_params) {
		goto error;
	}

	if (ctf_text_legacy_opts->output->len > 0) {
		if (bt_value_map_insert_string(base_params, "output-path",
				ctf_text_legacy_opts->output->str)) {
			goto error;
		}
	}

	if (ctf_text_legacy_opts->names) {
		if (bt_value_map_insert(base_params, "names",
				ctf_text_legacy_opts->names)) {
			goto error;
		}
	}

	if (ctf_text_legacy_opts->fields) {
		if (bt_value_map_insert(base_params, "fields",
				ctf_text_legacy_opts->fields)) {
			goto error;
		}
	}

	if (bt_value_map_insert_bool(base_params, "no-delta",
			ctf_text_legacy_opts->no_delta)) {
		goto error;
	}

	if (bt_value_map_insert_bool(base_params, "clock-cycles",
			ctf_text_legacy_opts->clock_cycles)) {
		goto error;
	}

	if (bt_value_map_insert_bool(base_params, "clock-seconds",
			ctf_text_legacy_opts->clock_seconds)) {
		goto error;
	}

	if (bt_value_map_insert_bool(base_params, "clock-date",
			ctf_text_legacy_opts->clock_date)) {
		goto error;
	}

	if (bt_value_map_insert_bool(base_params, "clock-gmt",
			ctf_text_legacy_opts->clock_gmt)) {
		goto error;
	}

	goto end;

error:
	BT_PUT(base_params);

end:
	return base_params;
}

static
struct bt_value *base_params_from_ctf_legacy_opts(
		struct ctf_legacy_opts *ctf_legacy_opts)
{
	struct bt_value *base_params;

	base_params = bt_value_map_create();
	if (!base_params) {
		goto error;
	}

	if (bt_value_map_insert_integer(base_params, "offset-s",
			ctf_legacy_opts->offset_s)) {
		goto error;
	}

	if (bt_value_map_insert_integer(base_params, "offset-ns",
			ctf_legacy_opts->offset_s)) {
		goto error;
	}

	goto end;

error:
	BT_PUT(base_params);

end:
	return base_params;
}

static
bool validate_cfg(struct bt_cfg *cfg, struct ctf_legacy_opts *ctf_legacy_opts,
		struct ctf_text_legacy_opts *ctf_text_legacy_opts)
{
	bool legacy_input = false;
	bool legacy_output = false;

	/* Determine if the input and output should be legacy-style */
	if (cfg->legacy_ctf_input || cfg->legacy_lttng_live_input ||
			cfg->sources->len == 0 ||
			!bt_value_array_is_empty(cfg->legacy_input_paths) ||
			ctf_legacy_opts->offset_s_is_set ||
			ctf_legacy_opts->offset_ns_is_set) {
		legacy_input = true;
	}

	if (cfg->legacy_ctf_text_output || cfg->legacy_dummy_output ||
			cfg->legacy_ctf_metadata_output ||
			cfg->sinks->len == 0 ||
			ctf_text_legacy_opts->any_is_set) {
		legacy_output = true;
	}

	if (legacy_input) {
		/* If no legacy input plugin was specified, default to CTF */
		if (!cfg->legacy_lttng_live_input) {
			cfg->legacy_ctf_input = true;
		}

		/* Make sure no non-legacy sources are specified */
		if (cfg->sources->len != 0) {
			fprintf(stderr,
				"Both legacy and non-legacy inputs specified\n");
			goto error;
		}

		/* Make sure zero or one legacy input is specified */
		if (cfg->legacy_ctf_input && cfg->legacy_lttng_live_input) {
			fprintf(stderr,
				"Both \"ctf\" and \"lttng-live\" legacy input plugins specified\n");
			goto error;
		}

		/* Make sure at least one input path exists */
		if (bt_value_array_is_empty(cfg->legacy_input_paths)) {
			fprintf(stderr,
				"No input path/URL specified for legacy input plugin\n");
			goto error;
		}
	}

	if (legacy_output) {
		int total;

		/*
		 * If no legacy output plugin was specified, default to
		 * CTF-text.
		 */
		if (!cfg->legacy_dummy_output &&
				!cfg->legacy_ctf_metadata_output) {
			cfg->legacy_ctf_text_output = true;
		}

		/* Make sure no non-legacy sources are specified */
		if (cfg->sinks->len != 0) {
			fprintf(stderr,
				"Both legacy and non-legacy outputs specified\n");
			goto error;
		}

		/* Make sure zero or one legacy output is specified */
		total = cfg->legacy_dummy_output + cfg->legacy_ctf_text_output +
			cfg->legacy_ctf_metadata_output;
		if (total != 1) {
			fprintf(stderr,
				"More than one legacy output plugin specified\n");
			goto error;
		}

		/*
		 * If any CTF-text option was specified, the output must be
		 * legacy CTF-text.
		 */
		if (ctf_text_legacy_opts->any_is_set &&
				!cfg->legacy_ctf_text_output) {
			fprintf(stderr,
				"Options for \"text\" specified with different legacy output plugin\n");
			goto error;
		}
	}

	/*
	 * If the output is the legacy CTF-metadata plugin, then the
	 * input should be the legacy CTF input plugin.
	 */
	if (cfg->legacy_ctf_metadata_output && !cfg->legacy_ctf_input) {
		fprintf(stderr,
			"\"ctf-metadata\" legacy output plugin requires legacy \"ctf\" input plugin\n");
		goto error;
	}

	return true;

error:
	return false;
}

static
int parse_int64(const char *arg, int64_t *val)
{
	char *endptr;

	errno = 0;
	*val = strtoll(arg, &endptr, 0);
	if (*endptr != '\0' || arg == endptr || errno != 0) {
		return -1;
	}

	return 0;
}

enum {
	OPT_NONE = 0,
	OPT_OUTPUT_PATH,
	OPT_INPUT_FORMAT,
	OPT_OUTPUT_FORMAT,
	OPT_HELP,
	OPT_HELP_LEGACY,
	OPT_VERSION,
	OPT_LIST,
	OPT_VERBOSE,
	OPT_DEBUG,
	OPT_NAMES,
	OPT_FIELDS,
	OPT_NO_DELTA,
	OPT_CLOCK_OFFSET,
	OPT_CLOCK_OFFSET_NS,
	OPT_CLOCK_CYCLES,
	OPT_CLOCK_SECONDS,
	OPT_CLOCK_DATE,
	OPT_CLOCK_GMT,
	OPT_CLOCK_FORCE_CORRELATE,
	OPT_PLUGIN_PATH,
	OPT_SOURCE,
	OPT_SINK,
};

static struct poptOption long_options[] = {
	/* longName, shortName, argInfo, argPtr, value, descrip, argDesc */
	{ "plugin-path", 'p', POPT_ARG_STRING, NULL, OPT_PLUGIN_PATH, NULL, NULL },
	{ "output", 'w', POPT_ARG_STRING, NULL, OPT_OUTPUT_PATH, NULL, NULL },
	{ "input-format", 'i', POPT_ARG_STRING, NULL, OPT_INPUT_FORMAT, NULL, NULL },
	{ "output-format", 'o', POPT_ARG_STRING, NULL, OPT_OUTPUT_FORMAT, NULL, NULL },
	{ "help", 'h', POPT_ARG_NONE, NULL, OPT_HELP, NULL, NULL },
	{ "help-legacy", '\0', POPT_ARG_NONE, NULL, OPT_HELP_LEGACY, NULL, NULL },
	{ "version", 'V', POPT_ARG_NONE, NULL, OPT_VERSION, NULL, NULL },
	{ "list", 'l', POPT_ARG_NONE, NULL, OPT_LIST, NULL, NULL },
	{ "verbose", 'v', POPT_ARG_NONE, NULL, OPT_VERBOSE, NULL, NULL },
	{ "debug", 'd', POPT_ARG_NONE, NULL, OPT_DEBUG, NULL, NULL },
	{ "names", 'n', POPT_ARG_STRING, NULL, OPT_NAMES, NULL, NULL },
	{ "fields", 'f', POPT_ARG_STRING, NULL, OPT_FIELDS, NULL, NULL },
	{ "no-delta", '\0', POPT_ARG_NONE, NULL, OPT_NO_DELTA, NULL, NULL },
	{ "clock-offset", '\0', POPT_ARG_STRING, NULL, OPT_CLOCK_OFFSET, NULL, NULL },
	{ "clock-offset-ns", '\0', POPT_ARG_STRING, NULL, OPT_CLOCK_OFFSET_NS, NULL, NULL },
	{ "clock-cycles", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_CYCLES, NULL, NULL },
	{ "clock-seconds", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_SECONDS, NULL, NULL },
	{ "clock-date", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_DATE, NULL, NULL },
	{ "clock-gmt", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_GMT, NULL, NULL },
	{ "clock-force-correlate", '\0', POPT_ARG_NONE, NULL, OPT_CLOCK_FORCE_CORRELATE, NULL, NULL },
	{ "source", '\0', POPT_ARG_NONE, NULL, OPT_SOURCE, NULL, NULL },
	{ "sink", '\0', POPT_ARG_NONE, NULL, OPT_SINK, NULL, NULL },
	{ NULL, 0, 0, NULL, 0, NULL, NULL },
};

struct bt_cfg *bt_cfg_from_args(int argc, char *argv[], int *exit_code)
{
	struct bt_cfg *cfg = NULL;
	poptContext pc = NULL;
	char *arg = NULL;
	struct ctf_legacy_opts ctf_legacy_opts = { 0 };
	struct ctf_text_legacy_opts ctf_text_legacy_opts = { 0 };
	int opt;

	*exit_code = 0;

	if (argc == 1) {
		show_usage(stdout);
		goto end;
	}

	ctf_text_legacy_opts.output = g_string_new(NULL);
	if (!ctf_text_legacy_opts.output) {
		goto error;
	}

	/* Create config */
	cfg = g_new0(struct bt_cfg, 1);
	if (!cfg) {
		goto error;
	}

	cfg->sources = g_ptr_array_new_with_free_func(bt_cfg_component_destroy);
	if (!cfg->sources) {
		goto error;
	}

	cfg->sinks = g_ptr_array_new_with_free_func(bt_cfg_component_destroy);
	if (!cfg->sinks) {
		goto error;
	}

	cfg->legacy_input_paths = bt_value_array_create();
	if (!cfg->legacy_input_paths) {
		goto error;
	}

	/* Parse options */
	pc = poptGetContext(NULL, argc, (const char **) argv, long_options, 0);
	if (!pc) {
		goto error;
	}

	poptReadDefaultConfig(pc, 0);

	while ((opt = poptGetNextOpt(pc)) > 0) {
		arg = poptGetOptArg(pc);

		switch (opt) {
		case OPT_PLUGIN_PATH:
			if (cfg->plugin_paths) {
				fprintf(stderr, "Duplicate --plugin-path option\n");
				goto error;
			}

			cfg->plugin_paths = plugin_paths_from_arg(arg);
			if (!cfg->plugin_paths) {
				fprintf(stderr,
					"Invalid --plugin-path option's argument\n");
				goto error;
			}
			break;
		case OPT_OUTPUT_PATH:
			if (ctf_text_legacy_opts.output->len > 0) {
				fprintf(stderr, "Duplicate --output option\n");
				goto error;
			}

			g_string_assign(ctf_text_legacy_opts.output, arg);
			ctf_text_legacy_opts.any_is_set = true;
			break;
		case OPT_INPUT_FORMAT:
		case OPT_SOURCE:
		{
			struct bt_cfg_component *bt_cfg_component;

			if (opt == OPT_INPUT_FORMAT) {
				if (!strcmp(arg, "ctf")) {
					/* Legacy CTF input plugin */
					cfg->legacy_ctf_input = true;
					break;
				} else if (!strcmp(arg, "lttng-live")) {
					/* Legacy LTTng-live input plugin */
					cfg->legacy_lttng_live_input = true;
					break;
				}
			}

			/* Non-legacy: try to create a component config */
			bt_cfg_component = bt_cfg_component_from_arg(arg);
			if (!bt_cfg_component) {
				fprintf(stderr,
					"Invalid source component format:\n    %s\n",
					arg);
				goto error;
			}

			g_ptr_array_add(cfg->sources, bt_cfg_component);
			bt_cfg_component = NULL;
			break;
		}
		case OPT_OUTPUT_FORMAT:
		case OPT_SINK:
		{
			struct bt_cfg_component *bt_cfg_component;

			if (opt == OPT_OUTPUT_FORMAT) {
				if (!strcmp(arg, "text")) {
					/* Legacy CTF-text output plugin */
					cfg->legacy_ctf_text_output = true;
					break;
				} else if (!strcmp(arg, "dummy")) {
					/* Legacy dummy output plugin */
					cfg->legacy_dummy_output = true;
					break;
				} else if (!strcmp(arg, "ctf-metadata")) {
					/* Legacy CTF-metadata output plugin */
					cfg->legacy_ctf_metadata_output = true;
					break;
				}
			}

			/* Non-legacy: try to create a component config */
			bt_cfg_component = bt_cfg_component_from_arg(arg);
			if (!bt_cfg_component) {
				fprintf(stderr,
					"Invalid sink component format:\n    %s\n",
					arg);
				goto error;
			}

			g_ptr_array_add(cfg->sinks, bt_cfg_component);
			bt_cfg_component = NULL;
			break;
		}
		case OPT_NAMES:
			if (ctf_text_legacy_opts.names) {
				fprintf(stderr, "Duplicate --names option\n");
				goto error;
			}

			ctf_text_legacy_opts.names = names_from_arg(arg);
			if (!ctf_text_legacy_opts.names) {
				fprintf(stderr,
					"Invalid --names option's argument\n");
				goto error;
			}

			ctf_text_legacy_opts.any_is_set = true;
			break;
		case OPT_FIELDS:
			if (ctf_text_legacy_opts.fields) {
				fprintf(stderr, "Duplicate --fields option\n");
				goto error;
			}

			ctf_text_legacy_opts.fields = fields_from_arg(arg);
			if (!ctf_text_legacy_opts.fields) {
				fprintf(stderr,
					"Invalid --fields option's argument\n");
				goto error;
			}

			ctf_text_legacy_opts.any_is_set = true;
			break;
		case OPT_NO_DELTA:
			ctf_text_legacy_opts.no_delta = true;
			ctf_text_legacy_opts.any_is_set = true;
			break;
		case OPT_CLOCK_CYCLES:
			ctf_text_legacy_opts.clock_cycles = true;
			ctf_text_legacy_opts.any_is_set = true;
			break;
		case OPT_CLOCK_SECONDS:
			ctf_text_legacy_opts.clock_seconds = true;
			ctf_text_legacy_opts.any_is_set = true;
			break;
		case OPT_CLOCK_DATE:
			ctf_text_legacy_opts.clock_date = true;
			ctf_text_legacy_opts.any_is_set = true;
			break;
		case OPT_CLOCK_GMT:
			ctf_text_legacy_opts.clock_gmt = true;
			ctf_text_legacy_opts.any_is_set = true;
			break;
		case OPT_CLOCK_OFFSET:
		{
			int64_t val;

			if (ctf_legacy_opts.offset_s_is_set) {
				fprintf(stderr,
					"Duplicate --clock-offset option\n");
				goto error;
			}

			if (parse_int64(arg, &val)) {
				fprintf(stderr,
					"Invalid --clock-offset option's argument\n");
				goto error;
			}

			ctf_legacy_opts.offset_s = val;
			ctf_legacy_opts.offset_s_is_set = true;
			break;
		}
		case OPT_CLOCK_OFFSET_NS:
		{
			int64_t val;

			if (ctf_legacy_opts.offset_ns_is_set) {
				fprintf(stderr,
					"Duplicate --clock-offset-ns option\n");
				goto error;
			}

			if (parse_int64(arg, &val)) {
				fprintf(stderr,
					"Invalid --clock-offset-ns option's argument\n");
				goto error;
			}

			ctf_legacy_opts.offset_ns = val;
			ctf_legacy_opts.offset_ns_is_set = true;
			break;
		}
		case OPT_CLOCK_FORCE_CORRELATE:
			cfg->force_correlate = true;
			break;
		case OPT_HELP:
			bt_cfg_destroy(cfg);
			cfg = NULL;
			show_usage(stdout);
			goto end;
		case OPT_HELP_LEGACY:
			bt_cfg_destroy(cfg);
			cfg = NULL;
			show_legacy_usage(stdout);
			goto end;
		case OPT_VERSION:
			bt_cfg_destroy(cfg);
			cfg = NULL;
			show_version(stdout);
			goto end;
		case OPT_LIST:
			cfg->do_list = true;
			goto end;
		case OPT_VERBOSE:
			cfg->verbose = true;
			break;
		case OPT_DEBUG:
			cfg->debug = true;
			break;
		default:
			fprintf(stderr, "Unknown option specified\n");
			goto error;
		}

		free(arg);
		arg = NULL;
	}

	/* Check for option parsing error */
	if (opt < -1) {
		fprintf(stderr,
			"Error while parsing command line options: %s\n",
			poptStrerror(opt));
		goto error;
	}

	/* Consume leftover arguments as legacy input paths */
	while (true) {
		const char *input_path = poptGetArg(pc);

		if (!input_path) {
			break;
		}

		if (bt_value_array_append_string(cfg->legacy_input_paths,
				input_path)) {
			goto error;
		}
	}

	/* Validate legacy/non-legacy options */
	if (!validate_cfg(cfg, &ctf_legacy_opts, &ctf_text_legacy_opts)) {
		fprintf(stderr, "Invalid options\n");
		goto error;
	}

	/*
	 * If the input is the legacy input plugin CTF or LTTng-live,
	 * get the base parameters for the eventual components.
	 */
	if (cfg->legacy_ctf_input || cfg->legacy_lttng_live_input) {
		cfg->legacy_input_base_params =
			base_params_from_ctf_legacy_opts(&ctf_legacy_opts);
		if (!cfg->legacy_input_base_params) {
			goto error;
		}
	}

	/*
	 * If the output is the legacy output plugin CTF-text,
	 * get the base parameters for the eventual component.
	 */
	if (cfg->legacy_ctf_text_output) {
		cfg->legacy_output_base_params =
			base_params_from_ctf_text_legacy_opts(
				&ctf_text_legacy_opts);
		if (!cfg->legacy_output_base_params) {
			goto error;
		}
	}

	goto end;

error:
	bt_cfg_destroy(cfg);
	cfg = NULL;
	*exit_code = 1;

end:
	if (pc) {
		poptFreeContext(pc);
	}

	if (ctf_text_legacy_opts.output) {
		g_string_free(ctf_text_legacy_opts.output, TRUE);
	}

	free(arg);
	BT_PUT(ctf_text_legacy_opts.names);
	BT_PUT(ctf_text_legacy_opts.fields);

	return cfg;
}
