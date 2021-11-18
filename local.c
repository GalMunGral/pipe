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

uint16_t port = 2081;

void *handle(void *);
int handshake(int);
void loop(int, int);
int connect_by_hostname(char *, char *);

int serv_sock;
char *REMOTE_ADDR;
char *REMOTE_PORT;

void cleanup()
{
  printf("BYE-BYE!\n");
  close(serv_sock);
}

int main(int argc, char **argv)
{
  addr4_t serv_addr;

  atexit(cleanup);

  if (argc < 3)
  {
    fprintf(stderr, "./remote [server] [port]\n");
    exit(-1);
  }

  REMOTE_ADDR = argv[1];
  REMOTE_PORT = argv[2];

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

    if ((src_sock = accept(serv_sock, (addr_t *)&addr, &addr_len)) < 0)
    {
      fprintf(stderr, "accept()\n");
      // exit(-1);
    }

    if (inet_ntop(AF_INET, &addr.sin_addr, (char *)&addr_name, sizeof(addr_name)) == NULL)
    {
      fprintf(stderr, "inet_ntop()\n");
      // exit(-1);
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
  char VER;
  unsigned char NMETHODS;
  char METHODS[256] = {0};
  char CMD;
  char RSV;
  char ATYPE;
  unsigned char ADDR_LEN;
  char DST_ADDR[1024] = {0};
  unsigned short DST_PORT;
  unsigned char BND_ADDR[4];
  unsigned char BND_PORT[2];

  printf("HANDSHAKE\n");

  if (recv(src_sock, &VER, 1, 0) < 0 || VER != 0x05)
  {
    fprintf(stderr, "handshake >> VER\n");
    return -1;
  }

  if (recv(src_sock, &NMETHODS, 1, 0) < 0)
  {
    fprintf(stderr, "handshake >> NMETHODS\n");
    return -1;
  }

  if (recv(src_sock, &METHODS, NMETHODS, 0) < 0)
  {
    fprintf(stderr, "handshake >> NMETHODS\n");
    return -1;
  }

  if ((dst_sock = connect_by_hostname(REMOTE_ADDR, REMOTE_PORT)) < 0)
  {
    fprintf(stderr, "handshake ==== remote\n");
    return -1;
  }

  if (send(src_sock, "\x05", 1, 0) < 0)
  {
    fprintf(stderr, "handshake << VER\n");
    return -1;
  }

  if (send(src_sock, "\x00", 1, 0) < 0)
  {
    fprintf(stderr, "handshake << METHOD\n");
    return -1;
  }

  if (recv(src_sock, &VER, 1, 0) < 0 || VER != 0x05)
  {
    fprintf(stderr, "handshake >> VER\n");
    return -1;
  }

  if (recv(src_sock, &CMD, 1, 0) < 0 || CMD != 0x01)
  {
    fprintf(stderr, "handshake >> CMD\n");
    return -1;
  }

  if (recv(src_sock, &RSV, 1, 0) < 0)
  {
    fprintf(stderr, "handshake >> RSV\n");
    return -1;
  }

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
      fprintf(stderr, "handshake >> ADDR_LEN (%d)\n", errno);
      return -1;
    }

    if (recv(src_sock, &DST_ADDR, ADDR_LEN, 0) < 0)
    {
      fprintf(stderr, "handshake >> DST_ADDR\n");
      return -1;
    }
    break;

  default:
    fprintf(stderr, ">> INVALID ATYPE (%d)\n", ATYPE);
    return -1;
  }

  if (recv(src_sock, &DST_PORT, 2, 0) < 0)
  {
    fprintf(stderr, "handshake >> PORT\n");
    return -1;
  }

  if (send(dst_sock, &ATYPE, 1, 0) < 0)
  {
    fprintf(stderr, "handshake --> ATYPE\n");
    close(dst_sock);
    return -1;
  }

  switch (ATYPE)
  {
  case 0x01:
  {
    if (send(dst_sock, DST_ADDR, 4, 0) < 0)
    {
      fprintf(stderr, "handshake --> DST_ADDR\n");
      close(dst_sock);
      return -1;
    }
    break;
  }

  case 0x04:
  {
    if (send(dst_sock, DST_ADDR, 16, 0) < 0)
    {
      fprintf(stderr, "handshake --> DST_ADDR\n");
      close(dst_sock);
      return -1;
    }
    break;
  }
  case 0x03:
  {
    if (send(dst_sock, &ADDR_LEN, 1, 0) < 0)
    {
      fprintf(stderr, "handshake --> ADDR_LEN (%d)\n", errno);
      close(dst_sock);
      return -1;
    }
    if (send(dst_sock, DST_ADDR, ADDR_LEN, 0) < 0)
    {
      fprintf(stderr, "handshake --> DST_ADDR\n");
      close(dst_sock);
      return -1;
    }
    break;
  }
  default:
  {
    fprintf(stderr, "INVALID ATYPE (%d)\n", ATYPE);
    close(dst_sock);
    return -1;
  }
  }

  if (send(dst_sock, &DST_PORT, 2, 0) < 0)
  {
    fprintf(stderr, "handshake --> DST_PORT\n");
    close(dst_sock);
    return -1;
  }

  if (recv(dst_sock, &ATYPE, 1, 0) < 0)
  {
    fprintf(stderr, "handshake <-- ATYPE\n");
    close(dst_sock);
    return -1;
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
    if (recv(dst_sock, BND_ADDR, 4, 0) < 0)
    {
      fprintf(stderr, "handshake <-- BND_ADDR\n");
      close(dst_sock);
      return -1;
    }

    if (send(src_sock, BND_ADDR, 4, 0) < 0)
    {
      fprintf(stderr, "handshake << BND_ADDR\n");
      close(dst_sock);
      return -1;
    }
    break;
  }

  case 0x04:
  {
    if (recv(dst_sock, BND_ADDR, 16, 0) < 0)
    {
      fprintf(stderr, "handshake <-- BND_ADDR\n");
      close(dst_sock);
      return -1;
    }

    if (send(src_sock, BND_ADDR, 16, 0) < 0)
    {
      fprintf(stderr, "handshake << BND_ADDR\n");
      close(dst_sock);
      return -1;
    }
    break;
  }
  default:
  {
    fprintf(stderr, "YOU SHOULDN'T BE HERE, %d\n", ATYPE);
    close(dst_sock);
    return -1;
  }
  }

  if (recv(dst_sock, BND_PORT, 2, 0) < 0)
  {
    fprintf(stderr, "handshake <-- BND_PORT\n");
    close(dst_sock);
    return -1;
  }

  if (send(src_sock, BND_PORT, 2, 0) < 0)
  {
    fprintf(stderr, "handshake << BND_PORT\n");
    close(dst_sock);
    return -1;
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
            fprintf(stderr, "loop::recv() (%d)\n", errno);
            return;
          }

          // write(1, &buf, size);

          if ((size = send(socks[i ^ 1], &buf, size, MSG_NOSIGNAL)) < 0)
          {
            fprintf(stderr, "loop::send()\n");
            return;
          }

          // if (size == 0)
          // {
          //   printf("FINISHED, LOOP EXIT\n");
          //   return;
          // }
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
    return -1;
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
