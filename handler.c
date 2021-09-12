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

void handle_session(struct pollfd *fd)
{
  echo(fd->fd);
}
