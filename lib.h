#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>

#define ADDR_SIZE sizeof(struct sockaddr_in)
#define BUF_SIZE 4096


typedef struct {
  char buf[BUF_SIZE];
  int idx;
  int len;
} Buffer;

typedef struct {
  int key;
  Buffer in;
  Buffer out;
} State;

typedef struct {
  int (*check)(char *start, size_t len);
  size_t consume;
  size_t copy;
  char *produce;
} Rule;

enum addr_t {
  IPV4 = 1,
  DOMAIN = 3,
  IPV6 = 4
};

typedef struct {
  enum addr_t atyp;
  union {
    struct in_addr in;
    struct in6_addr in6;
    char *dn;
  } addr;
  unsigned short port;
} Address;

typedef struct session {
  int socket;
  int peer;
  Address dest;
  State state;
  enum {
    CREATED,
    CONNECTED
  } status;
  struct session *next;
} Session;

Session *add_session(int socket);
Session *get_session(int socket);
void remove_session(int socket);
size_t session_count(void);

void print_session(void);

void get_pollfds(struct pollfd *fds, size_t n);

void accept_connection(int socket);
void handle_session(struct pollfd *fd);


