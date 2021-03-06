/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/plugin/plugin-dev.h>
#include <babeltrace/component/component.h>
#include <babeltrace/component/component-sink.h>
#include <babeltrace/component/notification/iterator.h>
#include <babeltrace/component/notification/notification.h>
#include <plugins-common.h>
#include <assert.h>
#include "dummy.h"

static
void destroy_private_dummy_data(struct dummy *dummy)
{
	if (dummy->iterators) {
		g_ptr_array_free(dummy->iterators, TRUE);
	}
	g_free(dummy);
}

void dummy_destroy(struct bt_component *component)
{
	struct dummy *dummy;

	assert(component);
	dummy = bt_component_get_private_data(component);
	assert(dummy);
	destroy_private_dummy_data(dummy);
}

enum bt_component_status dummy_init(struct bt_component *component,
		struct bt_value *params, UNUSED_VAR void *init_method_data)
{
	enum bt_component_status ret;
	struct dummy *dummy = g_new0(struct dummy, 1);

	if (!dummy) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	dummy->iterators = g_ptr_array_new_with_free_func(
			(GDestroyNotify) bt_put);
	if (!dummy->iterators) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_component_set_private_data(component, dummy);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}
end:
	return ret;
error:
	destroy_private_dummy_data(dummy);
	return ret;
}

enum bt_component_status dummy_new_connection(struct bt_port *own_port,
		struct bt_connection *connection)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_component *component;
	struct dummy *dummy;
	struct bt_notification_iterator *iterator;

	component = bt_port_get_component(own_port);
	assert(component);

	dummy = bt_component_get_private_data(component);
	assert(dummy);

	iterator = bt_connection_create_notification_iterator(connection);
	if (!iterator) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	g_ptr_array_add(dummy->iterators, iterator);
end:
	bt_put(component);
	return ret;
}

enum bt_component_status dummy_consume(struct bt_component *component)
{
	enum bt_component_status ret;
	struct bt_notification *notif = NULL;
	size_t i;
	struct dummy *dummy;

	dummy = bt_component_get_private_data(component);
	assert(dummy);

	/* Consume one notification from each iterator. */
	for (i = 0; i < dummy->iterators->len; i++) {
		struct bt_notification_iterator *it;
		enum bt_notification_iterator_status it_ret;

		it = g_ptr_array_index(dummy->iterators, i);

		it_ret = bt_notification_iterator_next(it);
		switch (it_ret) {
		case BT_NOTIFICATION_ITERATOR_STATUS_ERROR:
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		case BT_NOTIFICATION_ITERATOR_STATUS_END:
			ret = BT_COMPONENT_STATUS_END;
			g_ptr_array_remove_index(dummy->iterators, i);
			i--;
			continue;
		default:
			break;
		}

		notif = bt_notification_iterator_get_notification(it);
		if (!notif) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}

		/*
		 * Dummy! I'm doing nothing with this notification,
		 * NOTHING.
		 */
		BT_PUT(notif);
	}

	if (dummy->iterators->len == 0) {
		ret = BT_COMPONENT_STATUS_END;
	}
end:
	bt_put(notif);
	return ret;
}
