#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>

typedef struct sockaddr addr_t;
typedef struct sockaddr_in addr4_t;
typedef struct sockaddr_in6 addr6_t;
typedef struct pollfd pollfd_t;
typedef struct addrinfo addrinfo_t;

int connect_by_hostname(char *, char *);
void loop(int, int);

#define ensure(cond, msg)                 \
  if (!(cond))                            \
  {                                       \
    fprintf(stderr, "[error] %s\n", msg); \
    goto failure;                         \
  }
