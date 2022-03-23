#include <stdlib.h>
#include <assert.h>
#include <uv.h>

#define PADDING_SIZE 13
#define TELES_RES_SIZE 7
#define TS_SOCK_BUF_SIZE 4096

uv_loop_t *loop;

typedef enum
{
    CLOSED = -2,
    HALF_OPEN,
    CLOSING,
    INIT,
    HANDSHAKE_FINISHED,
    CONNECTING,
    CONNECTED,
} ts_state_t;

typedef struct ts_sock_s ts_sock_t;
typedef struct ts_sockpair_s ts_sockpair_t;

typedef void (*ts_sock_handler_t)(ts_sock_t *sock,
                                  ssize_t nread,
                                  const uv_buf_t *buf);

struct ts_sock_s
{
    int offset;
    char buffer[TS_SOCK_BUF_SIZE];
    uv_tcp_t stream;
    ts_sock_t *peer;
    ts_sockpair_t *pair;
    ts_sock_handler_t handler;
};

struct ts_sockpair_s
{
    ts_state_t state;
    ts_sock_t sock1;
    ts_sock_t sock2;
};

ts_sockpair_t *ts_sockpair_init(uv_loop_t *loop)
{
    ts_sockpair_t *sp = malloc(sizeof(ts_sockpair_t));
    memset(sp, 0, sizeof(ts_sockpair_t));
    sp->state = INIT;
    uv_tcp_init(loop, &sp->sock1.stream);
    uv_tcp_init(loop, &sp->sock2.stream);
    sp->sock1.stream.data = (void *)&sp->sock1;
    sp->sock2.stream.data = (void *)&sp->sock2;
    sp->sock1.pair = sp;
    sp->sock2.pair = sp;
    sp->sock1.peer = &sp->sock2;
    sp->sock2.peer = &sp->sock1;
    return sp;
}

void on_sockpair_close(uv_handle_t *handle)
{
    uv_stream_t *stream = (uv_stream_t *)handle;
    ts_sock_t *sock = (ts_sock_t *)stream->data;
    ts_sockpair_t *sp = sock->pair;
    if (--sp->state == CLOSED)
    {
        free(sp);
    }
}

void ts_sockpair_deinit(ts_sockpair_t *sp)
{
    sp->state = CLOSING;
    uv_close((uv_handle_t *)&sp->sock1.stream, on_sockpair_close);
    uv_close((uv_handle_t *)&sp->sock2.stream, on_sockpair_close);
}

void alloc_buffer(uv_handle_t *handle,
                  size_t suggested_size,
                  uv_buf_t *buf)
{
    *buf = uv_buf_init((char *)malloc(suggested_size), suggested_size);
}

void ts_read(uv_stream_t *stream,
             ssize_t nread,
             const uv_buf_t *buf)
{
    ts_sock_t *s = (ts_sock_t *)stream->data;
    memcpy(s->buffer + s->offset, buf->base, nread);
    s->offset += nread;
    free(buf->base);
    // free((void *)buf); // not sure
}

typedef struct ts_write_s
{
    uv_write_t req;
    uv_buf_t data;
} ts_write_t;

ts_write_t *ts_write_init(ssize_t size)
{
    ts_write_t *w = malloc(sizeof(ts_write_t));
    w->req.data = w; // free struct in `on_write`;
    w->data = uv_buf_init(malloc(size), size);
    return w;
}

void on_write(uv_write_t *req, int status)
{
    if (status < 0)
    {
        fprintf(stderr, "on_write (%s)\n", uv_strerror(status));
    }
    ts_write_t *w = (ts_write_t *)req->data;
    free(w->data.base);
    free(w);
}

void on_read(uv_stream_t *stream,
             ssize_t nread,
             const uv_buf_t *buf)
{
    ts_sock_t *s = (ts_sock_t *)stream->data;
    s->handler(s, nread, buf);
}

void forward_to_peer(ts_sock_t *s,
                     ssize_t nread,
                     const uv_buf_t *buf)
{
    if (nread == 0)
        return;
    if (nread < 0)
    {
        ts_sockpair_deinit(s->pair);
        if (nread != UV_EOF)
        {
            fprintf(stderr, "on_tunnel_read (%s)\n", uv_err_name(nread));
        }
        return;
    }
    ts_write_t *w = ts_write_init(nread);
    memcpy(w->data.base, buf->base, nread);
    uv_write(&w->req, (uv_stream_t *)&s->peer->stream, &w->data, 1, on_write);
}

void on_server_connect(uv_connect_t *req, int status)
{
    ts_sockpair_t *sp = (ts_sockpair_t *)req->data;

    if (status < 0)
    {
        fprintf(stderr, "on_server_connect (%s)\n", uv_err_name(status));
        if (status != UV_ECANCELED)
        {
            ts_sockpair_deinit(sp);
        }
        return;
    }

    // Connection establised
    sp->sock1.handler = forward_to_peer;
    sp->sock2.handler = forward_to_peer;
    uv_read_start((uv_stream_t *)&sp->sock2.stream, alloc_buffer, on_read);

    int len;
    struct sockaddr_in addr; // sockaddr_storage ?
    uv_tcp_getsockname(&sp->sock2.stream, (struct sockaddr *)&addr, &len);

    ts_write_t *w = ts_write_init(TELES_RES_SIZE);
    w->data.base[0] = '\x01'; // IPv4;
    memcpy(w->data.base + 1, &addr.sin_addr, 4);
    memcpy(w->data.base + 5, &addr.sin_port, 2);

    uv_write(&w->req, (uv_stream_t *)&sp->sock1.stream, &w->data, 1, on_write);

    free(req);
}

