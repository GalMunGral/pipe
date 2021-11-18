#include "lib.h"

#define PORT 2080

void *handle(void *);
int handle_by_type(int);
int handle_ipv4(int src);
int handle_ipv6(int src);
int handle_hostname(int src);

int sock;

void cleanup()
{
  printf("BYE-BYE!\n");
  close(sock);
}

int main()
{
  atexit(cleanup);
  signal(SIGPIPE, SIG_IGN);

  addr4_t serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);
  serv_addr.sin_addr.s_addr = INADDR_ANY;
#if !defined(__linux__)
  serv_addr.sin_len = sizeof(addr4_t);
#endif
  assert((sock = socket(AF_INET, SOCK_STREAM, 0)) > 0);
  assert(bind(sock, (addr_t *)&serv_addr, sizeof(addr4_t)) == 0);
  assert(listen(sock, 10) == 0);
  printf("listening on port %d\n", PORT);

  for (;;)
  {
    int src;
    addr4_t addr;
    socklen_t len = sizeof(addr4_t);
    pthread_t thread;
    if ((src = accept(sock, (addr_t *)&addr, &len)) > 0)
      pthread_create(&thread, NULL, handle, &src);
  }

  return 0;
}

void *handle(void *arg)
{
  int src_sock = *(int *)arg;
  int dst_sock;
  ensure((dst_sock = handle_by_type(src_sock)) > 0, "handle_by_type()");
  loop(src_sock, dst_sock);
  return NULL;

failure:
  close(src_sock);
  close(dst_sock);
  return NULL;
}

int handle_by_type(int src)
{
  unsigned char ATYPE;
  ensure(recv(src, &ATYPE, 1, 0) > 0, ">>ATYPE");

  switch (ATYPE)
  {
  case 0x01:
    return handle_ipv4(src);
  case 0x04:
    return handle_ipv6(src);
  case 0x03:
    return handle_hostname(src);
  default:
    return -1;
  }

failure:
  return -1;
}

int handle_ipv4(int src)
{
  int dst = -1;
  char DST_ADDR[4];
  unsigned short DST_PORT;
  addr4_t addr;
  addr4_t bnd_addr;
  socklen_t len = sizeof(addr4_t);

  ensure(recv(src, &DST_ADDR, 4, 0) > 0, ">> DST_ADDR");
  ensure(recv(src, &DST_PORT, 2, 0) > 0, ">> DST_PORT");
  ensure((dst = socket(AF_INET, SOCK_STREAM, 0)) > 0, "ipv4 socket");

  addr.sin_family = AF_INET;
  addr.sin_addr = *(struct in_addr *)DST_ADDR;
  addr.sin_port = DST_PORT;
#if !defined(__linux__)
  addr.sin_len = sizeof(addr4_t);
#endif
  ensure(!connect(dst, (addr_t *)&addr, sizeof(addr4_t)), "connect()");

  ensure(!getsockname(dst, (addr_t *)&bnd_addr, &len), "getsockname()");
  ensure(send(src, "\x01", 1, 0) > 0, "<< ATYPE");
  ensure(send(src, &bnd_addr.sin_addr, 4, 0) > 0, "<< BND_ADDR");
  ensure(send(src, &bnd_addr.sin_port, 2, 0) > 0, "<< BND_PORT");
  return dst;

failure:
  close(dst);
  return -1;
}

int handle_ipv6(int src)
{
  int dst = -1;
  char DST_ADDR[16];
  unsigned short DST_PORT;
  addr6_t addr;
  addr6_t bnd_addr;
  socklen_t len = sizeof(addr6_t);

  ensure(recv(src, &DST_ADDR, 16, 0) > 0, ">> DST_ADDR");
  ensure(recv(src, &DST_PORT, 2, 0) > 0, ">> DST_PORT");
  ensure((dst = socket(AF_INET, SOCK_STREAM, 0)) > 0, "socket()");

  addr.sin6_family = AF_INET6;
  addr.sin6_addr = *(struct in6_addr *)DST_ADDR;
  addr.sin6_port = DST_PORT;
#if !defined(__linux__)
  addr.sin6_len = sizeof(addr6_t);
#endif
  ensure(!connect(dst, (addr_t *)&addr, sizeof(addr6_t)), "connect()");

  ensure(!getsockname(dst, (addr_t *)&bnd_addr, &len), "getsockname()");
  ensure(send(src, "\x04", 1, 0) > 0, "<< ATYPE");
  ensure(send(src, &bnd_addr.sin6_addr, 16, 0) > 0, "<< BND_ADDR");
  ensure(send(src, &bnd_addr.sin6_port, 2, 0) > 0, "<< BND_PORT");
  return dst;

failure:
  close(dst);
  return -1;
}

int handle_hostname(int src)
{
  int dst = -1;
  unsigned char ADDR_LEN;
  char DST_ADDR[1024] = {0};
  unsigned short DST_PORT;
  char dst_port[8];
  addr4_t bnd_addr;
  socklen_t len = sizeof(addr4_t);

  ensure(recv(src, &ADDR_LEN, 1, 0) > 0, "<< ADDR_LEN");
  ensure(recv(src, &DST_ADDR, ADDR_LEN, 0) > 0, "<< DST_ADDR");
  ensure(recv(src, &DST_PORT, 2, 0) > 0, "<< DST_PORT");
  sprintf(dst_port, "%hu", ntohs(DST_PORT));
  ensure((dst = connect_by_hostname(DST_ADDR, dst_port)) > 0, "socket()");

  ensure(!getsockname(dst, (addr_t *)&bnd_addr, &len), "getsockname()");
  ensure(send(src, "\x01", 1, 0) > 0, "<< ATYPE");
  ensure(send(src, &bnd_addr.sin_addr, 4, 0) > 0, "<< BND_ADDR");
  ensure(send(src, &bnd_addr.sin_port, 2, 0) > 0, "<< BND_PORT");
  return dst;

failure:
  close(dst);
  return -1;
}
