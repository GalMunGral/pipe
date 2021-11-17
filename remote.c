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

typedef struct sockaddr addr_t;
typedef struct sockaddr_in addr4_t;
typedef struct sockaddr_in6 addr6_t;
typedef struct pollfd pollfd_t;
typedef struct addrinfo addrinfo_t;

uint16_t port = 2080;

void *handle(void *);
int handshake(int);
void loop(int, int);
int connect_by_hostname(char *, char *);

int serv_sock;

void cleanup()
{
  printf("BYE-BYE!\n");
  close(serv_sock);
}

int main(int argc, char **argv)
{
  addr4_t serv_addr;

  atexit(cleanup);

  if (argc < 1)
  {
    exit(-1);
  }

  printf("%s\n", argv[0]);

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
#if !defined(__linux__)
  serv_addr.sin_len = sizeof(addr4_t);
#endif
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  if ((serv_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "socket()\n");
    exit(-1);
  }

  if (bind(serv_sock, (addr_t *)&serv_addr, sizeof(addr4_t)) < 0)
  {
    fprintf(stderr, "bind()\n");
    exit(-1);
  }

  if (listen(serv_sock, 10) < 0)
  {
    fprintf(stderr, "listen()\n");
    exit(-1);
  }

  printf("listening on %hu\n", port);

  for (;;)
  {
    pthread_t thread;
    int src_sock;
    addr4_t addr;
    socklen_t addr_len = sizeof(addr4_t);
    char addr_name[1024];

    if ((src_sock = accept(serv_sock, (addr_t *)&addr.sin_addr, &addr_len)) < 0)
    {
      fprintf(stderr, "accept()\n");
      exit(-1);
    }

    if (inet_ntop(AF_INET, (addr_t *)&addr, (char *)&addr_name, sizeof(addr_name)) == NULL)
    {
      fprintf(stderr, "inet_ntop()\n");
      exit(-1);
    }
    printf("Connected to %s\n", addr_name);
    pthread_create(&thread, NULL, handle, &src_sock);
  }

  return 0;
}

void *handle(void *arg)
{
  int src_sock = *(int *)arg;
  int dst_sock;
  if ((dst_sock = handshake(src_sock)) > 0)
  {
    loop(src_sock, dst_sock);
    printf("LOOP EXIT\n");
  }
  close(src_sock);
  close(dst_sock);
  return NULL;
}

