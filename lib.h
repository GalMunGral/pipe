#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>

#define ADDR_SIZE sizeof(struct sockaddr_in)
#define BUF_SIZE 4096

typedef struct session {
  int socket;
  char buffer[BUF_SIZE];
  size_t read_n;
  struct session *next;
} Session;

Session *add_session(int socket);
void remove_session(int socket);
size_t session_count(void);

void print_session(void);

void get_pollfds(struct pollfd *fds, size_t n);

void accept_connection(int socket);
void handle_session(struct pollfd *fd);


