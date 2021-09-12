#include <string.h>
#include "lib.h"

static Session *sessions = NULL;

size_t session_count(void)
{
  size_t total = 0;
  Session *cur = sessions;
  for (; cur; cur = cur->next) ++total;
  return total;
}

Session *add_session(int socket)
{
  Session *s = (Session *)malloc(sizeof(Session));
  memset(s, 0, sizeof(Session));
  s->socket = socket;
  s->next = sessions;
  sessions = s;
  return s;
}

Session *get_session(int socket)
{
  Session *cur = sessions;
  while (cur && cur->socket != socket)
    cur = cur->next;
  return cur;
}

void remove_session(int socket)
{
  Session d, *cur = &d;
  d.socket = -1;
  d.next = sessions;

  for (; cur && cur->next; cur = cur->next) {
    Session *next;
    if ((next = cur->next)->socket == socket) {
      printf("remove session [%d]", cur->next->socket);
      cur->next = next->next;
      free(next);
    }
  }
  sessions = d.next;
}

void get_pollfds(struct pollfd *fds, size_t n)
{
  Session *cur = sessions;
  for (int i = 0; i < n; ++i, cur = cur->next) {
    fds[i].fd = cur->socket;
    fds[i].events = POLLERR | POLLHUP | POLLIN;
  }
}

void print_session(void)
{
  Session *cur = sessions;
  for (; cur; cur = cur->next) {
    printf("[%d]", cur->socket);
  }
  printf("\n");
}


