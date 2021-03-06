#ifndef BABELTRACE_COMPONENT_PORT_H
#define BABELTRACE_COMPONENT_PORT_H

/*
 * BabelTrace - Babeltrace Component Connection Interface
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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_port;
struct bt_connection;

enum bt_port_status {
	BT_PORT_STATUS_OK = 0,
	BT_PORT_STATUS_ERROR = -1,
	BT_PORT_STATUS_INVALID = -2,
};

enum bt_port_type {
	BT_PORT_TYPE_INPUT = 0,
	BT_PORT_TYPE_OUTPUT = 1,
	BT_PORT_TYPE_UNKOWN = -1,
};

extern const char *BT_DEFAULT_PORT_NAME;

extern const char *bt_port_get_name(struct bt_port *port);
extern enum bt_port_type bt_port_get_type(struct bt_port *port);

extern enum bt_port_status bt_port_get_connection_count(struct bt_port *port,
		uint64_t *count);
extern struct bt_connection *bt_port_get_connection(struct bt_port *port,
		int index);

extern struct bt_component *bt_port_get_component(struct bt_port *port);

/* Only valid before a connection is established. */
extern enum bt_port_status bt_port_get_maximum_connection_count(
		struct bt_port *port, uint64_t *count);
extern enum bt_port_status bt_port_set_maximum_connection_count(
		struct bt_port *port, uint64_t count);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_COMPONENT_PORT_H */
