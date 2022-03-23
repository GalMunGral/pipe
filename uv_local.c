#include <stdlib.h>
#include <assert.h>
#include <uv.h>

#define PADDING_SIZE 13

const char *peer_addr;
const char *peer_port;
uv_loop_t *loop;

void alloc_buffer(uv_handle_t *handle,
                  size_t suggested_size,
                  uv_buf_t *buf)
{
    *buf = uv_buf_init((char *)malloc(suggested_size), suggested_size);
}

typedef enum conn_state
{
    UNSPEC = -1,
    INIT = 0,
    SOCKS_HANDSHAKE,
    PEER_ACK,
    READY,
    REQUEST,
} conn_state_t;

typedef enum sock_type
{
    SOCK_CLIENT,
    SOCK_PEER,
} sock_type_t;

typedef struct
{
    enum conn_state state;
    unsigned char buffer[4096];
    int offset;
    char name[1024];
    char port[512];
    uv_stream_t *peer;
    void (*handler)(uv_stream_t *client,
                    ssize_t nread,
                    const uv_buf_t *buf);
} context_t;

enum sock_type get_type(uv_stream_t *sock)
{
    context_t *ctx = (context_t *)sock->data;
    return (ctx->state == UNSPEC) ? SOCK_PEER : SOCK_CLIENT;
}

uv_stream_t *get_peer(uv_stream_t *sock)
{
    context_t *ctx = (context_t *)sock->data;
    return ctx->peer;
}

context_t *get_peer_context(uv_stream_t *sock)
{
    context_t *ctx = (context_t *)sock->data;
    uv_stream_t *peer = ctx->peer;
    return (context_t *)peer->data;
}

void shutdown_sockpair(uv_stream_t *sock)
{
    uv_close((uv_handle_t *)sock, NULL);
    uv_close((uv_handle_t *)get_peer(sock), NULL);
}

enum conn_state
get_state(uv_stream_t *sock)
{
    context_t *ctx = (context_t *)sock->data;
    if (ctx->state == UNSPEC)
    {
        ctx = (context_t *)ctx->peer->data;
    }
    return ctx->state;
}

void set_state(uv_stream_t *sock, conn_state_t state)
{
    context_t *ctx = (context_t *)sock->data;
    if (ctx->state == UNSPEC)
    {
        ctx = (context_t *)ctx->peer->data;
    }
    ctx->state = state;
}

context_t *create_context()
{
    context_t *ctx = malloc(sizeof(context_t));
    memset(ctx, 0, sizeof(context_t));
    return ctx;
}

void on_write(uv_write_t *req, int status)
{
    if (status < 0)
    {
        fprintf(stderr, "on_write callback error %s\n", uv_err_name(status));
        return;
    }
    else
    {
        // printf("SUCCESS\n");
    }
}

void forward_to_peer(uv_stream_t *socket,
                     ssize_t nread,
                     const uv_buf_t *buf)
{
    if (nread < 0)
    {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        // uv_close((uv_handle_t *)socket, NULL);
        // uv_close((uv_handle_t *)((context_t *)socket->data)->peer, NULL);
        return;
    }
    // printf("FORWARD\n");
    assert(socket);
    context_t *ctx = (context_t *)socket->data;
    assert(ctx);
    uv_stream_t *peer = ctx->peer;
    assert(peer);
    uv_write_t *req = malloc(sizeof(uv_write_t));
    // printf("size:%zd\n", nread);
    uv_buf_t *out_buf = malloc(sizeof(uv_buf_t));
    *out_buf = uv_buf_init(malloc(nread), nread);
    memcpy(out_buf->base, buf->base, nread);
    uv_write(req, peer, out_buf, 1, on_write);
}

void buffer(uv_stream_t *client,
            ssize_t nread,
            const uv_buf_t *buf)
{
    context_t *ctx = (context_t *)client->data;
    memcpy(ctx->buffer + ctx->offset, buf->base, nread);
    ctx->offset += nread;
    // free(buf->base);
}

