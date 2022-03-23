#include <stdlib.h>
#include <assert.h>
#include <uv.h>

#define PADDING_SIZE 13
#define TELES_RES_SIZE 7
#define SOCKS_RES_SIZE 10
#define TS_SOCK_BUF_SIZE 4096

uv_loop_t *loop;

const char *remote_addr;
const char *remote_port;

typedef enum
{
    ST_SOCKS,
    ST_TELESCOPE,
} sock_tag_t;

typedef enum
{
    INIT,
    SOCKS_HANDSHAKE_REQUESTED,
    TUNNEL_HANDSHAKE_FINISHED,
    READY,
    PROXY_REQUEST_SENT,
} sockpair_state_t;

typedef struct ts_sock_s ts_sock_t;
typedef struct ts_sockpair_s ts_sockpair_t;

typedef void (*ts_sock_handler_t)(ts_sock_t *sock,
                                  ssize_t nread,
                                  const uv_buf_t *buf);

struct ts_sock_s
{
    int offset;
    char buffer[TS_SOCK_BUF_SIZE];
    sock_tag_t tag;
    uv_tcp_t stream;
    ts_sock_t *peer;
    ts_sockpair_t *pair;
    ts_sock_handler_t handler;
};

struct ts_sockpair_s
{
    sockpair_state_t state;
    ts_sock_t sock1;
    ts_sock_t sock2;
};

ts_sockpair_t *ts_sockpair_init(uv_loop_t *loop)
{
    ts_sockpair_t *sp = malloc(sizeof(ts_sockpair_t));
    memset(sp, 0, sizeof(ts_sockpair_t));
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

void ts_sockpair_deinit(ts_sockpair_t *pair)
{
    uv_close((uv_handle_t *)&pair->sock1.stream, NULL);
    uv_close((uv_handle_t *)&pair->sock2.stream, NULL);
    free(pair);
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
        return;
    }
    ts_write_t *w = (ts_write_t *)req->data;
    free(w->data.base);
    free(w);
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

void finish_handshake(ts_sockpair_t *sp)
{
    ts_write_t *w = ts_write_init(2);
    memcpy(w->data.base, "\x05\x00", 2);
    uv_write(&w->req, (uv_stream_t *)&sp->sock1.stream, &w->data, 1, on_write);
    sp->sock1.offset = 0;
    sp->sock2.offset = 0;
    sp->state = READY;
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
        if (s->offset < 2)
            return;
        if (s->buffer[0] != '\x05')
        {
            fprintf(stderr, "on_client_read/INIT (socks version)\n");
            ts_sockpair_deinit(s->pair);
            return;
        }

        unsigned char nmethods = (unsigned char)s->buffer[1];
        if (s->offset < 2 + nmethods)
            return;
        if (s->offset > 2 + nmethods)
        {
            fprintf(stderr, "on_client_read/INIT (nmethods doesn't match)\n");
            ts_sockpair_deinit(s->pair);
            return;
        }

        s->pair->state = SOCKS_HANDSHAKE_REQUESTED;
        break;
    }
    case TUNNEL_HANDSHAKE_FINISHED:
    {
        if (s->offset < 2)
            return;
        if (s->buffer[0] != '\x05')
        {
            fprintf(stderr, "on_client_read/INIT (socks version)\n");
            ts_sockpair_deinit(s->pair);
            return;
        }

        int nmethods = s->buffer[1];
        if (s->offset < 2 + nmethods)
            return;
        if (s->offset > 2 + nmethods)
        {
            fprintf(stderr, "on_client_read/INIT (nmethods doesn't match)\n");
            ts_sockpair_deinit(s->pair);
            return;
        }

        finish_handshake(s->pair);
        break;
    }
    case READY:
    {
        // +-----+-----+-----+------+-------+------+------+
        // |  0  |  1  |  2  |  3   |   4   | ...  | ...  |
        // +-----+-----+-----+------+-------+------+------+
        // | VER | CMD | RSV | ATYP | NADDR | NAME | PORT |
        // +-----+-----+-----+------+-------+------+------+

        if (s->offset < 5)
            return;
        if (s->buffer[0] != '\x05')
        {
            fprintf(stderr, "on_client_read/READY (socks version)\n");
            ts_sockpair_deinit(s->pair);
            return;
        }
        if (s->buffer[1] != '\x01')
        {
            fprintf(stderr, "on_client_read/READY (socks command)\n");
            ts_sockpair_deinit(s->pair);
            return;
        }
        if (s->buffer[3] != '\x03')
        {
            fprintf(stderr, "on_client_read/READY (address type)\n");
            ts_sockpair_deinit(s->pair);
            return;
        }

        unsigned char naddr = (unsigned char)s->buffer[4];
        if (s->offset < 5 + naddr + 2)
            return;

        int frame_size = naddr + 4;
        ts_write_t *w = ts_write_init(frame_size);
        memcpy(w->data.base, s->buffer + 3, frame_size);
        uv_write(&w->req, (uv_stream_t *)&s->peer->stream, &w->data, 1, on_write);
        s->pair->state = PROXY_REQUEST_SENT;
        break;
    }
    default:
        fprintf(stderr, "on_client_read/default (invalid state/input)");
        ts_sockpair_deinit(s->pair);
    }
}

