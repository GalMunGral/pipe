#include <string.h>
#include "lib.h"

void accept_connection(int sock)
{
  int client;
  struct sockaddr_in client_addr;
  socklen_t addr_size = ADDR_SIZE;

  if (-1 == (client = accept(sock, (struct sockaddr *)&client_addr, &addr_size))) {
    dprintf(2, "[%d] failed to accept\n", errno);
  }
  
  printf("ACCEPTED\n");
  add_session(client);
}


void echo(int sock)
{

  char buffer[BUF_SIZE];
  size_t read_size;
  printf("HANDLED\n");

  if (-1 == (read_size = recv(sock, &buffer, BUF_SIZE, 0))) {
    dprintf(2, "[%d] failed to read\n", errno);
  } else if (read_size == 0) {
    printf("connection closed\n");
    remove_session(sock);
  } else if (read_size != (write(sock, &buffer, read_size))) {
    dprintf(2, "[%d] failed to write\n", errno);
  }
}

void forward(int sock_a, int sock_b)
{
  char buffer[BUF_SIZE];
  size_t read_size;

  if (-1 == (read_size = recv(sock_a, &buffer, BUF_SIZE, 0))) {
    dprintf(2, "[%d] failed to read\n", errno);
  } else if (read_size == 0) {
    printf("connection closed\n");
    remove_session(sock_a);
  } else if (read_size != (write(sock_b, &buffer, read_size))) {
    dprintf(2, "[%d] failed to write\n", errno);
  }
}

void handle_session(struct pollfd *fd)
{
  // echo(fd->fd);
  if (!(fd->revents & POLLIN)) {
    return;
  }

  int sock = fd->fd;
  Session *sess = get_session(sock);

  void next_state(Session *sess);
  
  if (sess->status == CONNECTED) {
    forward(sock, sess->peer);
    return;
  }

  int nread = 0;
  void advance_state(State *s);

  if (-1 == (nread = recv(
    sock, 
    sess->state.in.buffer + sess->state.in.n,
    BUF_SIZE - sess->state.in.n,
    0
  ))) {
    dprintf(2, "[%d] failed to read\n", errno);
    remove_session(sock);
  } else if (0 == nread) {
    printf("connection closed\n");
    remove_session(sock);
  } else {
    printf("advance state\n");
    sess->state.in.n += nread;
    advance_state(&sess->state);
    printf("recv (%d)\n", nread);
  }
}

void advance_state(State *s)
{
  for(;;) {
    switch (s->key) {
      case 0:
        if (s->in.n - s->in.index >= 5) {
          strncpy(s->out.buffer + s->out.index,
            s->in.buffer + s->in.index, 5);
          s->out.index += 5;
          s->in.index += 5;
          s->out.buffer[s->out.index++] = '+';
          s->key = 1;
          break;
        } else {
          return;
        }
      case 1:
        if (s->in.n - s->in.index >= 5) {
          strncpy(s->out.buffer + s->out.index,
            s->in.buffer + s->in.index, 5);
          s->out.index += 5;
          s->in.index += 5;
          s->key = 2;
          break;
        } else {
          return;
        }
      default:
        return;
    }
    printf("STATE [%d]\n", s->key);
    printf(
      ">>>IN(%d): %s<<<\n\n>>>OUT(%d): %s<<<\n\n=",
      s->in.index,
      s->in.buffer,
      s->out.index,
      s->out.buffer
    );
  }

}














