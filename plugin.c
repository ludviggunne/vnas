#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/limits.h>

#include "api.h"
#include "port.h"
#include "ladspa.h"
#include "log.h"
#include "plugin.h"

struct cache {
  char         *key;
  const void   *ptr;
  struct cache *next;
};

struct plugin {
  node_id_t                node;
  const LADSPA_Descriptor *descriptor;
  LADSPA_Handle            instance;
  LADSPA_Data              cntl_ports[];
};

static const char   *s_plugin_mt = "plugin";
static struct cache *s_get_descriptor_cache = NULL;
static struct cache *s_descriptor_cache = NULL;

static void cache_insert(struct cache **cache, const char *key, const void *ptr)
{
  struct cache *head = calloc(1, sizeof(*head));
  head->key = strdup(key);
  head->ptr = ptr;
  head->next = *cache;
  *cache = head;
}

static const void *cache_lookup(struct cache *cache, const char *key)
{
  while (cache) {
    if (strcmp(cache->key, key) == 0)
      return cache->ptr;
    cache = cache->next;
  }
  return NULL;
}

static char *find_library(const char *path)
{
  struct stat st = {0};

  if (stat(path, &st) == 0) {
    return strdup(path);
  }

  if (strchr(path, '/') != NULL)
    return NULL;

  char *ladspa_path = getenv("LADSPA_PATH");
  if (ladspa_path == NULL)
    return NULL;

  ladspa_path = strdup(ladspa_path);

  char *ptr = ladspa_path, *prefix;
  char buf[PATH_MAX] = {0};

  logdebug("Looking for %s:", path);

  while ((prefix = strtok(ptr, ":")) != NULL) {

    ptr = NULL;
    snprintf(buf, sizeof buf, "%s/%s", prefix, path);

    logdebug("\t%s", buf);

    if (stat(buf, &st) == 0)
      return strdup(buf);
  }

  return NULL;
}

static void parse_plugin_spec(const char *spec, char **file_path, char **plugin)
{
  char buf[4096];
  strcpy(buf, spec);

  *file_path = strtok(buf, ":");
  if (*file_path != NULL) *file_path = strdup(*file_path);

  *plugin = strtok(NULL, ":");
  if (*plugin != NULL) *plugin = strdup(*plugin);
}

static const LADSPA_Descriptor *find_plugin(LADSPA_Descriptor_Function get_descriptor, const char *plugin)
{
  int i = 0;
  const LADSPA_Descriptor *desc;

  logdebug("Looking for %s:", plugin);

  while ((desc = get_descriptor(i++)) != NULL) {
    logdebug("\t%s - %s", desc->Label, desc->Name);
    if (strcmp(desc->Name, plugin) == 0)
      break;
    if (strcmp(desc->Label, plugin) == 0)
      break;
  }

  return desc;
}

static const LADSPA_Descriptor *load_plugin(const char *spec, char *error, size_t error_size)
{
  const LADSPA_Descriptor *descriptor = NULL;
  LADSPA_Descriptor_Function get_descriptor = NULL;
  char *file_path = NULL;
  char *plugin = NULL;
  char *library_path = NULL;
  void *lib_handle = NULL;

  parse_plugin_spec(spec, &file_path, &plugin);
  if (plugin == NULL) {
    snprintf(error, error_size, "invalid plugin specification %s", spec);
    goto done;
  }

  if ((descriptor = cache_lookup(s_descriptor_cache, plugin)) == NULL) {

    if ((get_descriptor = cache_lookup(s_get_descriptor_cache, file_path)) == NULL) {

      library_path = find_library(file_path);
      if (library_path == NULL) {
        snprintf(error, error_size, "could not find %s", file_path);
        goto done;
      }

      lib_handle = dlopen(library_path, RTLD_NOW);
      if (lib_handle == NULL) {
        snprintf(error, error_size, "failed to load %s: %s", file_path, dlerror());
        goto done;
      }

      get_descriptor = dlsym(lib_handle, "ladspa_descriptor");
      if (get_descriptor == NULL) {
        snprintf(error, error_size, "%s does not export ladspa_descriptor", file_path);
        goto done;
      }

      cache_insert(&s_get_descriptor_cache, file_path, get_descriptor);

    } else {
      logdebug("Found cache entry for library %s", file_path);
    }

    descriptor = find_plugin(get_descriptor, plugin);
    if (descriptor == NULL) {
      snprintf(error, error_size, "%s does not export %s", file_path, plugin);
      goto done;
    }

    cache_insert(&s_descriptor_cache, plugin, descriptor);

  } else  {
    logdebug("Found cache entry for plugin %s", plugin);
  }

done:
  free(file_path);
  free(plugin);
  free(library_path);
  if (lib_handle && !descriptor) dlclose(lib_handle);

  return descriptor;
}

