/*
 * component-factory.c
 *
 * Babeltrace Plugin Component Factory
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/plugin/component-factory.h>
#include <babeltrace/plugin/component-factory-internal.h>
#include <babeltrace/plugin/component-class-internal.h>
#include <babeltrace/plugin/source-internal.h>
#include <babeltrace/plugin/sink-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compiler.h>
#include <babeltrace/ref.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <gmodule.h>
#include <stdbool.h>
#include <ftw.h>
#include <pthread.h>
#include <string.h>

#define NATIVE_PLUGIN_SUFFIX ".so"
#define NATIVE_PLUGIN_SUFFIX_LEN sizeof(NATIVE_PLUGIN_SUFFIX)
#define LIBTOOL_PLUGIN_SUFFIX ".la"
#define LIBTOOL_PLUGIN_SUFFIX_LEN sizeof(LIBTOOL_PLUGIN_SUFFIX)
#define PLUGIN_SUFFIX_LEN max_t(size_t, sizeof(NATIVE_PLUGIN_SUFFIX), \
		sizeof(LIBTOOL_PLUGIN_SUFFIX))
#define LOAD_DIR_NFDOPEN_MAX 8

DECLARE_PLUG_IN_SECTIONS;

static struct {
	pthread_mutex_t lock;
	struct bt_component_factory *factory;
	bool recurse;
} load_dir_info = {
	.lock = PTHREAD_MUTEX_INITIALIZER
};

static
enum bt_component_factory_status init_plugin(
		struct bt_component_factory *factory, struct bt_plugin *plugin)
{
	enum bt_component_status component_status;
	enum bt_component_factory_status ret = BT_COMPONENT_FACTORY_STATUS_OK;

	BT_MOVE(factory->current_plugin, plugin);
	component_status = bt_plugin_register_component_classes(
			factory->current_plugin, factory);
	BT_PUT(factory->current_plugin);
	if (component_status != BT_COMPONENT_STATUS_OK) {
		switch (component_status) {
		case BT_COMPONENT_STATUS_NOMEM:
			ret = BT_COMPONENT_FACTORY_STATUS_NOMEM;
			break;
		default:
			ret = BT_COMPONENT_FACTORY_STATUS_ERROR;
			break;
		}
	}
	return ret;
}

static
enum bt_component_factory_status
bt_component_factory_load_file(struct bt_component_factory *factory,
		const char *path)
{
	enum bt_component_factory_status ret = BT_COMPONENT_FACTORY_STATUS_OK;
	size_t path_len;
	GModule *module;
	struct bt_plugin *plugin;
	bool is_libtool_wrapper = false, is_shared_object = false;

	if (!factory || !path) {
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL;
		goto end;
	}

	path_len = strlen(path);
	if (path_len <= PLUGIN_SUFFIX_LEN) {
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL;
		goto end;
	}

	path_len++;
	/*
	 * Check if the file ends with a known plugin file type suffix (i.e. .so
	 * or .la on Linux).
	 */
	is_libtool_wrapper = !strncmp(LIBTOOL_PLUGIN_SUFFIX,
			path + path_len - LIBTOOL_PLUGIN_SUFFIX_LEN,
			LIBTOOL_PLUGIN_SUFFIX_LEN);
	is_shared_object = !strncmp(NATIVE_PLUGIN_SUFFIX,
			path + path_len - NATIVE_PLUGIN_SUFFIX_LEN,
			NATIVE_PLUGIN_SUFFIX_LEN);
	if (!is_shared_object && !is_libtool_wrapper) {
		/* Name indicates that this is not a plugin file. */
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL;
		goto end;
	}

	module = g_module_open(path, 0);
	if (!module) {
		printf_verbose("Module open error: %s\n", g_module_error());
		ret = BT_COMPONENT_FACTORY_STATUS_ERROR;
		goto end;
	}

	/* Load plugin and make sure it defines the required entry points. */
	plugin = bt_plugin_create_from_module(module, path);
	if (!plugin) {
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL_PLUGIN;
		if (!g_module_close(module)) {
			printf_error("Module close error: %s\n",
				g_module_error());
		}
		goto end;
	}
	ret = init_plugin(factory, plugin);
end:
	return ret;
}

static
int nftw_load_dir(const char *file, const struct stat *sb, int flag,
		struct FTW *s)
{
	int ret = 0;
	const char *name = file + s->base;

	/* Check for recursion */
	if (!load_dir_info.recurse && s->level > 1) {
		goto end;
	}