void handle_client(uv_stream_t *sock,
                   ssize_t nread,
                   const uv_buf_t *buf)
{
    if (nread < 0)
    {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t *)sock, NULL);
        uv_close((uv_handle_t *)get_peer(sock), NULL);
        return;
    }

    if (nread == 0)
    {
        printf("uh oh = 0\n");
        return;
    }

    context_t *ctx = (context_t *)sock->data;

    buffer(sock, nread, buf);

    conn_state_t state = get_state(sock);
    switch (state)
    {
    case INIT:
    {

        if (ctx->offset < 1)
            return;
        if (ctx->buffer[0] != '\x05')
        {
            fprintf(stderr, "(1) SOCKs version not supported");
            shutdown_sockpair(sock);
            return;
        }

        if (ctx->offset < 2)
            return;

        int nmethods = ctx->buffer[1];

        if (ctx->offset < 2 + nmethods)
            return;

        if (ctx->offset > 2 + nmethods)
        {
            fprintf(stderr, "received more methods");
            shutdown_sockpair(sock);
            return;
        }
        set_state(sock, SOCKS_HANDSHAKE);
        break;
    }
    case PEER_ACK:
    {

        if (ctx->offset < 1)
            return;
        if (ctx->buffer[0] != '\x05')
        {
            fprintf(stderr, "(1) SOCKs version not supported");
            shutdown_sockpair(sock);
            return;
        }

        if (ctx->offset < 2)
            return;

        int nmethods = ctx->buffer[1];

        if (ctx->offset < 2 + nmethods)
            return;

        if (ctx->offset > 2 + nmethods)
        {
            fprintf(stderr, "received more methods");
            shutdown_sockpair(sock);
            return;
        }

        uv_write_t *write_req = malloc(sizeof(uv_write_t));
        uv_buf_t *out_buf = malloc(sizeof(uv_buf_t));
        *out_buf = uv_buf_init(strdup("\x05\x00"), 2);
        uv_write(write_req, sock, out_buf, 1, NULL);
        get_peer_context(sock)->offset = 0;
        set_state(sock, READY);
        break;
    }
    case READY:
    {

        if (ctx->buffer[0] != '\x05')
        {
            fprintf(stderr, "(2) SOCKs version not supported");
            shutdown_sockpair(sock);
            return;
        }
        if (ctx->offset < 2)
            return;
        if (ctx->buffer[1] != '\x01')
        {
            fprintf(stderr, "(2) SOCKs command not supported");
            shutdown_sockpair(sock);
            return;
        }
        if (ctx->offset < 4)
            return;

        if (ctx->buffer[3] != '\x03')
        {
            fprintf(stderr, "IPV4/6 not supported");
            shutdown_sockpair(sock);
            return;
        }
        if (ctx->offset < 5)
            return;

        unsigned char naddr = ctx->buffer[4];

        if (ctx->offset < 5 + naddr + 2)
            return;

        int size = 2 + naddr + 2;
        uv_buf_t *out_buf = malloc(sizeof(uv_buf_t));
        *out_buf = uv_buf_init(malloc(size), size);
        memcpy(out_buf->base, ctx->buffer + 3, 2 + naddr + 2);

        uv_write_t *write_req = malloc(sizeof(uv_write_t));
        uv_write(write_req, get_peer(sock), out_buf, 1, NULL);

        set_state(sock, REQUEST);
        break;
    }
    default:
        fprintf(stderr, "not expected %d", state);
        shutdown_sockpair(sock);
    }
}