static int next_port(const LADSPA_Descriptor *descriptor, int i, int flags)
{
  while (++i < descriptor->PortCount)
    if ((descriptor->PortDescriptors[i] & flags) == flags)
      return i;

  return -1;
}

static int count_ports(const LADSPA_Descriptor *descriptor, int flags)
{
  int i = -1, c = 0;

  while ((i = next_port(descriptor, i, flags)) != -1)
    c++;

  return c;
}

static void instantiate_plugin(struct plugin *plugin)
{
  /* instantiate */
  plugin->instance = plugin->descriptor->instantiate(plugin->descriptor, get_sample_rate());
  if (plugin->descriptor->activate)
    plugin->descriptor->activate(plugin->instance);

  int i, n, flags;
  buffer_id_t buf;
  float *ptr;

  /* connect input ports */
  i = -1, n = 0, flags = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
  while ((i = next_port(plugin->descriptor, i, flags)) != -1) {
    buf = node_get_input(plugin->node, n++);
    ptr = get_buffer(buf);
    plugin->descriptor->connect_port(plugin->instance, i, ptr);
  }

  /* connect output ports */
  i = -1, n = 0, flags = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
  while ((i = next_port(plugin->descriptor, i, flags)) != -1) {
    buf = node_get_output(plugin->node, n++);
    ptr = get_buffer(buf);
    plugin->descriptor->connect_port(plugin->instance, i, ptr);
  }

  /* connect control ports */
  i = -1, flags = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
  while ((i = next_port(plugin->descriptor, i, flags)) != -1)
    plugin->descriptor->connect_port(plugin->instance, i, &plugin->cntl_ports[i]);
}

static void process_fn(void *data, unsigned long n, float **in, int num_in, float **out, int num_out)
{
  struct plugin *plugin = data;

  if (plugin->instance == NULL)
    instantiate_plugin(plugin);

  plugin->descriptor->run(plugin->instance, n);
}

static void init_cntl_ports(struct plugin *plugin)
{
  for (int i = 0; i < plugin->descriptor->PortCount; i++) {
    LADSPA_Data *v = &plugin->cntl_ports[i];

    /* TODO: Hints */

    *v = 0.f;
  }
}

static int api_load_plugin(lua_State *state)
{
  char error[4096];

  const char *spec = luaL_checkstring(state, 1);
  luaL_checktype(state, 2, LUA_TTABLE);

  /* load plugin */
  const LADSPA_Descriptor *descriptor = load_plugin(spec, error, sizeof error);
  if (descriptor == NULL)
    luaL_error(state, error);

  int num_inputs = count_ports(descriptor, LADSPA_PORT_AUDIO|LADSPA_PORT_INPUT);
  int num_outputs = count_ports(descriptor, LADSPA_PORT_AUDIO|LADSPA_PORT_OUTPUT);
  int list_size = lua_rawlen(state, 2);

  if (num_inputs != list_size)
    luaL_error(state, "%s expects %d inputs, but %d were provided",
               spec, num_inputs, list_size);

  /* create api object */
  int size = sizeof(struct plugin) + sizeof(LADSPA_Data) * descriptor->PortCount;
  struct plugin *plugin = lua_newuserdata(state, size);
  luaL_setmetatable(state, s_plugin_mt);
  api_gc_protect(state, -1);

  plugin->descriptor = descriptor;
  plugin->instance = NULL;
  init_cntl_ports(plugin);

  /* create node */
  node_id_t node = new_node();
  node_set_label(node, "plugin");
  node_set_data_ptr(node, plugin);
  node_set_process_fn(node, process_fn);
  plugin->node = node;

  /* connect inputs */
  for (int i = 0; i < num_inputs; i++) {
    lua_rawgeti(state, 2, i+1);
    struct api_node_output *out = api_get_node_output(state, -1);
    int in = node_add_input(node);
    node_connect(out->node, node, out->idx, in);
    lua_pop(state, 1);
  }

  /* add outputs */
  for (int i = 0; i < num_outputs; i++)
    node_add_output(node);

  return 1;
}

static int api_plugin_cntl(lua_State *state)
{
  struct plugin *plugin = luaL_checkudata(state, 1, s_plugin_mt);
  int port = luaL_checkint(state, 2);
  float value = luaL_checknumber(state, 3);

  plugin->cntl_ports[port] = value;

  return 0;
}

