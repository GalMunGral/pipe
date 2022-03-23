#include <stdlib.h>
#include <assert.h>
#include <uv.h>

#define PADDING_SIZE 13

uv_loop_t *loop;

void alloc_buffer(uv_handle_t *handle,
                  size_t suggested_size,
                  uv_buf_t *buf)
{
    *buf = uv_buf_init((char *)malloc(suggested_size), suggested_size);
}

enum conn_state
{
    INIT = 0,
    ACCEPTING,
    CONNECTING,
    CONNECTED,
};

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

void on_connect(uv_connect_t *req, int status)
{
    // printf("ON_CONNECT\n");
    if (status < 0)
    {
        fprintf(stderr, "on_connect callback error %s\n", uv_err_name(status));
        return;
    }
    uv_stream_t *client = (uv_stream_t *)req->data;
    assert(client);
    context_t *ctx = (context_t *)client->data;
    assert(ctx);
    uv_stream_t *peer = ctx->peer;
    assert(peer);

    struct sockaddr_storage addr;
    int len;
    uv_tcp_getsockname((uv_tcp_t *)peer, (struct sockaddr *)&addr, &len);
    // printf("######### %hu\n", addr.ss_family);

    char msg[7];
    msg[0] = '\x01';
    memcpy(msg + 1, &((struct sockaddr_in *)&addr)->sin_addr, 4);
    memcpy(msg + 5, &((struct sockaddr_in *)&addr)->sin_port, 2);

    ctx->handler = forward_to_peer;
    uv_read_start(peer, alloc_buffer, forward_to_peer);

    uv_buf_t *buf = malloc(sizeof(uv_buf_t));
    *buf = uv_buf_init(msg, sizeof(msg));
    uv_write_t *write_req = malloc(sizeof(uv_write_t));
    uv_write(write_req, (uv_stream_t *)client, buf, 1, NULL);
}

void on_resolved(uv_getaddrinfo_t *resolver, int status, struct addrinfo *res)
{
    // printf("ON_RESOLVED\n");
    if (status < 0)
    {
        fprintf(stderr, "getaddrinfo callback error %s\n", uv_err_name(status));
        return;
    }

    if (!res)
    {
        fprintf(stderr, "no result %s\n", uv_err_name(status));
        return;
    }
    uv_stream_t *client = (uv_stream_t *)resolver->data;
    assert(client);

    uv_tcp_t *peer = malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop, peer);
    context_t *ctx = (context_t *)client->data;
    assert(ctx);
    ctx->peer = (uv_stream_t *)peer;

    context_t *peer_ctx = create_context();
    peer_ctx->peer = client;
    peer->data = (void *)peer_ctx;
    // printf("PEER ESTABLISHED\n");

    uv_connect_t *req = (uv_connect_t *)malloc(sizeof(uv_connect_t));
    req->data = (void *)client;

    uv_tcp_connect(req, peer, (const struct sockaddr *)res->ai_addr, on_connect);
    uv_freeaddrinfo(res);
}

void connect_to_remote(uv_stream_t *client, char *name, char *port)
{
    struct addrinfo hints;
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    uv_getaddrinfo_t *resolver = malloc(sizeof(uv_getaddrinfo_t));
    resolver->data = (void *)client;
    // printf("NAME: %s, PORT: %s\n", name, port);

    int r;
    if ((r = uv_getaddrinfo(loop, resolver, on_resolved, name, port, &hints)))
    {
        fprintf(stderr, "getaddrinfo call error %s\n", uv_err_name(r));
        uv_close((uv_handle_t *)client, NULL);
    }
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

void noop(uv_stream_t *client,
          ssize_t nread,
          const uv_buf_t *buf)
{
    if (nread < 0)
    {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t *)client, NULL);
        return;
    }

    if (nread == 0)
    {
        printf("uh oh = 0\n");
        return;
    }

    printf("SHOULDN'T BE HERE\n");
}

void parse_request(uv_stream_t *client,
                   ssize_t nread,
                   const uv_buf_t *buf)
{
    if (nread < 0)
    {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t *)client, NULL);
        return;
    }

    if (nread == 0)
    {
        printf("uh oh = 0\n");
        return;
    }

    context_t *ctx = (context_t *)client->data;
    buffer(client, nread, buf);
    if (ctx->offset <= PADDING_SIZE + 2)
        return;
    if (ctx->buffer[PADDING_SIZE] != '\x03')
    {
        fprintf(stderr, "not implemented\n");
        uv_close((uv_handle_t *)client, NULL);
        return;
    }

    // NAME & PORT
    int n_name = ctx->buffer[PADDING_SIZE + 1];
    int name_offset = PADDING_SIZE + 2;
    int port_offset = name_offset + n_name;
    int expected = port_offset + 2;

    if (ctx->offset < expected)
    {
        printf("LESS THAN EXPTECTed\n");
        return;
    }
    if (ctx->offset > expected)
    {
        printf("MORE THAN EXPTECTed\n");
        return;
    }

    memcpy(ctx->name, ctx->buffer + name_offset, n_name);
    unsigned short bin_port = *(unsigned short *)(ctx->buffer + port_offset);
    sprintf(ctx->port, "%hu", ntohs(bin_port));
    ctx->handler = noop;
    connect_to_remote(client, ctx->name, ctx->port);
}

void handshake(uv_stream_t *client,
               ssize_t nread,
               const uv_buf_t *buf)
{
    if (nread < 0)
    {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t *)client, NULL);
        return;
    }

    if (nread == 0)
    {
        printf("uh oh = 0\n");
        return;
    }

    buffer(client, nread, buf);
    context_t *ctx = (context_t *)client->data;
    if (ctx->offset < PADDING_SIZE)
        return;
    uv_write_t *write_req = malloc(sizeof(uv_write_t));
    uv_buf_t *buffer = malloc(sizeof(uv_buf_t));
    *buffer = uv_buf_init((char *)ctx->buffer, PADDING_SIZE);
    uv_write(write_req, client, buffer, 1, NULL);
    ctx->handler = parse_request;
}

void handle(uv_stream_t *client,
            ssize_t nread,
            const uv_buf_t *buf)
{
    context_t *ctx = (context_t *)client->data;
    ctx->handler(client, nread, buf);
}

void on_new_client(uv_stream_t *server, int status)
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
        context_t *data = create_context();
        data->handler = handshake;
        client->data = (void *)data;
        uv_read_start((uv_stream_t *)client, alloc_buffer, handle);
    }
    else
    {
        uv_close((uv_handle_t *)client, NULL);
    }
}

int main(int argc, const char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "./remote [port]\n");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    loop = malloc(sizeof(uv_loop_t));
    uv_loop_init(loop);

    uv_tcp_t server;
    uv_tcp_init(loop, &server);

    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", atoi(argv[1]), &addr);
    uv_tcp_bind(&server, (const struct sockaddr *)&addr, 0);

    int r;
    if ((r = uv_listen((uv_stream_t *)&server, 10, on_new_client)))
    {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
        return 1;
    }
    return uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);
    free(loop);
    return 0;
}
