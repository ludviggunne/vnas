#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "api.h"
#include "node.h"
#include "log.h"

static const char *const s_filter_mt = "filter";
static const char *const s_script_mt = "expression";

extern int verbose;

enum {
  OP_INPUT = 0,
  OP_CONST,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
};

struct op {
  int   op;
  int   idx;
  int   off;
  float value;
};

struct filter {
  node_id_t   node;
  struct op  *script;
  float      *stack;
  int         len;
  float      *bufs;
  int         num_bufs;
  int         buf_size;
  int         buf_front;
};

static void process_fn(void *data, unsigned long n, float **in, int num_in, float **out, int num_out)
{
  struct filter *f = data;

  for (int i = 0; i < n; i++) {

    for (int j = 0; j < num_in; j++)
      f->bufs[j * f->buf_size + f->buf_front] = in[j][i];

    int sp = 0;
    for (int j = 0; j < f->len; j++) {

      struct op *op = &f->script[j];
      float x;
      int off;

      switch (op->op) {
      case OP_INPUT:
        off = (f->buf_size + f->buf_front - op->off) % f->buf_size;
        f->stack[sp++] = f->bufs[f->buf_size * op->idx + off];
        break;

      case OP_CONST:
        f->stack[sp++] = op->value;
        break;

      case OP_ADD:
        x = f->stack[sp-2] + f->stack[sp-1];
        sp -= 2;
        f->stack[sp++] = x;
        break;

      case OP_SUB:
        x = f->stack[sp-2] - f->stack[sp-1];
        sp -= 2;
        f->stack[sp++] = x;
        break;

      case OP_MUL:
        x = f->stack[sp-2] * f->stack[sp-1];
        sp -= 2;
        f->stack[sp++] = x;
        break;

      case OP_DIV:
        if (fabs(f->stack[sp-1]) < FLT_EPSILON)
          break;
        x = f->stack[sp-2] / f->stack[sp-1];
        sp -= 2;
        f->stack[sp++] = x;
        break;
      }
    }

    out[0][i] = f->stack[0];
    f->buf_front = (f->buf_front + 1) % f->buf_size;
  }
}

static void dump_script(struct filter *f)
{
  logtrace("filter: ");

  for (int i = 0; i < f->len; i++) {
    switch (f->script[i].op) {
    case OP_INPUT:
      logtrace("\tinput %d -%d", f->script[i].idx, f->script[i].off);
      break;
    case OP_CONST:
      logtrace("\tconst %.2f", f->script[i].value);
      break;
    case OP_ADD:
      logtrace("\tadd");
      break;
    case OP_SUB:
      logtrace("\tsub");
      break;
    case OP_MUL:
      logtrace("\tmul");
      break;
    case OP_DIV:
      logtrace("\tdiv");
      break;
    }
  }
}

static int api_create_filter(lua_State *state)
{
  static const int script_idx = 1;
  static const int inputs_idx = 2;

  luaL_checktype(state, script_idx, LUA_TTABLE);
  luaL_checktype(state, inputs_idx, LUA_TTABLE);

  int len = lua_rawlen(state, script_idx);
  int num_inputs = lua_rawlen(state, inputs_idx);

  struct op *script = malloc(sizeof(*script) * len);

  int max_input = -1;
  int max_offset = 0;
  for (int i = 0; i < len; i++) {
    struct op *op = &script[i];

    lua_rawgeti(state, script_idx, i+1);
    luaL_checktype(state, -1, LUA_TTABLE);

    lua_rawgeti(state, -1, 1); /* name */
    lua_rawgeti(state, -2, 2); /* value/index */
    lua_rawgeti(state, -3, 3); /* offset */

    const char *name = luaL_checkstring(state, -3);

    if (strcmp(name, "input") == 0) {
      op->op = OP_INPUT;
      op->idx = luaL_checkinteger(state, -2);
      op->off = luaL_checkinteger(state, -1);

      if (op->idx < 0)
        luaL_error(state, "input %d referenced");

      if (op->off < 0)
        luaL_error(state, "offset %d specified");

      if (op->idx > max_input)
        max_input = op->idx;

      if (op->off > max_offset)
        max_offset = op->off;

    } else if (strcmp(name, "const") == 0) {
      op->op = OP_CONST;
      op->value = luaL_checknumber(state, -2);
    } else if (strcmp(name, "add") == 0) {
      op->op = OP_ADD;
    } else if (strcmp(name, "sub") == 0) {
      op->op = OP_SUB;
    } else if (strcmp(name, "mul") == 0) {
      op->op = OP_MUL;
    } else if (strcmp(name, "div") == 0) {
      op->op = OP_DIV;
    } else {
      luaL_error(state, "invalid op %s", name);
    }

    lua_pop(state, 4);
  }

  if (max_input >= num_inputs)
    luaL_error(state, "number of inputs is %d but input %d is referenced", num_inputs, max_input);

  struct filter *filter = lua_newuserdata(state, sizeof(*filter));
  luaL_setmetatable(state, s_filter_mt);

  filter->script = script;
  filter->stack = malloc(sizeof(*filter->stack) * len);
  filter->len = len;
  filter->node = new_node();
  filter->buf_size = max_offset + 1;
  filter->buf_front = 0;
  filter->num_bufs = num_inputs;
  filter->bufs = calloc(1, sizeof(*filter->bufs) * filter->num_bufs * filter->buf_size);

  node_set_label(filter->node, "filter");
  node_set_data_ptr(filter->node, filter);
  node_set_process_fn(filter->node, process_fn);

  if (verbose)
    dump_script(filter);

  for (int i = 0; i < num_inputs; i++) {
    int idx = node_add_input(filter->node);

    lua_rawgeti(state, inputs_idx, i+1);
    struct api_node_output *out = api_get_node_output(state, -1);

    node_connect(out->node, filter->node, out->idx, idx);
    lua_pop(state, 1);
  }

  node_add_output(filter->node);

  return 1;
}

void api_define_filter(lua_State *state)
{
  const luaL_Reg methods[] = {
    { NULL, NULL, },
  };

  const luaL_Reg filters[] = {
    { "init", api_create_filter, },
    { NULL,     NULL, },
  };

  lua_newtable(state);
  for (const luaL_Reg *reg = filters; reg->name; reg++) {
    lua_pushcfunction(state, reg->func);
    lua_setfield(state, -2, reg->name);
  }
  lua_setglobal(state, "Filter");

  luaL_newmetatable(state, s_filter_mt);
  lua_newtable(state);
  luaL_setfuncs(state, methods, 0);
  api_define_out_method(state, s_filter_mt);
  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);
}
