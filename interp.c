#include "port.h"
#include "api.h"
#include "node.h"
#include "log.h"

static const char *s_interp_mt = "interp";

struct interp {
  node_id_t     node;
  float         src;
  float         dst;
  float         curr;
  float         t0f;
  int           conv;
  unsigned long t;
  unsigned long t0;
};

static void process_fn(void *data, unsigned long n, float **in, int num_in, float **out, int num_out)
{
  struct interp *interp = data;

  if (interp->conv) {
    interp->t0 = interp->t0f * get_sample_rate();
    interp->conv = 0;
  }

  for (int i = 0; i < n; i++) {
    if (interp->t0 != 0) {
      if (interp->t == interp->t0) {
        interp->t = 0;
        interp->t0 = 0;
        interp->src = interp->dst;

        logtrace("interp\tend\t%.2f", interp->src);
      } else {
        interp->t++;
      }
    }

    float x = interp->t0 == 0 ? 0 : interp->t / (float) interp->t0;
    interp->curr = interp->src + (interp->dst - interp->src) * x;
    out[0][i] = interp->curr;
  }
}

static int api_create_interp(lua_State *state)
{
  float init = luaL_checknumber(state, 1);

  struct interp *interp = lua_newuserdata(state, sizeof(*interp));
  luaL_setmetatable(state, s_interp_mt);
  api_gc_protect(state, -1);

  interp->src = init;
  interp->dst = init;
  interp->curr = init;
  interp->t = 0;
  interp->t0 = 0;
  interp->node = new_node();
  interp->conv = 0;

  node_set_data_ptr(interp->node, interp);
  node_set_process_fn(interp->node, process_fn);
  node_add_output(interp->node);
  node_set_cntl(interp->node);
  node_set_label(interp->node, "interp");

  return 1;
}

static int api_target(lua_State *state)
{
  struct interp *interp = luaL_checkudata(state, 1, s_interp_mt);

  interp->dst = luaL_checknumber(state, 2);
  interp->t0f = luaL_checknumber(state, 3);
  interp->conv = 1;
  interp->src = interp->curr;
  interp->t = 0;

  logtrace("interp\tstart\t%.2f\t%.2f\t%.2f",
       interp->src, interp->dst, interp->t0f);

  return 0;
}

void api_define_interp(lua_State *state)
{
  const luaL_Reg methods[] = {
    { "target", api_target, },
    { NULL,   NULL, },
  };

  const luaL_Reg funcs[] = {
    { "interp", api_create_interp, },
    { NULL,    NULL, },
  };

  for (const luaL_Reg *reg = funcs; reg->name; reg++) {
    lua_pushcfunction(state, reg->func);
    lua_setglobal(state, reg->name);
  }

  luaL_newmetatable(state, s_interp_mt);
  lua_newtable(state);
  luaL_setfuncs(state, methods, 0);
  api_define_out_method(state, s_interp_mt);
  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);
}