static int api_genbinds(lua_State *state)
{
  const char *spec = luaL_checkstring(state, 1);
  char error[4096];
  char *binds = plugin_genbinds(spec, error, sizeof error);

  if (binds == NULL)
    luaL_error(state, "%s", error);

  lua_pushstring(state, binds);
  free(binds);

  return 1;
}

void api_define_plugin(lua_State *state)
{
  const luaL_Reg methods[] = {
    { "cntl", api_plugin_cntl, },
    { NULL,   NULL, },
  };

  const luaL_Reg funcs[] = {
    { "load",     api_load_plugin, },
    { "bindings", api_genbinds, },
    { NULL,       NULL, },
  };

  lua_newtable(state);
  for (const luaL_Reg *reg = funcs; reg->name; reg++) {
    lua_pushcfunction(state, reg->func);
    lua_setfield(state, -2, reg->name);
  }
  lua_setglobal(state, "Plugin");

  luaL_newmetatable(state, s_plugin_mt);
  lua_newtable(state);
  luaL_setfuncs(state, methods, 0);
  api_define_out_method(state, s_plugin_mt);
  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);
}

static void write_buf(char **buf, const char *fmt, ...)
{
  va_list ap_1, ap_2;

  va_start(ap_1, fmt);
  va_copy(ap_2, ap_1);
  size_t len = vsnprintf(NULL, 0, fmt, ap_1);
  va_end(ap_1);

  size_t old_len = *buf ? strlen(*buf) : 0;
  size_t new_len = old_len + len;

  *buf = realloc(*buf, new_len + 1);
  vsnprintf(*buf + old_len, len + 1, fmt, ap_2);
  va_end(ap_2);

  (*buf)[new_len] = '\0';
}

char *plugin_genbinds(const char *spec, char *error, size_t error_size)
{
  char *buf = NULL;
  const LADSPA_Descriptor *descriptor = load_plugin(spec, error, error_size);

  if (descriptor == NULL)
    return NULL;

  int num_outputs = count_ports(descriptor, LADSPA_PORT_AUDIO|LADSPA_PORT_OUTPUT);
  char vars[4096] = {0};

  for (int i = 0; i < num_outputs; i++) {
    int len = strlen(vars);
    int size = sizeof(vars) - len;
    char *ptr = &vars[len];
    snprintf(ptr, size, ", out_%d", i);
  }

  write_buf(&buf, "-- Plugin:         %s\n", descriptor->Name);
  write_buf(&buf, "-- Label:          %s\n", descriptor->Label);
  write_buf(&buf, "-- Maker:          %s\n", descriptor->Maker);
  write_buf(&buf, "-- Copyright:      %s\n", descriptor->Copyright);
  write_buf(&buf, "\n");

  write_buf(&buf, "local M = {}\n");
  write_buf(&buf, "M.__index = M\n");
  write_buf(&buf, "\n");

  write_buf(&buf, "function M.load(inputs)\n");
  write_buf(&buf, "  local plugin%s = Plugin.load('%s', inputs)\n", vars, spec);
  write_buf(&buf, "  return setmetatable({ plugin = plugin }, M)%s\n", vars);
  write_buf(&buf, "end\n");

  write_buf(&buf, "\n");
  write_buf(&buf, "function M:out(index) return self.plugin:out(index) end\n");
  write_buf(&buf, "function M:numout(index) return self.plugin:numout(index) end\n");

  int i = -1;
  while ((i = next_port(descriptor, i, LADSPA_PORT_CONTROL|LADSPA_PORT_INPUT)) != -1) {
    char name[1024];
    strcpy(name, descriptor->PortNames[i]);

    for (char *c = name; *c; c++) {
      *c = tolower(*c);
      if (!isalnum(*c))
        *c = '_';
    }

    write_buf(&buf, "\n");
    write_buf(&buf, "-- %s\n", descriptor->PortNames[i]);
    write_buf(&buf, "function M:%s(value) self.plugin:cntl(%d, value) end\n", name, i);
    write_buf(&buf, "function M.min_%s() return %.2f end\n", name, descriptor->PortRangeHints[i].LowerBound);
    write_buf(&buf, "function M.max_%s() return %.2f end\n", name, descriptor->PortRangeHints[i].UpperBound);
  }

  write_buf(&buf, "\n");
  write_buf(&buf, "return M\n");

  return buf;
}