	switch (flag) {
	case FTW_F:
		if (name[0] == '.') {
			/* Skip hidden files */
			goto end;
		}
		bt_component_factory_load_file(load_dir_info.factory, file);
		break;
	case FTW_DNR:
		/* Continue to next file / directory. */
		printf_perror("Failed to read directory: %s\n", file);
		break;
	case FTW_NS:
		/* Continue to next file / directory. */
		printf_perror("Failed to stat() plugin file: %s\n", file);
		break;
	}

end:
	return ret;
}

static
enum bt_component_factory_status
bt_component_factory_load_dir(struct bt_component_factory *factory,
		const char *path, bool recurse)
{
	int nftw_flags = FTW_PHYS;
	size_t path_len = strlen(path);
	enum bt_component_factory_status ret = BT_COMPONENT_FACTORY_STATUS_OK;

	if (path_len >= PATH_MAX) {
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL;
		goto end;
	}

	pthread_mutex_lock(&load_dir_info.lock);

	load_dir_info.factory = factory;
	load_dir_info.recurse = recurse;
	ret = nftw(path, nftw_load_dir, LOAD_DIR_NFDOPEN_MAX, nftw_flags);

	pthread_mutex_unlock(&load_dir_info.lock);

	if (ret != 0) {
		perror("Failed to open plug-in directory");
		ret = BT_COMPONENT_FACTORY_STATUS_ERROR;
	}

end:
	return ret;
}

static
void bt_component_factory_destroy(struct bt_object *obj)
{
	struct bt_component_factory *factory = NULL;

	assert(obj);
	factory = container_of(obj, struct bt_component_factory, base);

	if (factory->component_classes) {
		g_ptr_array_free(factory->component_classes, TRUE);
	}
	g_free(factory);
}

struct bt_component_factory *bt_component_factory_create(void)
{
	struct bt_component_factory *factory;

	factory = g_new0(struct bt_component_factory, 1);
	if (!factory) {
		goto end;
	}

	bt_object_init(factory, bt_component_factory_destroy);
	factory->component_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_put);
	if (!factory->component_classes) {
		goto error;
	}
end:
	return factory;
error:
        BT_PUT(factory);
	return factory;
}

int bt_component_factory_get_component_class_count(
		struct bt_component_factory *factory)
{
	return factory ? factory->component_classes->len : -1;
}

struct bt_component_class *bt_component_factory_get_component_class_index(
		struct bt_component_factory *factory, int index)
{
	struct bt_component_class *component_class = NULL;

	if (!factory || index < 0 || index >= factory->component_classes->len) {
		goto end;
	}

	component_class = bt_get(g_ptr_array_index(
			factory->component_classes, index));
end:
	return component_class;
}

struct bt_component_class *bt_component_factory_get_component_class(
		struct bt_component_factory *factory,
		const char *plugin_name, enum bt_component_type type,
		const char *component_name)
{
	size_t i;
	struct bt_component_class *component_class = NULL;

	if (!factory || (!plugin_name && !component_name &&
			type == BT_COMPONENT_TYPE_UNKNOWN)) {
		/* At least one criterion must be provided. */
		goto no_match;
	}

	for (i = 0; i < factory->component_classes->len; i++) {
		struct bt_plugin *plugin = NULL;

		component_class = g_ptr_array_index(factory->component_classes,
				i);
		plugin = bt_component_class_get_plugin(component_class);
		assert(plugin);

		if (type != BT_COMPONENT_TYPE_UNKNOWN) {
			if (type != bt_component_class_get_type(
					component_class)) {
				bt_put(plugin);
				continue;
			}
		}

		if (plugin_name) {
			const char *cur_plugin_name = bt_plugin_get_name(
					plugin);

			assert(cur_plugin_name);
			if (strcmp(plugin_name, cur_plugin_name)) {
				bt_put(plugin);
				continue;
			}
		}

		if (component_name) {
			const char *cur_cc_name = bt_component_class_get_name(
					component_class);

			assert(cur_cc_name);
			if (strcmp(component_name, cur_cc_name)) {
				bt_put(plugin);
				continue;
			}
		}

		bt_put(plugin);
		/* All criteria met. */
		goto match;
	}

no_match:
	return NULL;
match:
	return bt_get(component_class);
}

