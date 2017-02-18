/*
 * component-graph.c
 *
 * Babeltrace Plugin Component Graph
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

#include <babeltrace/component/component-internal.h>
#include <babeltrace/component/component-graph-internal.h>
#include <babeltrace/component/component-connection-internal.h>
#include <babeltrace/component/component-sink-internal.h>
#include <babeltrace/component/component-port.h>
#include <babeltrace/compiler.h>
#include <unistd.h>

static
void bt_graph_destroy(struct bt_object *obj)
{
	struct bt_graph *graph = container_of(obj,
			struct bt_graph, base);

	if (graph->connections) {
		g_ptr_array_free(graph->connections, TRUE);
	}
	if (graph->sinks_to_consume) {
		g_queue_free(graph->sinks_to_consume);
	}
	if (graph->components) {
		g_ptr_array_free(graph->components, TRUE);
	}
	g_free(graph);
}

struct bt_graph *bt_graph_create(void)
{
	struct bt_graph *graph;

	graph = g_new0(struct bt_graph, 1);
	if (!graph) {
		goto end;
	}

	bt_object_init(graph, bt_graph_destroy);

	graph->connections = g_ptr_array_new_with_free_func(bt_object_release);
	if (!graph->connections) {
		goto error;
	}
	graph->components = g_ptr_array_new_with_free_func(bt_object_release);
	if (!graph->components) {
		goto error;
	}
	graph->sinks_to_consume = g_queue_new();
	if (!graph->sinks_to_consume) {
		goto error;
	}
end:
	return graph;
error:
	BT_PUT(graph);
	goto end;
}

struct bt_connection *bt_graph_connect(struct bt_graph *graph,
		struct bt_port *upstream_port,
		struct bt_port *downstream_port)
{
	struct bt_connection *connection = NULL;
	struct bt_graph *upstream_graph = NULL;
	struct bt_graph *downstream_graph = NULL;
	struct bt_component *upstream_component = NULL;
	struct bt_component *downstream_component = NULL;

	if (!graph || !upstream_port || !downstream_port) {
		goto end;
	}

	/* Ensure the components are not already part of another graph. */
	upstream_component = bt_port_get_component(upstream_port);
	assert(upstream_component);
	upstream_graph = bt_component_get_graph(upstream_component);
	if (upstream_graph && (graph != upstream_graph)) {
		fprintf(stderr, "Upstream component is already part of another graph\n");
		goto error;
	}

	downstream_component = bt_port_get_component(downstream_port);
	assert(downstream_component);
	downstream_graph = bt_component_get_graph(downstream_component);
	if (downstream_graph && (graph != downstream_graph)) {
		fprintf(stderr, "Downstream component is already part of another graph\n");
		goto error;
	}

	connection = bt_connection_create(graph, upstream_port,
			downstream_port);
	if (!connection) {
		goto error;
	}

	g_ptr_array_add(graph->connections, connection);
	g_ptr_array_add(graph->components, upstream_component);
	g_ptr_array_add(graph->components, downstream_port);
	if (bt_component_get_class_type(downstream_component) ==
			BT_COMPONENT_CLASS_TYPE_SINK) {
		g_queue_push_tail(graph->sinks_to_consume,
				downstream_component);
	}

	bt_component_set_graph(upstream_component, graph);
	bt_component_set_graph(downstream_component, graph);
end:
	bt_put(upstream_graph);
	bt_put(downstream_graph);
	return connection;
error:
	bt_put(upstream_component);
	bt_put(downstream_component);
	goto end;
}

enum bt_graph_status bt_graph_add_component_as_sibling(struct bt_graph *graph,
		struct bt_component *origin,
		struct bt_component *new_component)
{
	return BT_GRAPH_STATUS_OK;
}

enum bt_component_status bt_graph_consume(struct bt_graph *graph)
{
	struct bt_component *sink;
	enum bt_component_status status;
	GList *current_node;

	if (!graph) {
		status = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (g_queue_is_empty(graph->sinks_to_consume)) {
		status = BT_COMPONENT_STATUS_END;
		goto end;
	}

	current_node = g_queue_pop_head_link(graph->sinks_to_consume);
	sink = current_node->data;
	status = bt_component_sink_consume(sink);
	if (status != BT_COMPONENT_STATUS_END) {
		g_queue_push_tail_link(graph->sinks_to_consume, current_node);
		goto end;
	}

	/* End reached, the node is not added back to the queue and free'd. */
	g_queue_delete_link(graph->sinks_to_consume, current_node);

	/* Don't forward an END status if there are sinks left to consume. */
	if (!g_queue_is_empty(graph->sinks_to_consume)) {
		status = BT_GRAPH_STATUS_OK;
		goto end;
	}
end:
	return status;
}

enum bt_graph_status bt_graph_run(struct bt_graph *graph,
		enum bt_component_status *_component_status)
{
	enum bt_component_status component_status;
	enum bt_graph_status graph_status = BT_GRAPH_STATUS_OK;

	if (!graph) {
		graph_status = BT_GRAPH_STATUS_INVALID;
		goto error;
	}

	do {
		component_status = bt_graph_consume(graph);
		if (component_status == BT_COMPONENT_STATUS_AGAIN) {
			/*
			 * If AGAIN is received and there are multiple sinks,
			 * go ahead and consume from the next sink.
			 *
			 * However, in the case where a single sink is left,
			 * the caller can decide to busy-wait and call
			 * bt_graph_run continuously until the source is ready
			 * or it can decide to sleep for an arbitrary amount of
			 * time.
			 */
			if (graph->sinks_to_consume->length > 1) {
				component_status = BT_COMPONENT_STATUS_OK;
			}
		}
	} while (component_status == BT_COMPONENT_STATUS_OK);

	if (_component_status) {
		*_component_status = component_status;
	}

	if (g_queue_is_empty(graph->sinks_to_consume)) {
		graph_status = BT_GRAPH_STATUS_END;
	} else if (component_status == BT_COMPONENT_STATUS_AGAIN) {
		graph_status = BT_GRAPH_STATUS_AGAIN;
	} else {
		graph_status = BT_GRAPH_STATUS_ERROR;
	}
error:
	return graph_status;
}