void on_tunnel_read(ts_sock_t *s,
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
            fprintf(stderr, "on_tunnel_read/INIT (padding too long)\n");
            ts_sockpair_deinit(s->pair);
            return;
        }
        s->pair->state = TUNNEL_HANDSHAKE_FINISHED;
        break;
    }
    case SOCKS_HANDSHAKE_REQUESTED:
    {
        if (s->offset < PADDING_SIZE)
            return;
        if (s->offset > PADDING_SIZE)
        {
            fprintf(stderr, "on_tunnel_read/SOCKS_HANDSHAKE_REQUESTED (padding too long)\n");
            ts_sockpair_deinit(s->pair);
            return;
        }
        finish_handshake(s->pair);
        break;
    }
    case PROXY_REQUEST_SENT:
    {
        if (s->offset < TELES_RES_SIZE)
            return;
        if (s->offset > TELES_RES_SIZE)
        {
            fprintf(stderr, "on_tunnel_read/PROXY_REQUEST_SENT (address too long)\n");
            ts_sockpair_deinit(s->pair);
            return;
        }

        // Connection established, just pipe data now.
        s->handler = s->peer->handler = forward_to_peer;

        ts_write_t *w = ts_write_init(SOCKS_RES_SIZE);
        memcpy(w->data.base, "\x05\x00\x00", 3);             // VER+ACK+RSV
        memcpy(w->data.base + 3, s->buffer, TELES_RES_SIZE); // ATYP+IP4+PORT
        uv_write(&w->req, (uv_stream_t *)&s->peer->stream, &w->data, 1, on_write);
        break;
    }
    default:
        fprintf(stderr, "on_tunnel_read/default (invalid state/input)");
        ts_sockpair_deinit(s->pair);
    }
}

void on_read(uv_stream_t *stream,
             ssize_t nread,
             const uv_buf_t *buf)
{
    ts_sock_t *s = (ts_sock_t *)stream->data;
    s->handler(s, nread, buf);
}

void on_tunnel_connected(uv_connect_t *req, int status)
{
    ts_sockpair_t *sp = (ts_sockpair_t *)req->data;

    sp->sock1.handler = on_client_read;
    sp->sock2.handler = on_tunnel_read;
    uv_read_start((uv_stream_t *)&sp->sock1.stream, alloc_buffer, on_read);
    uv_read_start((uv_stream_t *)&sp->sock2.stream, alloc_buffer, on_read);

    ts_write_t *w = ts_write_init(PADDING_SIZE);
    uv_write(&w->req, (uv_stream_t *)&sp->sock2.stream, &w->data, 1, on_write);

    free(req);
}

void tunnel_connect(ts_sockpair_t *sp)
{
    struct sockaddr_in addr;
    uv_ip4_addr(remote_addr, atoi(remote_port), &addr);

    uv_connect_t *req = malloc(sizeof(uv_connect_t));
    req->data = (void *)sp;
    uv_tcp_connect(req, &sp->sock2.stream, (struct sockaddr *)&addr, on_tunnel_connected);
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
    tunnel_connect(sp);
}

int main(int argc, const char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage: uv_remote [port] [remote-ip] [remote-port]\n");
        return 1;
    }
    remote_addr = argv[2];
    remote_port = argv[3];

    signal(SIGPIPE, SIG_IGN);

    loop = malloc(sizeof(uv_loop_t));
    uv_loop_init(loop);

    uv_tcp_t server;
    struct sockaddr_in addr;
    int port = atoi(argv[1]);

    uv_tcp_init(loop, &server);
    uv_ip4_addr("0.0.0.0", port, &addr);
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
