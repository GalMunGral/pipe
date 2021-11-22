#include "lib.h"

void *handle(void *);
int handle_by_type(const int, const int);
int handle_ipv4(const int, const int);
int handle_ipv6(const int, const int);
int handle_hostname(const int, const int);

int sock;
short port;
const char *remote_addr;
const char *remote_port;

void cleanup()
{
    close(sock);
}

int main(const int argc, const char **argv)
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

    while (1)
        spawn_worker(handle, accept(sock, NULL, NULL));

    return 0;
}

void *handle(void *arg)
{
    int pair[2] = {*(int *)arg, -1};
    free(arg);

    ensure((pair[1] = connect_by_name(remote_addr, remote_port)) > 0, "socket(remote)");

    // 1. HANDSHAKE
    unsigned char buf[256];

    // check that server is alive
    ensure(sendall(pair[1], buf, PAD_SIZE, 0) > 0, "(pad) --> [remote]");
    ensure(recvall(pair[1], buf, PAD_SIZE, 0) > 0, "(pad) <-- [remote]");

    // accept request
    ensure(recvall(pair[0], buf, 2, 0) > 0, "(1) VER+NMETHODS (aborted)");
    ensure(buf[0] == '\x05', "(1) SOCKs version not supported");
    ensure(recvall(pair[0], buf + 2, /* NMETHODS */ buf[1], 0) > 0, "(1) METHODS (aborted)");
    ensure(sendall(pair[0], "\x05\x00", 2, 0) > 0, "(1) VER+METHOD (failed)");

    // 2. THE ACTUAL REQUEST
    ensure((handle_by_type(pair[0], pair[1]) == 0), NULL);
    loop(pair);

error:
    close(pair[0]);
    close(pair[1]);
    return NULL;
}

/**
 * Returns 0 for success and -1 for error
 */
int handle_by_type(const int src, const int dst)
{
    char buf[4];
    ensure(recvall(src, buf, 4, 0) > 0, "(2) VER+CMD+RSV+ATYP (aborted)");
    ensure(buf[0] == '\x05', "(2) SOCKs version not supported");
    ensure(buf[1] == '\x01', "(2) SOCKs command not supported");

    switch (buf[3])
    {
    case '\x01':
        return handle_ipv4(src, dst);
    case '\x04':
        return handle_ipv6(src, dst);
    case '\x03':
        return handle_hostname(src, dst);
    default:
        return -1;
    }

error:
    return -1;
}

int handle_ipv4(const int src, const int dst)
{
    unsigned char buf[4 + IPV4_SIZE + PORT_SIZE] = {};
    buf[0] = '\x01';
    ensure(recvall(src, buf + 1, IPV4_SIZE + PORT_SIZE, 0) > 0, "(2) ATYP+DST (aborted)");
    ensure(sendall(dst, buf, 1 + IPV4_SIZE + PORT_SIZE, 0) > 0, "(2) ATYP+DST --> [remote]");
    buf[0] = '\x05'; // VER
    buf[1] = '\x00'; // ACK
    buf[2] = '\x00'; // RSV
    ensure(recvall(dst, buf + 3, 1 + IPV4_SIZE + PORT_SIZE, 0) > 0, "(2) ATYP+BND <-- [remote]");
    ensure(sendall(src, buf, 4 + IPV4_SIZE + PORT_SIZE, 0) > 0, "(2) ATYP+BND (failed)");
    return 0;
error:
    return -1;
}

int handle_ipv6(const int src, const int dst)
{
    unsigned char buf[4 + IPV6_SIZE + PORT_SIZE] = {};
    buf[0] = '\x04';
    ensure(recvall(src, buf + 1, IPV6_SIZE + PORT_SIZE, 0) > 0, "(2) ATYP+DST (aborted)");
    ensure(sendall(dst, buf, 1 + IPV6_SIZE + PORT_SIZE, 0) > 0, "(2) ATYP+DST --> [remote]");
    buf[0] = '\x05'; // VER
    buf[1] = '\x00'; // ACK
    buf[2] = '\x00'; // RSV
    ensure(recvall(dst, buf + 3, 1 + IPV6_SIZE + PORT_SIZE, 0) > 0, "(2) ATYP+BND <-- [remote]");
    ensure(sendall(src, buf, 4 + IPV6_SIZE + PORT_SIZE, 0) > 0, "(2) ATYP+BND (failed)");
    return 0;
error:
    return -1;
}

int handle_hostname(const int src, const int dst)
{
    unsigned char buf[1024] = {};
    buf[0] = '\x03';
    ensure(recvall(src, buf + 1, 1, 0) > 0, "(2) N_ADDR (aborted)");
    ensure(recvall(src, buf + 2, /* N_ADDR */ buf[1] + PORT_SIZE, 0) > 0, "(2) ATYP+DST (aborted)");
    ensure(sendall(dst, buf, 2 + /* N_ADDR */ buf[1] + PORT_SIZE, 0) > 0, "(2) ATYP+DST --> [remote]");
    buf[0] = '\x05'; // VER
    buf[1] = '\x00'; // ACK
    buf[2] = '\x00'; // RSV
    ensure(recvall(dst, buf + 3, 1 + IPV4_SIZE + PORT_SIZE, 0) > 0, "(2) ATYP+BND <-- [remote]");
    ensure(sendall(src, buf, 4 + IPV4_SIZE + PORT_SIZE, 0) > 0, "(2) ATYP+BND (failed)");
    return 0;
error:
    return -1;
}