void on_resolved(uv_getaddrinfo_t *resolver, int status, struct addrinfo *res)
{
    ts_sockpair_t *sp = (ts_sockpair_t *)resolver->data;

    if (status < 0)
    {
        fprintf(stderr, "on_resolved (%s)\n", uv_err_name(status));
        if (status != UV_ECANCELED)
        {
            ts_sockpair_deinit(sp);
        }
        return;
    }

    if (!res)
    {
        fprintf(stderr, "on_resolved (no result)\n");
        ts_sockpair_deinit(sp);
        return;
    }

    uv_connect_t *req = malloc(sizeof(uv_connect_t));
    req->data = (void *)sp;
    uv_tcp_connect(req, &sp->sock2.stream, (struct sockaddr *)res->ai_addr, on_server_connect);

    free(resolver);
    uv_freeaddrinfo(res);
}

void resolve_dns(ts_sock_t *s)
{
    char name[256] = {'\0'};
    char port[16] = {'\0'};
    int naddr = s->buffer[1];
    memcpy(name, s->buffer + 2, naddr);
    strcpy(port, s->buffer + 2 + naddr);

    struct addrinfo hints;
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    uv_getaddrinfo_t *resolver = malloc(sizeof(uv_getaddrinfo_t));
    resolver->data = (void *)s->pair;

    int r;
    if ((r = uv_getaddrinfo(loop, resolver, on_resolved, name, port, &hints)))
    {
        fprintf(stderr, "uv_getaddrinfo (%s)\n", uv_err_name(r));
        free(resolver);
        ts_sockpair_deinit(s->pair);
    }
}

void on_client_read(ts_sock_t *s,
                    ssize_t nread,
                    const uv_buf_t *buf)
{
    if (nread == 0)
        return;
    if (nread < 0)
    {
        ts_sockpair_deinit(s->pair);
        if (nread != UV_EOF)
        {
            fprintf(stderr, "on_tunnel_read (%s)\n", uv_err_name(nread));
        }
        return;
    }
    ts_read((uv_stream_t *)&s->stream, nread, buf);

    switch (s->pair->state)
    {
    case INIT:
    {
        if (s->offset < PADDING_SIZE)
            return;
        if (s->offset > PADDING_SIZE)
        {
            fprintf(stderr, "on_client_read/INIT (padding too long)\n");
            ts_sockpair_deinit(s->pair);
            return;
        }
        ts_write_t *w = ts_write_init(PADDING_SIZE);
        uv_write(&w->req, (uv_stream_t *)&s->stream, &w->data, 1, on_write);
        s->pair->state = HANDSHAKE_FINISHED;
        s->offset = 0;
        break;
    }
    case HANDSHAKE_FINISHED:
    {
        // +------+-------+------+------+
        // |  0   |   1   | ...  | ...  |
        // +------+-------+------+------+
        // | ATYP | NADDR | NAME | PORT |
        // +------+-------+------+------+

        if (s->offset < 2)
            return;
        if (s->buffer[0] != '\x03')
        {
            fprintf(stderr, "on_client_read/HANDSHAKE_FINISHED (raw ip not supported)\n");
            ts_sockpair_deinit(s->pair);
            return;
        }

        int naddr = s->buffer[1];
        int frame_size = naddr + 4;
        if (s->offset < frame_size)
            return;
        if (s->offset > frame_size)
        {
            fprintf(stderr, "on_client_read/HANDSHAKE_FINISHED (address too long)\n");
            ts_sockpair_deinit(s->pair);
            return;
        }

        char *port = s->buffer + frame_size - 2;
        sprintf(port, "%hu", ntohs(*(unsigned short *)port)); // hmmm...
        s->pair->state = CONNECTING;
        resolve_dns(s);
        break;
    }
    default:
        fprintf(stderr, "on_client_read/default (invalid state/input)");
        ts_sockpair_deinit(s->pair);
    }
}

void on_client_connection(uv_stream_t *server, int status)
{
    if (status < 0)
    {
        fprintf(stderr, "on_client_connection (%s)\n", uv_strerror(status));
        return;
    }

    ts_sockpair_t *sp = ts_sockpair_init(loop);
    if (uv_accept(server, (uv_stream_t *)&sp->sock1.stream))
    {
        ts_sockpair_deinit(sp);
        return;
    }

    sp->sock1.handler = on_client_read;
    uv_read_start((uv_stream_t *)&sp->sock1.stream, alloc_buffer, on_read);
}

int main(int argc, const char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: uv_remote [port]\n");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    loop = malloc(sizeof(uv_loop_t));
    uv_loop_init(loop);

    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", atoi(argv[1]), &addr);

    uv_tcp_t server;
    uv_tcp_init(loop, &server);
    uv_tcp_bind(&server, (const struct sockaddr *)&addr, 0);

    int r;
    if ((r = uv_listen((uv_stream_t *)&server, 10, on_client_connection)))
    {
        fprintf(stderr, "main/uv_listen (%s)\n", uv_strerror(r));
        return 1;
    }

    if (uv_run(loop, UV_RUN_DEFAULT))
    {
        fprintf(stderr, "main/uv_run (%s)\n", uv_strerror(r));
        return 1;
    }

    uv_loop_close(loop);
    free(loop);
    return 0;
}
