#include "lib.h"

int main(int argc, char *argv[])
{
  char *remote_host = argv[1];
  short remote_port = atoi(argv[2]);
  short local_port = atoi(argv[3]);

  printf(
    "remote_host=%s\n"
    "remote_port=%hd\n"
    "local_port=%hd\n",
    remote_host, remote_port, local_port
  );

  struct sockaddr_in sin = {
    ADDR_SIZE,
    AF_INET,
    htons(local_port),
    INADDR_ANY,
    0
  };

  int s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (-1 == bind(s, (struct sockaddr *)&sin, ADDR_SIZE))
    dprintf(2, "[%d] failed to bind\n", errno);
  if (-1 == listen(s, 1000))
    dprintf(2, "[%d] failed to listen\n", errno);
  
  add_session(s);
 
  for (;;) {
    print_session();
    size_t n = session_count();

    struct pollfd *fds = (struct pollfd *)calloc(n, sizeof(struct pollfd));
    get_pollfds(fds, n);

    if (-1 == poll(fds, n, -1))
      dprintf(2, "[%d] failed to poll\n", errno);

    for (int i = 0; i < n; ++i) {
      if (!fds[i].revents) continue;
      if (fds[i].fd == s) {
        accept_connection(fds[i].fd);
      } else {
        handle_session(&fds[i]);
      }
    }

    free(fds);
  }
}

