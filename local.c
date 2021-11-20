#include "lib.h"

void *handle(void *);
int handle_by_type(int, int);
int handle_ipv4(int, int);
int handle_ipv6(int, int);
int handle_hostname(int, int);

int sock;
short port;
char *remote_addr;
char *remote_port;

void cleanup()
{
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
    int pair[2] = {-1, -1};
    ensure((pair[0] = *(int *)arg) > 0, "socket(local)");
    ensure((pair[1] = connect_by_name(remote_addr, remote_port)) > 0, "socket(remote)");

    char pad[PAD_SIZE]; // naive handshake
    ensure(send(pair[1], pad, PAD_SIZE, 0) > 0, "(pad) --> [remote]");
    ensure(recv(pair[1], pad, PAD_SIZE, 0) > 0, "(pad) <-- [remote]");

    ensure((handle_by_type(pair[0], pair[1]) == 0), "failed to establish connection");

    loop(pair);

error:
    close(pair[0]);
    close(pair[1]);
    return NULL;
}

/**
 * Returns 0 for success and -1 for error
 */
int handle_by_type(int src, int dst)
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
    ensure(recv(src, &buf, 1, 0) > 0 && (buf == 1 || buf == 3 || buf == 4), ">> ATYP");

    switch (buf)
    {
    case 1:
        return handle_ipv4(src, dst);
    case 4:
        return handle_ipv6(src, dst);
    case 3:
        return handle_hostname(src, dst);
    }

error:
    return -1;
}

int handle_ipv4(int src, int dst)
{
    char buf;
    char addr[IPV4_SIZE];
    char port[PORT_SIZE];

    ensure(recv(src, addr, IPV4_SIZE, 0) > 0, NULL); // >> DST_ADDR
    ensure(recv(src, port, PORT_SIZE, 0) > 0, NULL); // >> DST_PORT

    ensure(send(dst, "\x01", 1, 0) > 0, "ATYP --> [remote]");
    ensure(send(dst, addr, IPV4_SIZE, 0) > 0, "DST_ADDR --> [remote]");
    ensure(send(dst, port, PORT_SIZE, 0) > 0, "DST_PORT --> [remote]");

    ensure(recv(dst, &buf, 1, 0) > 0, "ATYP <-- [remote]");
    ensure(recv(dst, addr, IPV4_SIZE, 0) > 0, "BND_ADDR <-- [remote]");
    ensure(recv(dst, port, PORT_SIZE, 0) > 0, "BND_PORT <-- [remote]");

    ensure(send(src, "\x05\x00\x00\x01", 4, 0) > 0, NULL); // << VER|ACK|RSV|ATYP
    ensure(send(src, addr, IPV4_SIZE, 0) > 0, NULL);       // << BND_ADDR
    ensure(send(src, port, PORT_SIZE, 0) > 0, NULL);       // << BND_PORT
    return 0;

error:
    return -1;
}

int handle_ipv6(int src, int dst)
{
    char buf;
    char addr[IPV6_SIZE];
    char port[PORT_SIZE];

    ensure(recv(src, addr, IPV6_SIZE, 0) > 0, NULL); // >> DST_ADDR
    ensure(recv(src, port, PORT_SIZE, 0) > 0, NULL); // >> DST_PORT

    ensure(send(dst, "\x04", 1, 0) > 0, "ATYP --> [remote]");
    ensure(send(dst, addr, IPV6_SIZE, 0) > 0, "DST_ADDR --> [remote]");
    ensure(send(dst, port, PORT_SIZE, 0) > 0, "DST_PORT --> [remote]");

    ensure(recv(dst, &buf, 1, 0) > 0, "ATYP <-- [remote]");
    ensure(recv(dst, addr, IPV6_SIZE, 0) > 0, "BND_ADDR <-- [remote]");
    ensure(recv(dst, port, PORT_SIZE, 0) > 0, "BND_PORT <-- [remote]");

    ensure(send(src, "\x05\x00\x00\x04", 4, 0) > 0, NULL); // << VER|ACK|RSV|ATYP
    ensure(send(src, addr, IPV6_SIZE, 0) > 0, NULL);       // << BND_ADDR
    ensure(send(src, port, PORT_SIZE, 0) > 0, NULL);       // << BND_PORT
    return 0;

error:
    return -1;
}

int handle_hostname(int src, int dst)
{
    unsigned char buf;
    char addr[256] = {};
    char port[PORT_SIZE];

    ensure(recv(src, &buf, 1, 0) > 0 && buf > 0, NULL); // >> N_ADDR
    ensure(recv(src, addr, buf, 0) > 0, NULL);          // >> DST_ADDR
    ensure(recv(src, port, PORT_SIZE, 0) > 0, NULL);    // >> DST_PORT

    ensure(send(dst, "\x03", 1, 0) > 0, "ATYP --> [remote]");
    ensure(send(dst, &buf, 1, 0) > 0, "N_ADDR --> [remote]");
    ensure(send(dst, addr, buf, 0) > 0, "DST_ADDR --> [remote]");
    ensure(send(dst, port, PORT_SIZE, 0) > 0, "DST_PORT --> [remote]");

    ensure(recv(dst, &buf, 1, 0) > 0, "ATYP(v4) <-- [remote]");
    ensure(recv(dst, addr, IPV4_SIZE, 0) > 0, "BND_ADDR(v4) <-- [remote]");
    ensure(recv(dst, port, PORT_SIZE, 0) > 0, "BND_PORT <-- [remote]");

    ensure(send(src, "\x05\x00\x00\x01", 4, 0) > 0, NULL); // << VER|ACK|RSV|ATYP(v4)
    ensure(send(src, addr, IPV4_SIZE, 0) > 0, NULL);       // << BND_ADDR
    ensure(send(src, port, PORT_SIZE, 0) > 0, NULL);       // << BND_PORT
    return 0;

error:
    return -1;
}
