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

ssize_t recvall(int, void *, size_t, int);

int connect_by_name(char *, char *);
int loop(int[2]);

#define PORT_SIZE 2
#define IPV4_SIZE 4
#define IPV6_SIZE 16

#define LOOP_BUFFER_SIZE 8192
#define LOOP_POLL_TIMEOUT 5000

#define EXIT_SHUTDOWN 0
#define EXIT_POLL_ERR 1
#define EXIT_POLL_TIMEOUT 2
#define EXIT_RECV_ERR 3

#define PAD_SIZE 13

#define ensure(cond, msg)                                            \
    if (!(cond))                                                     \
    {                                                                \
        if (msg)                                                     \
            fprintf(stderr, "[error(%d)] %s\n", errno, (char *)msg); \
        goto error;                                                  \
    }