void handle_peer(uv_stream_t *sock,
                 ssize_t nread,
                 const uv_buf_t *buf)
{
    if (nread < 0)
    {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t *)sock, NULL);
        uv_close((uv_handle_t *)get_peer(sock), NULL);
        return;
    }

    if (nread == 0)
    {
        printf("uh oh = 0\n");
        return;
    }

    context_t *ctx = (context_t *)sock->data;
    buffer(sock, nread, buf);

    conn_state_t state = get_state(sock);
    switch (state)
    {
    case INIT:
    {
        if (ctx->offset < 13)
            return;
        if (ctx->offset > 13)
        {
            fprintf(stderr, "[SH] shouldn't send more data!!");
            shutdown_sockpair(sock);
            return;
        }
        ctx->offset = 0;
        set_state(sock, PEER_ACK);
        break;
    }
    case SOCKS_HANDSHAKE:
    {
        if (ctx->offset < 13)
            return;
        if (ctx->offset > 13)
        {
            fprintf(stderr, "[SH] shouldn't send more data!!");
            shutdown_sockpair(sock);
            return;
        }
        ctx->offset = 0;
        // ACK no auth
        uv_write_t *write_req = malloc(sizeof(uv_write_t));
        uv_buf_t *out_buf = malloc(sizeof(uv_buf_t));
        *out_buf = uv_buf_init(strdup("\x05\x00"), 2);
        uv_write(write_req, get_peer(sock), out_buf, 1, NULL);
        get_peer_context(sock)->offset = 0;
        set_state(sock, READY);
        break;
    }
    case REQUEST:
    {
        if (ctx->offset < 7)
            return;
        if (ctx->offset > 7)
        {
            fprintf(stderr, "[RQ] shouldn't send more data!!");
            shutdown_sockpair(sock);
            return;
        }
        uv_buf_t *out_buf = malloc(sizeof(uv_buf_t));
        *out_buf = uv_buf_init(malloc(10), 10);
        out_buf->base[0] = '\x05'; // VER
        out_buf->base[1] = '\x00'; // ACK
        out_buf->base[2] = '\x00'; // RSV
        memcpy(out_buf->base + 3, ctx->buffer, 7);

        // Communication established
        ctx->handler = forward_to_peer;
        ((context_t *)ctx->peer->data)->handler = forward_to_peer;

        uv_write_t *write_req = malloc(sizeof(uv_write_t));
        uv_write(write_req, get_peer(sock), out_buf, 1, NULL);
        break;
    }
    default:
        fprintf(stderr, "[default] shouldn't send more data!!");
        shutdown_sockpair(sock);
    }
}

void handle(uv_stream_t *sock,
            ssize_t nread,
            const uv_buf_t *buf)
{
    context_t *ctx = (context_t *)sock->data;
    ctx->handler(sock, nread, buf);
}

void on_peer_connected(uv_connect_t *req, int status)
{
    uv_stream_t *peer = (uv_stream_t *)req->data;
    context_t *peer_ctx = (context_t *)peer->data;
    uv_stream_t *client = peer_ctx->peer;
    context_t *ctx = (context_t *)client->data;

    ctx->handler = handle_client;
    peer_ctx->handler = handle_peer;
    uv_read_start(client, alloc_buffer, handle);
    uv_read_start(peer, alloc_buffer, handle);

    uv_write_t *write_req = malloc(sizeof(uv_write_t));
    uv_buf_t *buf = malloc(sizeof(uv_buf_t));
    *buf = uv_buf_init((char *)ctx->buffer, 13);
    uv_write(write_req, peer, buf, 1, NULL);
}

void contact_peer(uv_stream_t *client)
{

    uv_tcp_t *peer = malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop, peer);

    context_t *ctx = create_context();
    ctx->peer = (uv_stream_t *)peer;
    client->data = (void *)ctx;

    context_t *peer_ctx = create_context();
    peer_ctx->state = UNSPEC;
    peer_ctx->peer = client;
    peer->data = (void *)peer_ctx;

    uv_connect_t *req = (uv_connect_t *)malloc(sizeof(uv_connect_t));
    req->data = (void *)peer;
    struct sockaddr_in addr;
    uv_ip4_addr(peer_addr, atoi(peer_port), &addr);
    uv_tcp_connect(req, peer, (const struct sockaddr *)&addr, on_peer_connected);
}

void on_new_connection(uv_stream_t *server, int status)
{
    // printf("new connection\n");
    if (status < 0)
    {
        fprintf(stderr, "New connection error %s\n", uv_strerror(status));
        return;
    }

    uv_tcp_t *client = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop, client);
    if (uv_accept(server, (uv_stream_t *)client) == 0)
    {
        contact_peer((uv_stream_t *)client);
    }
    else
    {
        uv_close((uv_handle_t *)client, NULL);
    }
}

int main(int argc, const char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "./remote [port] [remote-ip] [remote-port]\n");
        return 1;
    }
    peer_addr = argv[2];
    peer_port = argv[3];

    signal(SIGPIPE, SIG_IGN);

    loop = malloc(sizeof(uv_loop_t));
    uv_loop_init(loop);

    uv_tcp_t server;
    uv_tcp_init(loop, &server);

    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", atoi(argv[1]), &addr);
    uv_tcp_bind(&server, (const struct sockaddr *)&addr, 0);

    int r;
    if ((r = uv_listen((uv_stream_t *)&server, 10, on_new_connection)))
    {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
        return 1;
    }
    return uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);
    free(loop);
    return 0;
}
