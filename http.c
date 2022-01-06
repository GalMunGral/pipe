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
    int src = *(int *)arg, dst; 

    free(arg);
    
    ensure((dst = connect_by_name(remote_addr, remote_port)) > 0, "socket(remote)");

    char buf[4096] = {0};
    char buf2[4096];

    ensure(sendall(dst, "\x05\x02", 2, 0) > 0, "(1) VER+NMETHODS (aborted)");
    ensure(sendall(dst, "\x00\x01", 2, 0) > 0, "(1) METHODS (aborted)");
    ensure(recvall(dst, buf2, 2, 0) > 0, "(1) VER+METHOD");

    int i = 0;

    ensure(recv(src, buf, 4096, 0), "recv initial");
    
    while (!buf[i]) ++i;
    
    char host[50] = {0};
    unsigned short port;

    if (0 == strncmp(buf, "GET", 3))  {
        // for (i += 3; buf[i] && buf[i] == ' '; ++i);
        i = strstr(buf, "//") - buf + 2;
        printf("GET\n");

        int j;
        for (j = i; buf[j] && buf[j] != ' ' && buf[j] != '/'; ++j);
        strncpy(host, buf + i, j - i);
        printf("HOST: %s\n", host);

        port = htons(80);

        char len = strlen(host);

        ensure(sendall(dst, "\x05\x01\x00\x03", 4, 0) > 0, "(2) VER+CMD+RSV+ATYP (aborted)");
        ensure(sendall(dst, &len, 1, 0) > 0, "(2) n_addr (aborted)");
        ensure(sendall(dst, host, len, 0) > 0, "(2) atyp+dst_addr (aborted)");
        ensure(sendall(dst, &port, 2, 0) > 0, "(2) atyp+dst_port (aborted)");
        ensure(recvall(dst, buf2, 4 + IPV4_SIZE + PORT_SIZE, 0) > 0, "(2) atyp+bnd (failed)");


        ensure(sendall(dst, buf, strlen(buf), 0) > 0, "buf");

    } else if (0 == strncmp(buf, "CONNECT", 7)) {
        for (i += 7; buf[i] && buf[i] == ' '; ++i);
        printf("CONNECT\n");

        int j;
        for (j = i; buf[j] && buf[j] != ' ' && buf[j] != ':'; ++j);
        strncpy(host, buf + i, j - i);
        printf("HOST: %s\n", host);

        char port_buf[10];
        ++j;
        int k;
        for (k = j; buf[k] && buf[k] != ' '; ++k);
        strncpy(port_buf, buf + j, k - j);
        printf("PORT: %s\n", port_buf);

        port = htons(atoi(port_buf));
        printf("DONE");

        char len = strlen(host);
        char* res = "HTTP/1.1 200 OK\n\n";

        ensure(sendall(dst, "\x05\x01\x00\x03", 4, 0) > 0, "(2) VER+CMD+RSV+ATYP (aborted)");
        ensure(sendall(dst, &len, 1, 0) > 0, "(2) n_addr (aborted)");
        ensure(sendall(dst, host, len, 0) > 0, "(2) atyp+dst_addr (aborted)");
        ensure(sendall(dst, &port, 2, 0) > 0, "(2) atyp+dst_port (aborted)");
        ensure(recvall(dst, buf2, 4 + IPV4_SIZE + PORT_SIZE, 0) > 0, "(2) atyp+bnd (failed)");


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

