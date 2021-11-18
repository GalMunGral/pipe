#include "lib.h"

#define PORT 2081

void *handle(void *);
int handle_by_type(int);

int sock;
char *REMOTE_ADDR;
char *REMOTE_PORT;

void cleanup()
{
  printf("BYE-BYE!\n");
  close(sock);
}

int main(int argc, char **argv)
{

  atexit(cleanup);
  signal(SIGPIPE, SIG_IGN);

  addr4_t serv_addr;
  ensure(argc == 3, "./remote [server] [port]");

  REMOTE_ADDR = argv[1];
  REMOTE_PORT = argv[2];

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(PORT);
#if !defined(__linux__)
  serv_addr.sin_len = sizeof(addr4_t);
#endif
  ensure((sock = socket(AF_INET, SOCK_STREAM, 0)) > 0, "socket()");
  ensure(!bind(sock, (addr_t *)&serv_addr, sizeof(addr4_t)), "bind()");
  ensure(!listen(sock, 10), "listen()");
  printf("listening on %d\n", PORT);

  for (;;)
  {
    pthread_t thread;
    int src;
    addr4_t addr;
    socklen_t addr_len = sizeof(addr4_t);
    ensure((src = accept(sock, (addr_t *)&addr, &addr_len)) > 0, "accept()");
    pthread_create(&thread, NULL, handle, &src);
  }
  return 0;

failure:
  close(sock);
  return -1;
}

void *handle(void *arg)
{
  int src = *(int *)arg;
  int dst;
  ensure((dst = handle_by_type(src)) > 0, "handle_by_type()");
  loop(src, dst);
  return NULL;

failure:
  close(src);
  close(dst);
  return NULL;
}

int handle_ipv4(int src)
{
  int dst = -1;
  char DST_ADDR[4];
  char DST_PORT[2];
  char ATYPE;
  char BND_ADDR[4];
  char BND_PORT[2];

  ensure(recv(src, DST_ADDR, 4, 0) > 0, ">> DST_ADDR");
  ensure(recv(src, DST_PORT, 2, 0) > 0, ">> DST_PORT");

  ensure((dst = connect_by_hostname(REMOTE_ADDR, REMOTE_PORT)) > 0, "socket()");

  ensure(send(dst, "\x01", 1, 0) > 0, "==> ATYPE");
  ensure(send(dst, DST_ADDR, 4, 0) > 0, "==> DST_ADDR");
  ensure(send(dst, DST_PORT, 2, 0) > 0, "==> DST_PORT");

  ensure(recv(dst, &ATYPE, 1, 0) > 0, "<=== ATYPE");
  ensure(recv(dst, BND_ADDR, 4, 0) > 0, "<== BND_ADDR");
  ensure(recv(dst, BND_PORT, 2, 0) > 0, "<== BND_PORT");

  ensure(send(src, "\x01", 1, 0) > 0, "<< ATYPE");
  ensure(send(src, BND_ADDR, 4, 0) > 0, "<< BND_ADDR");
  ensure(send(src, BND_PORT, 2, 0) > 0, "<< BND_PORT");
  return dst;

failure:
  close(dst);
  return -1;
}

int handle_ipv6(int src)
{
  int dst = -1;
  char DST_ADDR[16];
  char DST_PORT[2];
  char ATYPE;
  char BND_ADDR[16];
  char BND_PORT[2];

  ensure(recv(src, &DST_ADDR, 16, 0) > 0, ">> DST_ADDR");
  ensure(recv(src, &DST_PORT, 2, 0) > 0, ">> DST_PORT");

  ensure((dst = connect_by_hostname(REMOTE_ADDR, REMOTE_PORT)) > 0, "socket()");

  ensure(send(dst, "\x04", 1, 0) > 0, "==> ATYPE");
  ensure(send(dst, DST_ADDR, 16, 0) > 0, "==> DST_ADR");
  ensure(send(dst, &DST_PORT, 2, 0) > 0, "==> DST_PORT");

  ensure(recv(dst, &ATYPE, 1, 0) > 0, "<=== ATYPE");
  ensure(recv(dst, BND_ADDR, 16, 0) > 0, "<== BND_ADDR");
  ensure(recv(dst, BND_PORT, 2, 0) > 0, "<== BND_PORT");

  ensure(send(src, "\x04", 1, 0) > 0, "<< ATYPE");
  ensure(send(src, BND_ADDR, 16, 0) > 0, "<< BND_ADDR");
  ensure(send(src, BND_PORT, 2, 0) > 0, "<< BND_PORT");
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
  char DST_PORT[2];
  char ATYPE;
  char BND_ADDR[16];
  char BND_PORT[2];

  ensure(recv(src, &ADDR_LEN, 1, 0) > 0, ">> ADDR_LEN");
  ensure(recv(src, DST_ADDR, ADDR_LEN, 0) > 0, ">> DST_ADDR");
  ensure(recv(src, DST_PORT, 2, 0) > 0, ">> DST_PORT");

  ensure((dst = connect_by_hostname(REMOTE_ADDR, REMOTE_PORT)) > 0, "socket()");

  ensure(send(dst, &ATYPE, 1, 0) > 0, "==> ATYPE");
  ensure(send(dst, &ADDR_LEN, 1, 0) > 0, "==> ADDR_LEN");
  ensure(send(dst, DST_ADDR, ADDR_LEN, 0) > 0, "==> DST_ADDR");
  ensure(send(dst, DST_PORT, 2, 0) > 0, "==> DST_PORT");

  ensure(recv(dst, &ATYPE, 1, 0) > 0, "<=== ATYPE");
  ensure(recv(dst, BND_ADDR, ATYPE == 0x01 ? 4 : 16, 0) > 0, "<== BND_ADDR");
  ensure(recv(dst, BND_PORT, 2, 0) > 0, "<== BND_PORT");

  ensure(send(src, &ATYPE, 1, 0) > 0, "<< ATYPE");
  ensure(send(src, BND_ADDR, ATYPE == 0x01 ? 4 : 16, 0) > 0, "<< BND_ADDR");
  ensure(send(src, BND_PORT, 2, 0) > 0, "<< BND_PORT");
  return dst;

failure:
  close(dst);
  return -1;
}

int handle_by_type(int src)
{
  char VER;
  unsigned char NMETHODS;
  char METHODS[256] = {0};
  char CMD;
  char RSV;
  char ATYPE;

  ensure(recv(src, &VER, 1, 0) > 0 && VER == 0x05, "1>> VER");
  ensure(recv(src, &NMETHODS, 1, 0) > 0, "1>> NMETHODS");
  ensure(recv(src, &METHODS, NMETHODS, 0) > 0, "1>> METHODS");

  ensure(send(src, "\x05", 1, 0) > 0, "1<< VER");
  ensure(send(src, "\x00", 1, 0) > 0, "1<< METHOD");

  ensure(recv(src, &VER, 1, 0) > 0 && VER == 0x05, "2>> VER");
  ensure(recv(src, &CMD, 1, 0) > 0 && CMD == 0x01, "2>> CMD");
  ensure(recv(src, &RSV, 1, 0) > 0, "2>> RSV");
  ensure(recv(src, &ATYPE, 1, 0) > 0, "2>> ATYPE");

  ensure(send(src, "\x05", 1, 0) > 0, "2<< VER");
  ensure(send(src, "\x00", 1, 0) > 0, "2<< ACK");
  ensure(send(src, "\x00", 1, 0) > 0, "2<< RSV");

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
