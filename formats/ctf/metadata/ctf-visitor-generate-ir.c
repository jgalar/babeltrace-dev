/*
 * ctf-visitor-generate-ir.c
 *
 * Common Trace Format Metadata Visitor (generate IR).
 *
 * Copyright 2014 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <inttypes.h>
#include <errno.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/list.h>
#include <babeltrace/types.h>
#include <babeltrace/ctf/metadata.h>
#include <babeltrace/compat/uuid.h>
#include <babeltrace/endian.h>
#include <babeltrace/ctf/events-internal.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/clock-internal.h>
#include "ctf-scanner.h"
#include "ctf-parser.h"
#include "ctf-ast.h"

struct bt_trace_tsdl {
	/* root scope */
	struct declaration_scope *root_declaration_scope;
	struct declaration_scope *declaration_scope;
};

#define _bt_list_first_entry(ptr, type, member)	\
	bt_list_entry((ptr)->next, type, member)

/*
 * Returns 0/1 boolean, or < 0 on error.
 */
static
int get_boolean(struct ctf_node *unary_expression)
{
	if (unary_expression->type != NODE_UNARY_EXPRESSION) {
		fprintf(stderr, "[error] %s: expecting unary expression\n",
			__func__);
		return -EINVAL;
	}
	switch (unary_expression->u.unary_expression.type) {
	case UNARY_UNSIGNED_CONSTANT:
		if (unary_expression->u.unary_expression.u.unsigned_constant == 0) {
			return 0;
		} else {
			return 1;
		}
	case UNARY_SIGNED_CONSTANT:
		if (unary_expression->u.unary_expression.u.signed_constant == 0) {
			return 0;
		} else {
			return 1;
		}
	case UNARY_STRING:
		if (!strcmp(unary_expression->u.unary_expression.u.string, "true")) {
			return 1;
		} else if (!strcmp(unary_expression->u.unary_expression.u.string, "TRUE")) {
			return 1;
		} else if (!strcmp(unary_expression->u.unary_expression.u.string, "false")) {
			return 0;
		} else if (!strcmp(unary_expression->u.unary_expression.u.string, "FALSE")) {
			return 0;
		} else {
			fprintf(stderr,
				"[error] %s: unexpected string \"%s\"\n",
				__func__,
				unary_expression->u.unary_expression.u.string);
			return -EINVAL;
		}
		break;
	default:
		fprintf(stderr,
			"[error] %s: unexpected unary expression type\n",
			__func__);
		return -EINVAL;
	}
}

static
int get_unary_unsigned(struct bt_list_head *head, uint64_t *value)
{
	struct ctf_node *node;
	int i = 0, ret = 0;

	bt_list_for_each_entry(node, head, siblings) {
		if (node->type != NODE_UNARY_EXPRESSION
			|| node->u.unary_expression.type !=
				UNARY_UNSIGNED_CONSTANT
			|| node->u.unary_expression.link != UNARY_LINK_UNKNOWN
			|| i != 0) {
			ret = -EINVAL;
			goto end;
		}

		*value = node->u.unary_expression.u.unsigned_constant;
		i++;
	}

end:
	return ret;
}

/*
 * String returned must be freed by the caller using g_free.
 */
static
char *concatenate_unary_strings(struct bt_list_head *head)
{
	struct ctf_node *node;
	GString *str;
	int i = 0;

	str = g_string_new("");
	bt_list_for_each_entry(node, head, siblings) {
		char *src_string;

		if (node->type != NODE_UNARY_EXPRESSION
			|| node->u.unary_expression.type != UNARY_STRING
			|| !((node->u.unary_expression.link !=
				UNARY_LINK_UNKNOWN) ^ (i == 0))) {
			return NULL;
		}

		switch (node->u.unary_expression.link) {
		case UNARY_DOTLINK:
			g_string_append(str, ".");
			break;
		case UNARY_ARROWLINK:
			g_string_append(str, "->");
			break;
		case UNARY_DOTDOTDOT:
			g_string_append(str, "...");
			break;
		default:
			break;
		}

		src_string = node->u.unary_expression.u.string;
		g_string_append(str, src_string);
		i++;
	}
	return g_string_free(str, FALSE);
}

