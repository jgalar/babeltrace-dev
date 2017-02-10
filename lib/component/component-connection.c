/*
 * component-connection.c
 *
 * Babeltrace Connection
 *
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/component/component-connection-internal.h>
#include <babeltrace/component/component-graph-internal.h>
#include <babeltrace/component/component-port.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler.h>
#include <glib.h>

static
void bt_connection_destroy(struct bt_object *obj)
{
	struct bt_connection *connection = container_of(obj,
			struct bt_connection, base);

	g_ptr_array_free(connection->iterators, TRUE);

	/*
	 * No bt_put on ports as a connection only holds _weak_ references
	 * to them.
	 */
	g_free(connection);
}

BT_HIDDEN
struct bt_connection *bt_connection_create(
		struct bt_graph *graph,
		struct bt_port *upstream, struct bt_port *downstream)
{
	struct bt_connection *connection = NULL;

	if (bt_port_get_type(upstream) != BT_PORT_TYPE_OUTPUT) {
		goto end;
	}
	if (bt_port_get_type(downstream) != BT_PORT_TYPE_INPUT) {
		goto end;
	}

	connection = g_new0(struct bt_connection, 1);
	if (!connection) {
		goto end;
	}

	bt_object_init(connection, bt_connection_destroy);
	/* Weak references are taken, see comment in header. */
	connection->output = upstream;
	connection->input = downstream;
	connection->iterators = g_ptr_array_new_with_free_func(
			(GDestroyNotify) bt_object_release);
	bt_object_set_parent(connection, &graph->base);
end:
	return connection;
}

struct bt_port *bt_connection_get_input_port(
		struct bt_connection *connection)
{
	struct bt_port *port = NULL;

	if (!connection) {
		goto end;
	}

	port = connection->input;
end:
	return port;
}

struct bt_port *bt_connection_get_output_port(
		struct bt_connection *connection)
{
	struct bt_port *port = NULL;

	if (!connection) {
		goto end;
	}

	port = connection->output;
end:
	return port;
}

struct bt_notification_iterator *
bt_connection_create_notification_iterator(struct bt_connection *connection)
{
	return NULL;
}

struct bt_notification_iterator *
bt_connection_get_notification_iterator(struct bt_connection *connection,
		int index)
{
	struct bt_notification_iterator *iterator = NULL;

	if (!connection || index < 0 || index >= connection->iterators->len) {
		goto end;
	}

	iterator = bt_get(g_ptr_array_index(connection->iterators, index));
end:
	return iterator;
}

int bt_connection_get_iterator_count(struct bt_connection *connection)
{
	int ret;

	if (!connection) {
		ret = -1;
		goto end;
	}

	ret = connection->iterators->len;
end:
	return ret;
}
