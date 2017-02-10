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

#include <babeltrace/component/component-graph-internal.h>
#include <babeltrace/component/component-connection-internal.h>
#include <babeltrace/component/component-sink.h>
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
	if (graph->sinks) {
		g_list_free(graph->sinks);
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
end:
	return graph;
error:
	BT_PUT(graph);
	goto end;
}

enum bt_graph_status bt_graph_connect(struct bt_graph *graph,
		struct bt_component *upstream,
		struct bt_component *downstream)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;
	struct bt_connection *connection;
	struct bt_graph *upstream_graph = NULL;
	struct bt_graph *downstream_graph = NULL;

	if (!graph || !upstream || !downstream) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	upstream_graph = bt_component_get_graph(upstream);
	if (upstream_graph && (graph != upstream_graph)) {
		status = BT_GRAPH_STATUS_ALREADY_IN_A_GRAPH;
		goto end;
	}
	downstream_graph = bt_component_get_graph(downstream);
	if (downstream_graph && (graph != downstream_graph)) {
		status = BT_GRAPH_STATUS_ALREADY_IN_A_GRAPH;
		goto end;
	}

	if (bt_component_get_class_type(upstream) ==
			BT_COMPONENT_CLASS_TYPE_SINK) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}
	if (bt_component_get_class_type(downstream) ==
			BT_COMPONENT_CLASS_TYPE_SOURCE) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	connection = bt_connection_create(graph, upstream,
			downstream);
	if (!connection) {
		status = BT_GRAPH_STATUS_MULTIPLE_INPUTS_UNSUPPORTED;
		goto end;
	}

	g_ptr_array_add(graph->connections, connection);
	g_ptr_array_add(graph->components, upstream);
	g_ptr_array_add(graph->components, downstream);
	if (bt_component_get_class_type(downstream) ==
			BT_COMPONENT_CLASS_TYPE_SINK) {
		graph->sinks = g_list_append(graph->sinks, downstream);
	}
end:
	bt_put(upstream_graph);
	bt_put(downstream_graph);
	return status;
}

enum bt_graph_status bt_graph_add_component_as_sibling(struct bt_graph *graph,
		struct bt_component *origin,
		struct bt_component *new_component)
{
	return BT_GRAPH_STATUS_OK;
}

enum bt_component_status bt_graph_consume(struct bt_graph *graph)
{
	GList *current_node = graph->sinks;
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;

	if (!graph) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	if (!graph->sinks) {
		status = BT_GRAPH_STATUS_NO_SINK;
		goto end;
	}

	while (graph->sinks) {
		enum bt_component_status sink_status;
		struct bt_component *sink = current_node->data;

		sink_status = bt_component_sink_consume(sink);
		switch (sink_status) {
		case BT_COMPONENT_STATUS_AGAIN:
			/* Wait for an arbitraty 500 ms. */
			usleep(500000);
			break;
		case BT_COMPONENT_STATUS_END:
		{
			GList *next_node = current_node->next;

			/* Remove sink. */
			graph->sinks = g_list_delete_link(graph->sinks,
					current_node);
			if (next_node) {
				current_node = next_node;
				continue;
			} else {
				current_node = graph->sinks;
			}
		}
		case BT_COMPONENT_STATUS_OK:
			break;
		default:
			fprintf(stderr, "Sink component returned an error...\n");
			status = BT_GRAPH_STATUS_ERROR;
			goto end;
		}

		current_node = current_node->next ?
				current_node->next : graph->sinks;
	}
end:
	return status;
}

enum bt_graph_status bt_graph_run(struct bt_graph *graph)
{
	GList *current_node = graph->sinks;
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;

	if (!graph) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	if (!graph->sinks) {
		status = BT_GRAPH_STATUS_NO_SINK;
		goto end;
	}

	while (graph->sinks) {
		enum bt_component_status sink_status;
		struct bt_component *sink = current_node->data;

		sink_status = bt_component_sink_consume(sink);
		switch (sink_status) {
		case BT_COMPONENT_STATUS_AGAIN:
			/* Wait for an arbitraty 500 ms. */
			usleep(500000);
			break;
		case BT_COMPONENT_STATUS_END:
		{
			GList *next_node = current_node->next;

			/* Remove sink. */
			graph->sinks = g_list_delete_link(graph->sinks,
					current_node);
			if (next_node) {
				current_node = next_node;
				continue;
			} else {
				current_node = graph->sinks;
			}
		}
		case BT_COMPONENT_STATUS_OK:
			break;
		default:
			fprintf(stderr, "Sink component returned an error...\n");
			status = BT_GRAPH_STATUS_ERROR;
			goto end;
		}

		current_node = current_node->next ?
				current_node->next : graph->sinks;
	}
end:
	return status;
}
