#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "node.h"

struct conn {
  node_id_t node;
  int       idx;
};

struct node {
  char              *label;

  void              *data_ptr;
  node_process_fn_t  process_fn;

  int                num_inputs;
  int                num_outputs;
  struct conn        inputs[NODE_MAX_CONNECTIONS];
  buffer_id_t        outputs[NODE_MAX_CONNECTIONS];

  int                visited;
  int                cntl;
};

static struct node  *s_nodes = NULL;
static int           s_num_nodes = 0;

static node_id_t     s_null_node;
static node_id_t     s_root_node;

static node_id_t    *s_sorted;
static int           s_num_sorted;

static float        *s_buffers;
static int           s_num_buffers;
static unsigned long s_buffer_size;

static void resize_impl(void)
{
  unsigned long size = s_buffer_size * s_num_buffers * sizeof(*s_buffers);
  s_buffers = realloc(s_buffers, size);
}

static buffer_id_t alloc_buffer(void)
{
  buffer_id_t buf = s_num_buffers++;
  resize_impl();
  memset(&s_buffers[buf], 0, sizeof(s_buffers[buf]) * s_buffer_size);
  return buf;
}

float *get_buffer(buffer_id_t buf)
{
  return &s_buffers[buf * s_buffer_size];
}

static void null_process_fn(void *data, unsigned long n, float **in, int num_in, float **out, int num_out)
{
  memset(out[0], 0, sizeof(*out[0]) * n);
}

void resize_buffers(unsigned long size)
{
  if (size <= s_buffer_size)
    return;

  s_buffer_size = size;
  resize_impl();
}

void node_pool_init(void)
{
  s_root_node = new_node();
  s_null_node = new_node();

  node_set_label(s_root_node, "root");
  node_set_label(s_null_node, "null");

  (void) node_add_output(s_null_node);
  node_set_process_fn(s_null_node, null_process_fn);
}

node_id_t get_root_node(void)
{
  return s_root_node;
}

node_id_t new_node(void)
{
  node_id_t node = s_num_nodes++;
  s_nodes = realloc(s_nodes, sizeof(*s_nodes) * s_num_nodes);
  s_sorted = realloc(s_sorted, sizeof(*s_sorted) * s_num_nodes);

  struct node *ptr = &s_nodes[node];
  ptr->label = NULL;
  ptr->data_ptr = NULL;
  ptr->process_fn = NULL;
  ptr->num_inputs = 0;
  ptr->num_outputs = 0;
  ptr->visited = 0;
  ptr->cntl = 0;

  return node;
}

int node_add_input(node_id_t node)
{
  struct node *ptr = &s_nodes[node];

  int idx = ptr->num_inputs++;

  ptr->inputs[idx].node = s_null_node;
  ptr->inputs[idx].idx = 0;

  return idx;
}

int node_add_output(node_id_t node)
{
  struct node *ptr = &s_nodes[node];
  int idx = ptr->num_outputs++;
  ptr->outputs[idx] = alloc_buffer();
  return idx;
}

buffer_id_t node_get_output(node_id_t node, int idx)
{
  return s_nodes[node].outputs[idx];
}

buffer_id_t node_get_input(node_id_t node, int idx)
{
  struct conn *input = &s_nodes[node].inputs[idx];
  return node_get_output(input->node, input->idx);
}

int node_get_num_outputs(node_id_t node)
{
  return s_nodes[node].num_outputs;
}

void node_connect(node_id_t src, node_id_t dst, int src_idx, int dst_idx)
{
  struct conn *conn = &s_nodes[dst].inputs[dst_idx];
  conn->node = src;
  conn->idx = src_idx;
}

void node_set_data_ptr(node_id_t node, void *ptr)
{
  s_nodes[node].data_ptr = ptr;
}

void node_set_process_fn(node_id_t node, node_process_fn_t fn)
{
  s_nodes[node].process_fn = fn;
}

void node_set_cntl(node_id_t node)
{
  s_nodes[node].cntl = 1;
}

int node_is_cntl(node_id_t node)
{
  return s_nodes[node].cntl;
}


static void topo_sort(node_id_t node)
{
  struct node *ptr = &s_nodes[node];

  if (ptr->visited)
    return;

  for (int i = 0; i < ptr->num_inputs; i++) {
    node_id_t input = ptr->inputs[i].node;
    topo_sort(input);
  }

  s_sorted[s_num_sorted++] = node;

  ptr->visited = 1;
}

void topo_sort_nodes(void)
{
  topo_sort(s_root_node);
}

static void process_node(node_id_t node, unsigned long n)
{
  float *in[NODE_MAX_CONNECTIONS];
  float *out[NODE_MAX_CONNECTIONS];

  struct node *ptr = &s_nodes[node];

  if (!ptr->process_fn)
    return;

  for (int i = 0; i < ptr->num_inputs; i++) {
    node_id_t input = ptr->inputs[i].node;
    int idx = ptr->inputs[i].idx;
    in[i] = get_buffer(s_nodes[input].outputs[idx]);
  }

  for (int i = 0; i < ptr->num_outputs; i++)
    out[i] = get_buffer(ptr->outputs[i]);

  ptr->process_fn(ptr->data_ptr, n,
                  in, ptr->num_inputs,
                  out, ptr->num_outputs);
}

void process_nodes(unsigned long n)
{
  for (int i = 0; i < s_num_sorted; i++)
    process_node(s_sorted[i], n);
}

void node_set_label(node_id_t node, const char *label)
{
  s_nodes[node].label = strdup(label);
}

void export_graph(const char *path)
{
  FILE *f = fopen(path, "w");

  if (f == NULL) {
    perror(path);
    return;
  }

  fprintf(f, "digraph {");

  for (int i = 0; i < s_num_nodes; i++) {
    struct node *ptr = &s_nodes[i];
    const char *label = ptr->label ? ptr->label : "?";
    fprintf(f, "\"%d\" [label=\"%d:%s\"]\n", i, i, label);

    for (int j = 0; j < ptr->num_inputs; j++) {
      fprintf(f, "\"%d\" -> \"%d\"\n", i, ptr->inputs[j].node);
    }
  }

  fprintf(f, "}\n");
}
