#include "lib.h"

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
    int res = poll((pollfd_t *)&fds, 2, 10000);

    if (res < 0)
    {
      fprintf(stderr, "loop::poll()\n");
      return;
    }

    // if (res == 0)
    // {
    //   printf("TIMEOUT, LOOP EXIT\n");
    //   return;
    // }

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
