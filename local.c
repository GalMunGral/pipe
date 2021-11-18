#include "lib.h"

void *handle(void *);
int handle_by_type(int);
int handle_ipv4(int);
int handle_ipv6(int);
int handle_hostname(int);

int sock;
short port;
char *remote_addr;
char *remote_port;

void cleanup()
{
  printf("BYE-BYE!\n");
  close(sock);
}

int main(int argc, char **argv)
{
  atexit(cleanup);
  signal(SIGPIPE, SIG_IGN);

  assert(argc == 4);
  port = atoi(argv[1]);
  remote_addr = argv[2];
  remote_port = argv[3];

  addr4_t addr;
#if !defined(__linux__)
  addr.sin_len = sizeof(addr4_t);
#endif
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  assert((sock = socket(AF_INET, SOCK_STREAM, 0)) > 0);
  assert(bind(sock, (addr_t *)&addr, sizeof(addr4_t)) == 0);
  assert(listen(sock, 100) == 0);

  printf("listening on %hd\n", port);

  for (;;)
  {
    int src;
    pthread_t thread;
    if ((src = accept(sock, NULL, NULL)) > 0)
      pthread_create(&thread, NULL, handle, &src);
  }
  return 0;
}

void *handle(void *arg)
{
  int pair[2] = {*(int *)arg, -1};
  if ((pair[1] = handle_by_type(pair[0])) > 0)
    loop(pair);
  close(pair[0]);
  close(pair[1]);
  printf("close\n");
  return NULL;
}

int handle_by_type(int src)
{
  unsigned char buf;
  char methods[256];

  ensure(recv(src, &buf, 1, 0) > 0 && buf == 5, "> VER");
  ensure(recv(src, &buf, 1, 0) > 0 && buf > 0, "> NMETHODS");
  ensure(recv(src, &methods, buf, 0) > 0, "> METHODS");

  ensure(send(src, "\x05", 1, 0) > 0, "< VER");
  ensure(send(src, "\x00", 1, 0) > 0, "< METHOD");

  ensure(recv(src, &buf, 1, 0) > 0 && buf == 5, ">> VER");
  ensure(recv(src, &buf, 1, 0) > 0 && buf == 1, ">> CMD");
  ensure(recv(src, &buf, 1, 0) > 0 && buf == 0, ">> RSV");
  ensure(recv(src, &buf, 1, 0) > 0, ">> ATYP");

  ensure(send(src, "\x05", 1, 0) > 0, "<< VER");
  ensure(send(src, "\x00", 1, 0) > 0, "<< ACK");
  ensure(send(src, "\x00", 1, 0) > 0, "<< RSV");

  switch (buf)
  {
  case 1:
    printf("ipv4\n");
    return handle_ipv4(src);
  case 4:
    printf("ipv6\n");
    return handle_ipv6(src);
  case 3:
    return handle_hostname(src);
  default:
    return -1;
  }

error:
  return -1;
}

int handle_ipv4(int src)
{
  int dst = -1;

  char buf;
  char addr[IPV4_SIZE];
  char port[PORT_SIZE];

  ensure(recv(src, addr, IPV4_SIZE, 0) > 0, ">> DST_ADDR");
  ensure(recv(src, port, PORT_SIZE, 0) > 0, ">> DST_PORT");

  ensure((dst = connect_by_name(remote_addr, remote_port)) > 0, "ipv4 socket");

  ensure(send(dst, "\x01", 1, 0) > 0, "==> ATYP");
  ensure(send(dst, addr, IPV4_SIZE, 0) > 0, "==> DST_ADDR");
  ensure(send(dst, port, PORT_SIZE, 0) > 0, "==> DST_PORT");

  ensure(recv(dst, &buf, 1, 0) > 0, "<== ATYP");
  ensure(recv(dst, addr, IPV4_SIZE, 0) > 0, "<== BND_ADDR");
  ensure(recv(dst, port, PORT_SIZE, 0) > 0, "<== BND_PORT");

  ensure(send(src, "\x01", 1, 0) > 0, "<< ATYP");
  ensure(send(src, addr, IPV4_SIZE, 0) > 0, "<< BND_ADDR");
  ensure(send(src, port, PORT_SIZE, 0) > 0, "<< BND_PORT");
  return dst;

error:
  close(dst);
  return -1;
}

int handle_ipv6(int src)
{
  int dst = -1;

  char buf;
  char addr[IPV6_SIZE];
  char port[PORT_SIZE];

  ensure(recv(src, addr, IPV6_SIZE, 0) > 0, ">> DST_ADDR");
  ensure(recv(src, port, PORT_SIZE, 0) > 0, ">> DST_PORT");

  ensure((dst = connect_by_name(remote_addr, remote_port)) > 0, "ipv6 socket");

  ensure(send(dst, "\x04", 1, 0) > 0, "==> ATYP");
  ensure(send(dst, addr, IPV6_SIZE, 0) > 0, "==> DST_ADDR");
  ensure(send(dst, port, PORT_SIZE, 0) > 0, "==> DST_PORT");

  ensure(recv(dst, &buf, 1, 0) > 0, "<== ATYP");
  ensure(recv(dst, addr, IPV6_SIZE, 0) > 0, "<== BND_ADDR");
  ensure(recv(dst, port, PORT_SIZE, 0) > 0, "<== BND_PORT");

  ensure(send(src, "\x04", 1, 0) > 0, "<< ATYP");
  ensure(send(src, addr, IPV6_SIZE, 0) > 0, "<< BND_ADDR");
  ensure(send(src, port, PORT_SIZE, 0) > 0, "<< BND_PORT");
  return dst;

error:
  close(dst);
  return -1;
}

int handle_hostname(int src)
{
  int dst = -1;

  unsigned char buf;
  char addr[256] = {};
  char port[PORT_SIZE];

  ensure(recv(src, &buf, 1, 0) > 0 && buf > 0, ">> N_ADDR");
  ensure(recv(src, addr, buf, 0) > 0, ">> DST_ADDR");
  ensure(recv(src, port, PORT_SIZE, 0) > 0, ">> DST_PORT");

  ensure((dst = connect_by_name(remote_addr, remote_port)) > 0, "ipv4 socket");

  ensure(send(dst, "\x03", 1, 0) > 0, "==> ATYP");
  ensure(send(dst, &buf, 1, 0) > 0, "==> N_ADDR");
  ensure(send(dst, addr, buf, 0) > 0, "==> DST_ADDR");
  ensure(send(dst, port, PORT_SIZE, 0) > 0, "==> DST_PORT");

  ensure(recv(dst, &buf, 1, 0) > 0, "<== ATYP (v4)");
  ensure(recv(dst, addr, IPV4_SIZE, 0) > 0, "<== BND_ADDR (v4)");
  ensure(recv(dst, port, PORT_SIZE, 0) > 0, "<== BND_PORT");

  ensure(send(src, "\x01", 1, 0) > 0, "<< ATYP");
  ensure(send(src, addr, IPV4_SIZE, 0) > 0, "<< BND_ADDR");
  ensure(send(src, port, PORT_SIZE, 0) > 0, "<< BND_PORT");
  return dst;

error:
  close(dst);
  return -1;
}
