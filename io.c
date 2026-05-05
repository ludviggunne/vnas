#include <stdio.h>
#include <stdlib.h>
#include <poll.h>

#include "io.h"

struct io_handle {
  int           event_flags;
  void         *arg;
  io_handler_t  handler;
};

static struct pollfd    *s_pollfds = NULL;
static struct io_handle *s_handles = NULL;
static unsigned long     s_num_handles = 0;

static void add_handler(int flags, int fd, void *args, io_handler_t handler)
{
  unsigned long i = s_num_handles++;

  s_pollfds = realloc(s_pollfds, sizeof(*s_pollfds) * s_num_handles);
  s_handles = realloc(s_handles, sizeof(*s_handles) * s_num_handles);

  struct pollfd *p = &s_pollfds[i];
  struct io_handle *h = &s_handles[i];

  p->fd = fd;
  p->events = flags;
  h->arg = args;
  h->handler = handler;
  h->event_flags = flags;
}

void io_add_output(int fd, void *args, io_handler_t handler)
{
  add_handler(POLLOUT, fd, args, handler);
}

void io_add_input(int fd, void *args, io_handler_t handler)
{
  add_handler(POLLIN, fd, args, handler);
}

void io_loop(void)
{
  while (poll(s_pollfds, s_num_handles, -1) >= 0) {
    for (int i = 0; i < s_num_handles; i++) {

      struct pollfd *p = &s_pollfds[i];
      struct io_handle *h = &s_handles[i];

      if (p->revents & h->event_flags) {
        h->handler(p->fd, h->arg);
      }
    }
  }
}
