#include "lib.h"

int connect_by_name(char *name, char *port)
{
  int sock = -1;

  addrinfo_t hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET; // use IPv4 for now
  hints.ai_socktype = SOCK_STREAM;

  addrinfo_t *result;
  ensure(getaddrinfo(name, port, &hints, &result) == 0, "getaddrinfo()");

  addrinfo_t *p;
  for (p = result; p != NULL; p = p->ai_next)
  {
    if (p->ai_family == AF_INET && (sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) > 0 &&
        connect(sock, p->ai_addr, p->ai_addrlen) == 0)
      break;
    close(sock);
  }
  freeaddrinfo(result);
  return p ? sock : -1;

error:
  close(sock);
  freeaddrinfo(result);
  return -1;
}

int loop(int pair[2])
{
  pollfd_t fds[2];
  fds[0].fd = pair[0];
  fds[1].fd = pair[1];
  fds[0].events = POLLIN;
  fds[1].events = POLLIN;

  for (;;)
  {
    char n_closed = 0;

    int res = poll((pollfd_t *)&fds, 2, LOOP_POLL_TIMEOUT);
    if (res == -1)
      return EXIT_POLL_ERR;
    if (res == 0)
      return EXIT_POLL_TIMEOUT;

    for (int i = 0; i < 2; i++)
    {
      char buf[4096];
      if (fds[i].revents & POLLIN)
      {
        ssize_t size = recv(pair[i], buf, sizeof(buf), 0);
        if (size == -1)
          return EXIT_RECV_ERR;
        if (size == 0)
          n_closed++;
        // no error checking here
        send(pair[i ^ 1], buf, size, 0);
      }
    }

    if (n_closed == 2)
      return EXIT_SHUTDOWN;
  }
}
