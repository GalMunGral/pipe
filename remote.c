#include "lib.h"

#define PORT 2080
void *handle(void *);
int handle_by_type(int);
int handle_ipv4(int);
int handle_ipv6(int);
int handle_hostname(int);

int sock;
short port;

void cleanup()
{
    close(sock);
}

int main(int argc, char **argv)
{
    atexit(cleanup);
    signal(SIGPIPE, SIG_IGN);

    assert(argc == 2);
    port = atoi(argv[1]);

    addr4_t serv_addr;
#if !defined(__linux__)
    serv_addr.sin_len = sizeof(addr4_t);
#endif
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    assert((sock = socket(AF_INET, SOCK_STREAM, 0)) > 0);
    assert(bind(sock, (addr_t *)&serv_addr, sizeof(addr4_t)) == 0);
    assert(listen(sock, 100) == 0);

    printf("listening on port %hd\n", port);

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
    return NULL;
}

int handle_by_type(int src)
{
    unsigned char atyp;
    ensure(recv(src, &atyp, 1, 0) > 0, ">>ATYP");

    switch (atyp)
    {
    case 1:
        return handle_ipv4(src);
    case 4:
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

    addr4_t addr;
#if !defined(__linux__)
    addr.sin_len = sizeof(addr4_t);
#endif
    addr.sin_family = AF_INET;
    ensure(recv(src, &addr.sin_addr, IPV4_SIZE, 0) > 0, "==> DST_ADDR");
    ensure(recv(src, &addr.sin_port, PORT_SIZE, 0) > 0, "==> DST_PORT");

    socklen_t len = sizeof(addr4_t);
    ensure((dst = socket(AF_INET, SOCK_STREAM, 0)) > 0, "ipv4 socket");
    ensure(connect(dst, (addr_t *)&addr, sizeof(addr4_t)) == 0, "connect()");
    ensure(getsockname(dst, (addr_t *)&addr, &len) == 0, "getsockname()");

    ensure(send(src, "\x01", 1, 0) > 0, "<== ATYP");
    ensure(send(src, &addr.sin_addr, IPV4_SIZE, 0) > 0, "<== BND_ADDR");
    ensure(send(src, &addr.sin_port, PORT_SIZE, 0) > 0, "<== BND_PORT");
    return dst;

error:
    close(dst);
    return -1;
}

int handle_ipv6(int src)
{
    int dst = -1;

    addr6_t addr;
#if !defined(__linux__)
    addr.sin6_len = sizeof(addr6_t);
#endif
    addr.sin6_family = AF_INET6;
    ensure(recv(src, &addr.sin6_addr, IPV6_SIZE, 0) > 0, "==> DST_ADDR");
    ensure(recv(src, &addr.sin6_port, PORT_SIZE, 0) > 0, "==> DST_PORT");

    socklen_t len = sizeof(addr6_t);
    ensure((dst = socket(AF_INET6, SOCK_STREAM, 0)) > 0, "ipv6 socket");
    ensure(connect(dst, (addr_t *)&addr, sizeof(addr6_t)) == 0, "connect()");
    ensure(getsockname(dst, (addr_t *)&addr, &len) == 0, "getsockname()");

    ensure(send(src, "\x04", 1, 0) > 0, "<== ATYP");
    ensure(send(src, &addr.sin6_addr, IPV6_SIZE, 0) > 0, "<== BND_ADDR");
    ensure(send(src, &addr.sin6_port, PORT_SIZE, 0) > 0, "<== BND_PORT");
    return dst;

error:
    close(dst);
    return -1;
}

int handle_hostname(int src)
{
    int dst = -1;

    unsigned char n_addr;
    char name[256] = {};
    char port[8] = {};
    ensure(recv(src, &n_addr, 1, 0) > 0, "==> N_ADDR");
    ensure(recv(src, name, n_addr, 0) > 0, "==> DST_ADDR");
    ensure(recv(src, port, PORT_SIZE, 0) > 0, "==> DST_PORT");
    sprintf(port, "%hu", ntohs(*(unsigned short *)port));

    addr4_t addr;
    socklen_t len = sizeof(addr4_t);
    ensure((dst = connect_by_name(name, port)) > 0, "ipv4 socket()");
    ensure(getsockname(dst, (addr_t *)&addr, &len) == 0, "getsockname()");

    ensure(send(src, "\x01", 1, 0) > 0, "<== ATYP");
    ensure(send(src, &addr.sin_addr, IPV4_SIZE, 0) > 0, "<== BND_ADDR");
    ensure(send(src, &addr.sin_port, PORT_SIZE, 0) > 0, "<== BND_PORT");
    return dst;

error:
    close(dst);
    return -1;
}
