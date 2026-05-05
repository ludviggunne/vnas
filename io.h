#ifndef IO_H
#define IO_H

typedef void (*io_handler_t)(int, void*);

void io_add_input(int fd, void *args, io_handler_t handler);
void io_loop(void);

#endif
