#include "lib.h"

void *handle(void *);

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

    // printf("listening on %hd\n", port);

    while (1)
        spawn_worker(handle, accept(sock, NULL, NULL));

    return 0;
}

void *handle(void *arg)
{
    int src = *(int *)arg, dst; 
    free(arg);
    ensure((dst = connect_by_name(remote_addr, remote_port)) > 0, "[HTTP] socket(remote)");

    char buf[4096] = {0};
    char buf2[4096];

    ensure(sendall(dst, "\x05\x02", 2, 0) > 0, "[HTTP-SOCKS5] (1) VER+NMETHODS (aborted)");
    ensure(sendall(dst, "\x00\x01", 2, 0) > 0, "[HTTP-SOCSK5](1) METHODS (aborted)");
    ensure(recvall(dst, buf2, 2, 0) > 0, "[HTTP-SOCKS5] (1) VER+METHOD");

    ensure(recv(src, buf, 4096, 0), "[HTTP] Initial Request");

    // printf("----------\n%s", buf);
    
    char host[256] = {0};
    unsigned short port;

    char *p1 = buf, *p2 = buf;
    for (; *p2 && *p2 == ' '; ++p1, ++p2);

    if (0 == strncmp(p2, "GET", 3))  { // HTTP
        p2 = strstr(p2, "//");
        ensure(p2 > p1, "Cannot find //");

        for (p1 = p2 += 2; *p2 && *p2 != ' ' && *p2 != '/'; ++p2);
        strncpy(host, p1, p2 - p1);
        // printf("host: %s\n", host);
        
        port = htons(80);

        char len = strlen(host);
        ensure(sendall(dst, "\x05\x01\x00\x03", 4, 0) > 0, "[HTTP-SOCSK5] (2) VER+CMD+RSV+ATYP");
        ensure(sendall(dst, &len, 1, 0) > 0, "[HTTP-SOCSK5] (2) n_addr");
        ensure(sendall(dst, host, len, 0) > 0, "[HTTP-SOCSK5] (2) atyp+dst_addr");
        ensure(sendall(dst, &port, 2, 0) > 0, "[HTTP-SOCSK5] (2) atyp+dst_port");
        ensure(recvall(dst, buf2, 4 + IPV4_SIZE + PORT_SIZE, 0) > 0, "[HTTP-SOCSK5] (2) atyp+bnd");
        
        // Forward the first HTTP request
        ensure(sendall(dst, buf, strlen(buf), 0) > 0, "buf");
    } else if (0 == strncmp(p2, "CONNECT", 7)) { // HTTPS
        for (p2 += 7; *p2 && *p2 == ' '; ++p2);
        for (p1 = p2; *p2 && *p2 != ' ' && *p2 != ':'; ++p2);
        ensure(*p2 == ':', "`:` not found");
        
        strncpy(host, p1, p2 - p1);
        // printf("host: %s\n", host);


        char port_buf[10];
        for (p1 = ++p2; *p2 && *p2 != ' '; ++p2);
        strncpy(port_buf, p1, p2 - p1);
        // printf("port: %s\n\n", port_buf);

        port = htons(atoi(port_buf));

        char len = strlen(host);

        ensure(sendall(dst, "\x05\x01\x00\x03", 4, 0) > 0, "[HTTP-SOCSK5] (2) VER+CMD+RSV+ATYP");
        ensure(sendall(dst, &len, 1, 0) > 0, "[HTTP-SOCSK5] (2) n_addr");
        ensure(sendall(dst, host, len, 0) > 0, "[HTTP-SOCSK5] (2) atyp+dst_addr");
        ensure(sendall(dst, &port, 2, 0) > 0, "[HTTP-SOCSK5] (2) atyp+dst_port");
        ensure(recvall(dst, buf2, 4 + IPV4_SIZE + PORT_SIZE, 0) > 0, "[HTTP-SOCKS5] (2) atyp+bnd");

        char* res = "HTTP/1.1 200 OK\n\n";
        ensure(sendall(src, res, strlen(res), 0) > 0, "res");
    } else {
        goto error;
    }

    int pair[] = {src, dst};
    loop(pair);
error:
    close(src);
    close(dst);
    return NULL;
}

