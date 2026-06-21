#ifndef NODE_H
#define NODE_H

#define NODE_MAX_CONNECTIONS 1024

typedef int node_id_t;
typedef int buffer_id_t;
typedef void (*node_process_fn_t)(void*, unsigned long, float**, int, float**, int);

void node_pool_init(void);
node_id_t new_node(void);
int node_add_input(node_id_t node);
int node_add_output(node_id_t node);
buffer_id_t node_get_output(node_id_t node, int idx);
buffer_id_t node_get_input(node_id_t node, int idx);
int node_get_num_outputs(node_id_t node);
void node_connect(node_id_t src, node_id_t dst, int src_idx, int dst_idx);
void node_set_data_ptr(node_id_t node, void *ptr);
void node_set_process_fn(node_id_t node, node_process_fn_t fn);
void node_set_cntl(node_id_t node);
int node_is_cntl(node_id_t node);
void resize_buffers(unsigned long size);
float *get_buffer(buffer_id_t buf);
void topo_sort_nodes(void);
void process_nodes(unsigned long n);
node_id_t get_root_node(void);
void node_set_label(node_id_t node, const char *label);
void export_graph(const char *path);

#endif