static
int ctf_clock_declaration_visit(struct ctf_node *node,
		struct bt_ctf_clock *clock, struct bt_ctf_trace *trace)
{
	int ret = 0;

	switch (node->type) {
	case NODE_CTF_EXPRESSION:
	{
		char *left;

		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!left) {
			ret = -EINVAL;
			goto error;
		}
		if (!strcmp(left, "name")) {
			char *right;

			if (bt_ctf_clock_get_name(clock)) {
				fprintf(stderr, "[error] %s: name already declared in clock declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			right = concatenate_unary_strings(
				&node->u.ctf_expression.right);
			if (!right) {
				fprintf(stderr, "[error] %s: unexpected unary expression for clock name\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			g_free(right);
			ret = bt_ctf_clock_set_name(clock, right);
			if (ret) {
				fprintf(stderr, "[error] %s: invalid clock name \"%s\"\n", __func__, right);
				ret = -EINVAL;
				goto error;
			}
		} else if (!strcmp(left, "uuid")) {
			char *right;
			unsigned char uuid[BABELTRACE_UUID_LEN];

			if (!bt_ctf_clock_get_uuid(clock)) {
				fprintf(stderr, "[error] %s: uuid already declared in clock declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			right = concatenate_unary_strings(
				&node->u.ctf_expression.right);
			if (!right) {
				fprintf(stderr, "[error] %s: unexpected unary expression for clock uuid\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			if (uuid_parse(right, uuid)) {
				fprintf(stderr, "[error] %s: failed to parse uuid\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			ret = bt_ctf_clock_set_uuid(clock, uuid);
			if (ret) {
				fprintf(stderr, "[error] %s: failed to set clock uuid\n", __func__);
				goto error;
			}
			g_free(right);
		} else if (!strcmp(left, "description")) {
			char *right;

			if (bt_ctf_clock_get_description(clock)) {
				fprintf(stderr, "[warning] %s: duplicated clock description\n", __func__);
				/* ret is 0; not an actual error, just warn. */
				goto error;
			}
			right = concatenate_unary_strings(
				&node->u.ctf_expression.right);
			if (!right) {
				fprintf(stderr, "[warning] %s: unexpected unary expression for clock description\n", __func__);
				/* ret is 0; not an actual error, just warn. */
				goto error;
			}
			ret = bt_ctf_clock_set_description(clock, right);
			if (ret) {
				fprintf(stderr, "[error] %s: could not set clock description", __func__);
				goto error;
			}
		} else if (!strcmp(left, "freq")) {
			uint64_t freq;

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				&freq);
			if (ret) {
				fprintf(stderr, "[error] %s: unexpected unary expression for clock freq\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			ret = bt_ctf_clock_set_frequency(clock, freq);
			if (ret) {
				fprintf(stderr, "[error] %s: could not set clock frequency", __func__);
				goto error;
			}
		} else if (!strcmp(left, "precision")) {
			uint64_t precision;

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				&precision);
			if (ret) {
				fprintf(stderr, "[error] %s: unexpected unary expression for clock precision\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			ret = bt_ctf_clock_set_precision(clock, precision);
			if (ret) {
				fprintf(stderr, "[error] %s: could not set clock precision", __func__);
				goto error;
			}
		} else if (!strcmp(left, "offset_s")) {
			uint64_t offset_s;

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				&offset_s);
			if (ret) {
				fprintf(stderr, "[error] %s: unexpected unary expression for clock offset_s\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			ret = bt_ctf_clock_set_offset_s(clock, offset_s);
			if (ret) {
				fprintf(stderr, "[error] %s: could not set clock offset_s", __func__);
				goto error;
			}
		} else if (!strcmp(left, "offset")) {
			uint64_t offset;

			ret = get_unary_unsigned(&node->u.ctf_expression.right,
				&offset);
			if (ret) {
				fprintf(stderr, "[error] %s: unexpected unary expression for clock offset\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			ret = bt_ctf_clock_set_offset(clock, offset);
			if (ret) {
				fprintf(stderr, "[error] %s: could not set clock offset", __func__);
				goto error;
			}
		} else if (!strcmp(left, "absolute")) {
			struct ctf_node *right;

			right = _bt_list_first_entry(
				&node->u.ctf_expression.right, struct ctf_node,
				siblings);
			ret = get_boolean(right);
			if (ret < 0) {
				fprintf(stderr, "[error] %s: unexpected \"absolute\" right member\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			ret = bt_ctf_clock_set_is_absolute(clock, ret);
			if (ret) {
				fprintf(stderr, "[error] %s: could not set clock absolute attribute", __func__);
				goto error;
			}
		} else {
			fprintf(stderr, "[warning] %s: attribute \"%s\" is unknown in clock declaration.\n", __func__, left);
		}

error:
		g_free(left);
		break;
	}
	default:
		ret = -EPERM;
	/* TODO: declaration specifier should be added. */
	}

	return ret;
}

static
int ctf_clock_visit(struct ctf_node *node, struct bt_ctf_trace *trace)
{
	int ret = 0;
	struct ctf_node *iter;
	struct bt_ctf_clock *clock;

	if (node->visited) {
		goto end;
	}
	node->visited = 1;

	/* Use a placeholder name until we get the real name */
	clock = bt_ctf_clock_create("unknown");
	if (!clock) {
		ret = -ENOMEM;
		goto error;
	}

	bt_list_for_each_entry(iter, &node->u.clock.declaration_list,
		siblings) {
		/*
		 * Iterate over every attribute associated with the current
		 * clock.
		 */
		ret = ctf_clock_declaration_visit(iter, clock, trace);
		if (ret)
			goto error;
	}
	if (opt_clock_force_correlate) {
		/*
		 * User requested to forcibly correlate the clock
		 * sources, even if we have no correlation
		 * information.
		 */
		ret = bt_ctf_clock_get_is_absolute(clock);
		if (ret < 0) {
			fprintf(stderr, "[error] Failed to get clock absolute attribute.\n");
			goto error;
		}

		if (!ret) {
			fprintf(stderr, "[warning] Forcibly correlating trace clock sources (--clock-force-correlate).\n");
		}

		ret = bt_ctf_clock_set_is_absolute(clock, 1);
		if (ret) {
			fprintf(stderr, "[error] Failed to set clock absolute attribute.\n");
			goto error;
		}
	}
	if (!bt_ctf_clock_get_name(clock)) {
		ret = -EPERM;
		fprintf(stderr, "[error] %s: missing name field in clock declaration\n", __func__);
		goto error;
	}
	if (bt_ctf_trace_get_clock_count(trace) > 0) {
		fprintf(stderr, "[error] Only CTF traces with a single clock description are supported by this babeltrace version.\n");
		ret = -EINVAL;
		goto error;
	}

	ret = bt_ctf_trace_add_clock(trace, clock);
	if (ret) {
		fprintf(stderr, "[error] Failed to add clock to trace.\n");
		goto error;
	}

error:
	bt_ctf_clock_put(clock);
end:
	return ret;
}

static
int ctf_typedef_visit(struct bt_ctf_trace *trace,
		struct declaration_scope *scope,
		struct ctf_node *type_specifier_list,
		struct bt_list_head *type_declarators)
{
	struct ctf_node *iter;
	GQuark identifier;
	int ret = 0;

	bt_list_for_each_entry(iter, type_declarators, siblings) {
		struct bt_declaration *type_declaration;
		int ret;

		type_declaration = ctf_type_declarator_visit(
					type_specifier_list,
					&identifier, iter,
					scope, NULL, trace);
		if (!type_declaration) {
			fprintf(stderr, "[error] %s: problem creating type declaration\n", __func__);
			ret = -EINVAL;
			goto end;
		}
		/*
		 * Don't allow typedef and typealias of untagged
		 * variants.
		 */
		if (type_declaration->id == CTF_TYPE_UNTAGGED_VARIANT) {
			fprintf(stderr, "[error] %s: typedef of untagged variant is not permitted.\n", __func__);
			bt_declaration_unref(type_declaration);
			ret = -EPERM;
			goto end;
		}

		ret = bt_register_declaration(identifier, type_declaration, scope);
		if (ret) {
			type_declaration->declaration_free(type_declaration);
			goto end;
		}
		bt_declaration_unref(type_declaration);
	}

end:
	return ret;
}

static
int ctf_root_declaration_visit(struct ctf_node *node,
		struct bt_ctf_trace *trace,
		struct bt_trace_tsdl *trace_tsdl)
{
	int ret = 0;

	if (node->visited) {
		return 0;
	}
	node->visited = 1;

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = ctf_typedef_visit(trace,
			trace_tsdl->root_declaration_scope,
			node->u._typedef.type_specifier_list,
			&node->u._typedef.type_declarators);
		if (ret) {
			goto end;
		}
		break;
	case NODE_TYPEALIAS:
/*
		ret = ctf_typealias_visit(trace,
			trace->root_declaration_scope,
			node->u.typealias.target,
			node->u.typealias.alias);
		if (ret) {
			goto end;
		}
		break;
*/
	case NODE_TYPE_SPECIFIER_LIST:
	{
		struct bt_declaration *declaration;

		/*
		 * Just add the type specifier to the root scope
		 * declaration scope. Release local reference.
		 */
/*
		declaration = ctf_type_specifier_list_visit(trace,
			node, trace_tsdl->root_declaration_scope);
		if (!declaration) {
			ret = -ENOMEM;
			goto end;
		}
		bt_declaration_unref(declaration);
		break;
*/
	}
	default:
		ret = -EPERM;
		goto end;
	}
end:
	return ret;
}

static
void destroy_trace_tsdl(struct bt_trace_tsdl *trace_tsdl)
{
	if (!trace_tsdl) {
		return;
	}

	if (trace_tsdl->root_declaration_scope) {
		bt_free_declaration_scope(trace_tsdl->root_declaration_scope);
	}
	if (trace_tsdl->declaration_scope) {
		bt_free_declaration_scope(trace_tsdl->declaration_scope);
	}
}

static
struct bt_trace_tsdl *create_trace_tsdl(void)
{
	struct bt_trace_tsdl *ret;

	ret = malloc(sizeof(struct bt_trace_tsdl));
	if (!ret) {
		goto end;
	}

	ret->root_declaration_scope = bt_new_declaration_scope(NULL);
	ret->declaration_scope = bt_new_declaration_scope(NULL);
	if (ret->root_declaration_scope || !ret->declaration_scope) {
		goto error;
	}
end:
	return ret;
error:
	destroy_trace_tsdl(ret);
	return ret;
}

int ctf_visitor_construct_metadata_ir(struct ctf_node *node,
		struct bt_ctf_trace *trace)
{
	int ret = 0;
	struct ctf_node *iter;
	struct bt_trace_tsdl *trace_tsdl = NULL;

	if (!trace) {
		ret = -1;
		goto end;
	}

	trace_tsdl = create_trace_tsdl();
	if (!trace_tsdl) {
		ret = -ENOMEM;
		goto end;
	}

	printf_verbose("CTF visitor: metadata construction...\n");
retry:
	switch (node->type) {
	case NODE_ROOT:
		/*
		 * declarations need to query the clock hash table, so clock
		 * needs to be treated first.
		 */
		if (bt_list_empty(&node->u.root.clock)) {
			struct bt_ctf_clock *default_clock =
				bt_ctf_clock_create("monotonic");

			if (!default_clock) {
				ret = -ENOMEM;
				goto end;
			}
			ret = bt_ctf_trace_add_clock(trace, default_clock);
			if (ret) {
				goto end;
			}
		} else {
			bt_list_for_each_entry(iter, &node->u.root.clock,
				siblings) {
				ret = ctf_clock_visit(iter, trace);
				if (ret) {
					fprintf(stderr, "[error] %s: clock declaration error\n", __func__);
					goto end;
				}
			}
		}
		bt_list_for_each_entry(iter, &node->u.root.declaration_list,
			siblings) {
			ret = ctf_root_declaration_visit(iter, trace, trace_tsdl);
			if (ret) {
				fprintf(stderr, "[error] %s: root declaration error\n", __func__);
				goto end;
			}
		}
	default:
		fprintf(stderr, "[error] %s: unknown node type %d\n", __func__,
			(int) node->type);
		ret = -EINVAL;
		goto end;
	}
	printf_verbose("done.\n");
end:
	destroy_trace_tsdl(trace_tsdl);
	return ret;
}