static
enum bt_component_factory_status _bt_component_factory_load(
		struct bt_component_factory *factory, const char *path,
		bool recursive)
{
	enum bt_component_factory_status ret = BT_COMPONENT_FACTORY_STATUS_OK;

	if (!factory || !path) {
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL;
		goto end;
	}

	if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
		ret = BT_COMPONENT_FACTORY_STATUS_NOENT;
		goto end;
	}

	if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
		ret = bt_component_factory_load_dir(factory, path, recursive);
	} else if (g_file_test(path, G_FILE_TEST_IS_REGULAR) ||
			g_file_test(path, G_FILE_TEST_IS_SYMLINK)) {
		ret = bt_component_factory_load_file(factory, path);
	} else {
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL;
		goto end;
	}
end:
	return ret;
}

enum bt_component_factory_status bt_component_factory_load_recursive(
		struct bt_component_factory *factory, const char *path)
{
	return _bt_component_factory_load(factory, path, true);
}

enum bt_component_factory_status bt_component_factory_load(
		struct bt_component_factory *factory, const char *path)
{
	return _bt_component_factory_load(factory, path, false);
}

enum bt_component_factory_status bt_component_factory_load_static(
		struct bt_component_factory *factory)
{
	size_t count, i;
	enum bt_component_factory_status ret = BT_COMPONENT_FACTORY_STATUS_OK;

	PRINT_PLUG_IN_SECTIONS(printf_verbose);

	count = SECTION_ELEMENT_COUNT(__plugin_register_funcs);
	if (SECTION_ELEMENT_COUNT(__plugin_register_funcs) != count ||
			SECTION_ELEMENT_COUNT(__plugin_names) != count ||
			SECTION_ELEMENT_COUNT(__plugin_authors) != count ||
			SECTION_ELEMENT_COUNT(__plugin_licenses) != count ||
			SECTION_ELEMENT_COUNT(__plugin_descriptions) != count) {
		printf_error("Some statically-linked plug-ins do not define all mandatory symbols\n");
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL_PLUGIN;
		goto end;
	}
	printf_verbose("Detected %zu statically-linked plug-ins\n", count);

	for (i = 0; i < count; i++) {
		struct bt_plugin *plugin = bt_plugin_create_from_static(i);

		if (!plugin) {
			continue;
		}

		(void) init_plugin(factory, plugin);
	}
end:
	return ret;
}

static
enum bt_component_factory_status
add_component_class(struct bt_component_factory *factory, const char *name,
		    const char *description, bt_component_init_cb init,
		    enum bt_component_type type)
{
	struct bt_component_class *component_class;
	enum bt_component_factory_status ret = BT_COMPONENT_FACTORY_STATUS_OK;

	if (!factory || !name || !init) {
		ret = BT_COMPONENT_FACTORY_STATUS_INVAL;
		goto end;
	}
	assert(factory->current_plugin);

	/*
	 * Ensure this component class does not clash with a currently
	 * registered class.
	 */
	component_class = bt_component_factory_get_component_class(factory,
		bt_plugin_get_name(factory->current_plugin), type, name);
	if (component_class) {
		struct bt_plugin *plugin = bt_component_class_get_plugin(
			component_class);

		printf_verbose("Duplicate component class registration attempted. Component class %s being registered by plugin %s (path: %s) conflicts with one already registered by plugin %s (path: %s)\n",
			name, bt_plugin_get_name(factory->current_plugin),
			bt_plugin_get_path(factory->current_plugin),
			bt_plugin_get_name(plugin),
			bt_plugin_get_path(plugin));
		ret = BT_COMPONENT_FACTORY_STATUS_DUPLICATE;
		BT_PUT(component_class);
		bt_put(plugin);
		goto end;
	}

	component_class = bt_component_class_create(type, name, description,
			init, factory->current_plugin);
	g_ptr_array_add(factory->component_classes, component_class);
end:
	return ret;
}

enum bt_component_factory_status
bt_component_factory_register_source_component_class(
		struct bt_component_factory *factory, const char *name,
		const char *description, bt_component_init_cb init)
{
	return add_component_class(factory, name, description, init,
			BT_COMPONENT_TYPE_SOURCE);
}

enum bt_component_factory_status
bt_component_factory_register_sink_component_class(
		struct bt_component_factory *factory, const char *name,
		const char *description, bt_component_init_cb init)
{
	return add_component_class(factory, name, description, init,
			BT_COMPONENT_TYPE_SINK);
}

enum bt_component_factory_status
bt_component_factory_register_filter_component_class(
		struct bt_component_factory *factory, const char *name,
		const char *description, bt_component_init_cb init)
{
	return add_component_class(factory, name, description, init,
			BT_COMPONENT_TYPE_FILTER);
}
