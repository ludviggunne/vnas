#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <jack/jack.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "io.h"
#include "api.h"
#include "port.h"
#include "event.h"
#include "log.h"

jack_client_t *client;
lua_State *lua_state;
int verbose = 0;
int is_init = 0;

static const char *program;

static const char *const setup =
  "package.path = '" PREFIX "/share/vnas/\?.lua;' .. package.path\n"
  "require 'lib.extra'\n";

static void usage(FILE *f)
{
  fprintf(f, "Usage: %s [OPTION...] [FILE]\n", program);
  fprintf(f, "    -v[v]                Print more info\n");
  fprintf(f, "    -b PATH:PLUGIN       Generate Lua bindings for a LADSPA plugin\n");
  fprintf(f, "    -g FILE              Generate a Graphviz file depicting internal audio graph\n");
  fprintf(f, "    -x SCRIPT            Run SCRIPT instead of FILE\n");
  fprintf(f, "    -c NAME              Set Jack client name\n");
  fprintf(f, "    -a                   List audio sources\n");
  fprintf(f, "    -A                   List audio destinations\n");
  fprintf(f, "    -m                   List MIDI sources\n");
  fprintf(f, "    -M                   List MIDI destinations\n");
}

static struct termios old_termios = {0};

static void reset_term(void)
{
  tcsetattr(STDOUT_FILENO, TCSANOW, &old_termios);
}

static void setup_term(void)
{
  struct termios new_termios;
  tcgetattr(STDOUT_FILENO, &old_termios);
  memcpy(&new_termios, &old_termios, sizeof new_termios);

  new_termios.c_lflag &= ~ICANON;
  new_termios.c_lflag &= ~ECHO;

  tcsetattr(STDOUT_FILENO, TCSANOW, &new_termios);
  atexit(reset_term);
}

int main(int argc, char **argv)
{
  srand(time(NULL));
  program = argv[0];
  const char *client_name = "vnas";
  const char *graph_path = NULL;
  const char *script_arg = NULL;
  const char *binds_spec = NULL;

  int c;
  while ((c = getopt(argc, argv, "b:g:vc:x:aAmM")) != -1) {
    switch (c) {
    case 'b':
      binds_spec = optarg;
      break;
    case 'g':
      graph_path = optarg;
      break;
    case 'v':
      verbose++;
      break;
    case 'c':
      client_name = optarg;
      break;
    case 'x':
      script_arg = optarg;
      break;
    case 'a':
      script_arg = "for _, e in ipairs(Port.Audio.sources()) do print(e) end exit()";
      break;
    case 'A':
      script_arg = "for _, e in ipairs(Port.Audio.destinations()) do print(e) end exit()";
      break;
    case 'm':
      script_arg = "for _, e in ipairs(Port.Midi.sources()) do print(e) end exit()";
      break;
    case 'M':
      script_arg = "for _, e in ipairs(Port.Midi.destinations()) do print(e) end exit()";
      break;
    default:
      usage(stderr);
      exit(EXIT_FAILURE);
    }
  }

  init_logging(verbose);

  char script_buf[4096];
  if (binds_spec) {
    snprintf(script_buf, sizeof script_buf,
             "print(Plugin.bindings('%s')); exit()",
             binds_spec);
    script_arg = script_buf;
  }

  lua_state = luaL_newstate();
  luaL_openlibs(lua_state);

  api_init(lua_state);
  node_pool_init();

  if (!script_arg && optind >= argc) {
    fprintf(stderr, "%s: missing file argument\n", program);
    usage(stderr);
    exit(EXIT_FAILURE);
  }

  client = jack_client_open(client_name, 0, NULL);

  if (client == NULL) {
    fprintf(stderr, "%s: failed to open jack client '%s'\n",
            program, client_name);
    exit(EXIT_FAILURE);
  }

  init_events();

  if (luaL_dostring(lua_state, setup) != LUA_OK) {
    const char *msg = luaL_checkstring(lua_state, -1);
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
  }

  int res;
  if (script_arg) {
    res = luaL_dostring(lua_state, script_arg);
  } else {
    const char *path = argv[optind];
    res = luaL_dofile(lua_state, path);
  }

  is_init = 1;

  if (res != LUA_OK) {
    const char *msg = luaL_checkstring(lua_state, -1);
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
  }

  if (graph_path) {
    export_graph(graph_path);
    exit(EXIT_SUCCESS);
  }

  if (api_did_exit())
    exit(EXIT_SUCCESS);

  topo_sort_nodes();
  set_jack_callbacks();

  jack_activate(client);
  setup_term();
  printf("Press C-c to quit\n");
  io_loop();
}