int handshake(int src_sock)
{
  int dst_sock = -1;
  unsigned char ATYPE;
  unsigned char ADDR_LEN;
  char DST_ADDR[1024] = {0};
  unsigned short DST_PORT;

  printf("HANDSHAKE\n");

  if (recv(src_sock, &ATYPE, 1, 0) < 0)
  {
    fprintf(stderr, "handshake >> ATYPE\n");
    return -1;
  }

  switch (ATYPE)
  {
  case 0x01:
    printf("IPv4\n");

    if (recv(src_sock, &DST_ADDR, 4, 0) < 0)
    {
      fprintf(stderr, "handshake >> DST_ADDR\n");
      return -1;
    }

    break;

  case 0x04:
    printf("IPv6\n");

    if (recv(src_sock, &DST_ADDR, 16, 0) < 0)
    {
      fprintf(stderr, "handshake >> DST_ADDR\n");
      return -1;
    }
    break;

  case 0x03:
    printf("DOMAIN NAME\n");

    if (recv(src_sock, &ADDR_LEN, 1, 0) < 0)
    {
      fprintf(stderr, "handshake >> ADDR_LEN\n");
      return -1;
    }

    if (recv(src_sock, &DST_ADDR, ADDR_LEN, 0) < 0)
    {
      fprintf(stderr, "handshake >> DST_ADDR\n");
      return -1;
    }
    break;

  default:
    fprintf(stderr, "handshake >> ATYPE\n");
    return -1;
  }

  if (recv(src_sock, &DST_PORT, 2, 0) < 0)
  {
    fprintf(stderr, "handshake >> PORT\n");
    return -1;
  }

  switch (ATYPE)
  {
  case 0x01:
  {
    addr4_t addr;

    if ((dst_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      fprintf(stderr, "handshake::socket()\n");
      close(dst_sock);
      return -1;
    }

    addr.sin_family = AF_INET;
#if !defined(__linux__)
    addr.sin_len = sizeof(addr4_t);
#endif
    addr.sin_addr = *(struct in_addr *)DST_ADDR;
    addr.sin_port = DST_PORT;

    if (connect(dst_sock, (addr_t *)&addr, sizeof(addr4_t)) < 0)
    {
      fprintf(stderr, "handshake::connect()\n");
      close(dst_sock);
      return -1;
    }

    break;
  }

  case 0x04:
  {
    addr6_t addr;

    if ((dst_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      fprintf(stderr, "handshake::socket()\n");
      close(dst_sock);
      return -1;
    }

    addr.sin6_family = AF_INET6;
#if !defined(__linux__)
    addr.sin6_len = sizeof(addr6_t);
#endif
    addr.sin6_addr = *(struct in6_addr *)DST_ADDR;
    addr.sin6_port = DST_PORT;

    if (connect(dst_sock, (addr_t *)&addr, sizeof(addr6_t)) < 0)
    {
      fprintf(stderr, "handshake::connect(), %d\n", errno);
      close(dst_sock);
      return -1;
    }

    break;
  }

  case 0x03:
  {
    char PORT[16];
    sprintf(PORT, "%hu", ntohs(DST_PORT));
    printf("RESOLVE: %s:%s\n", DST_ADDR, PORT);
    if ((dst_sock = connect_by_hostname(DST_ADDR, PORT)) < 0)
    {
      return -1;
    }
    ATYPE = 0x01;
    break;
  }
  }

  if (send(src_sock, "\x05", 1, 0) < 0)
  {
    fprintf(stderr, "handshake << VER\n");
    close(dst_sock);
    return -1;
  }

  if (send(src_sock, "\x00", 1, 0) < 0)
  {
    fprintf(stderr, "handshake << REP\n");
    close(dst_sock);
    return -1;
  }

  if (send(src_sock, "\x00", 1, 0) < 0)
  {
    fprintf(stderr, "handshake << RSV\n");
    close(dst_sock);
    return -1;
  }

  if (send(src_sock, &ATYPE, 1, 0) < 0)
  {
    fprintf(stderr, "handshake << ATYP\n");
    close(dst_sock);
    return -1;
  }

  switch (ATYPE)
  {
  case 0x01:
  {
    addr4_t addr;
    socklen_t len = sizeof(addr4_t);

    printf("ITS HERE V4\n");
    if (getsockname(dst_sock, (addr_t *)&addr, &len) < 0)
    {
      fprintf(stderr, "handshake::getsockname()\n");
      close(dst_sock);
      return -1;
    }

    if (send(src_sock, "\x01", 1, 0) < 0)
    {
      fprintf(stderr, "handshake << ATYPE\n");
      close(dst_sock);
      return -1;
    }

    if (send(src_sock, &addr.sin_addr, 4, 0) < 0)
    {
      fprintf(stderr, "handshake << BND_ADDR\n");
      close(dst_sock);
      return -1;
    }

    if (send(src_sock, &addr.sin_port, 2, 0) < 0)
    {
      fprintf(stderr, "handshake << BND_PORT\n");
      close(dst_sock);
      return -1;
    }
    break;
  }

  case 0x04:
  {
    addr6_t addr;
    socklen_t len = sizeof(addr6_t);
    printf("ITS HERE V6\n");

    if (getsockname(dst_sock, (addr_t *)&addr, &len) < 0)
    {
      fprintf(stderr, "handshake::getsockname()\n");
      close(dst_sock);
      return -1;
    }

    if (send(src_sock, "\x04", 1, 0) < 0)
    {
      fprintf(stderr, "handshake << ATYPE\n");
      close(dst_sock);
      return -1;
    }

    if (send(src_sock, &addr.sin6_addr, 16, 0) < 0)
    {
      fprintf(stderr, "handshake << BND_ADDR\n");
      close(dst_sock);
      return -1;
    }

    if (send(src_sock, &addr.sin6_port, 2, 0) < 0)
    {
      fprintf(stderr, "handshake << BND_PORT\n");
      close(dst_sock);
      return -1;
    }
    break;
  }
  default:
  {
    fprintf(stderr, "You're not supposed to be here...\n");
    close(dst_sock);
    return -1;
  }
  }
  return dst_sock;
}

void loop(int sock1, int sock2)
{
  printf("%d, %d\n", sock1, sock2);
  pollfd_t fds[2];
  int socks[2] = {sock1, sock2};
  char buf[4096];
  ssize_t size;

  fds[0].fd = sock1;
  fds[0].events = POLLIN;
  fds[1].fd = sock2;
  fds[1].events = POLLIN;

  for (;;)
  {
    int res = poll((pollfd_t *)&fds, 2, 200);

    if (res < 0)
    {
      fprintf(stderr, "loop::poll()\n");
      return;
    }

    if (res == 0)
    {
      printf("TIMEOUT, LOOP EXIT\n");
      return;
    }

    for (int i = 0; i < 2; i++)
    {
      if (fds[i].revents)
      {
        if (fds[i].revents & POLLIN)
        {
          size = recv(socks[i], &buf, sizeof(buf), 0);
          if (size < 0)
          {
            fprintf(stderr, "loop::recv()\n");
            return;
          }

          if ((size = send(socks[i ^ 1], &buf, size, 0)) < 0)
          {
            fprintf(stderr, "loop::send()\n");
            return;
          }

          if (size == 0)
          {
            printf("FINISHED, LOOP EXIT\n");
            return;
          }
        }
        else
        {
          printf("CLOSED, LOOP EXIT\n");
          return;
        }
      }
    }
  }
}

int connect_by_hostname(char *host, char *port)
{
  int sock;
  addrinfo_t hints;
  addrinfo_t *res;
  addrinfo_t *cur;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(host, port, &hints, &res) != 0)
  {
    fprintf(stderr, "handshake::getaddrinfo()\n");
    exit(-1);
  }

  for (cur = res; cur != NULL; cur = cur->ai_next)
  {
    if ((sock = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol)) < 0)
      continue;
    if (connect(sock, cur->ai_addr, cur->ai_addrlen) == 0)
    {
      printf("SUCCESS\n");
      break;
    }
    printf("NOT BREAK\n");
    close(sock);
  }

  if (cur == NULL)
  {
    fprintf(stderr, "handshake::RESOLVE\n");
    return -1;
  }

  if (cur->ai_family != AF_INET)
  {
    fprintf(stderr, "handshake::cur->ai_family (%d)\n", cur->ai_family);
    return -1;
  }

  char name[1024];
  inet_ntop(AF_INET, &((addr4_t *)cur->ai_addr)->sin_addr, (char *)&name, sizeof(name));
  printf("remote: %s:%hu\n", name, ntohs(((addr4_t *)cur->ai_addr)->sin_port));
  freeaddrinfo(res);
  return sock;
}
