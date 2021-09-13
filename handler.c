#include <string.h>
#include <netinet/in.h>
#include "lib.h"

void accept_connection(int sock)
{
  int client;
  struct sockaddr_in client_addr;
  socklen_t addr_size = ADDR_SIZE;

  if (-1 == (client = accept(sock, (struct sockaddr *)&client_addr, &addr_size))) {
    dprintf(2, "[%d] failed to accept\n", errno);
  }
  
  printf("ACCEPTED\n");
  add_session(client);
}


void echo(int sock)
{

  char buffer[BUF_SIZE];
  size_t read_size;
  printf("HANDLED\n");

  if (-1 == (read_size = recv(sock, &buffer, BUF_SIZE, 0))) {
    dprintf(2, "[%d] failed to read\n", errno);
  } else if (read_size == 0) {
    printf("connection closed\n");
    remove_session(sock);
  } else if (read_size != (write(sock, &buffer, read_size))) {
    dprintf(2, "[%d] failed to write\n", errno);
  }
}

void forward(int sock_a, int sock_b)
{
  char buffer[BUF_SIZE];
  size_t read_size;

  if (-1 == (read_size = recv(sock_a, &buffer, BUF_SIZE, 0))) {
    dprintf(2, "[%d] failed to read\n", errno);
  } else if (read_size == 0) {
    printf("connection closed\n");
    remove_session(sock_a);
  } else if (read_size != (write(sock_b, &buffer, read_size))) {
    dprintf(2, "[%d] failed to write\n", errno);
  }
}

void handle_session(struct pollfd *fd)
{
  // echo(fd->fd);
  if (!(fd->revents & POLLIN)) {
    return;
  }

  int sock = fd->fd;
  Session *sess = get_session(sock);

  void next_state(Session *sess);
  
  if (sess->status == CONNECTED) {
    forward(sock, sess->peer);
    return;
  }

  int nread = 0;
  void advance_state(Session *s);

  if (-1 == (nread = recv(
    sock, 
    sess->state.in.buf + sess->state.in.len,
    BUF_SIZE - sess->state.in.len,
    0
  ))) {
    dprintf(2, "[%d] failed to read\n", errno);
    remove_session(sock);
  } else if (0 == nread) {
    printf("connection closed\n");
    remove_session(sock);
  } else {
    printf("advance state\n");
    sess->state.in.len += nread;
    advance_state(sess);
    printf("recv (%d)\n", nread);
  }
}


void advance_state(Session *sess)
{
  State *s = &sess->state;
  printf("STATE [%d] ->\n", s->key);
  for(;;) {
    switch (s->key) {
      case -1: { // ERROR
        // TODO
      }
      case 0: { // VER (x05)
        if (s->in.len - s->in.idx>= 1) {
          s->in.idx += 1;
          s->key = 1;
          break;
        }
      }
      case 1: { // NMETHODS (1) + METHODS (1-255)
        int nmethods = 0, noauth = 0;
        if (s->in.len - s->in.idx < 2) return;
        nmethods = s->in.buf[s->in.idx];
        if (s->in.len - s->in.idx - 1 < nmethods) return;
        for (int i = 0; i < nmethods; i++) {
          if (s->in.buf[s->in.idx + i] == 0) {
            noauth = 1;
            break;
          }
        }
        if (!noauth) {
          s->key = -1;
          return;
        }

        // VER (x05) + METHOD (x00)
        unsigned short ver_methods = htons(0x0500);
        if (2 != (write(sess->socket, (void *)&ver_methods, 2)))
          dprintf(2, "[%d] failed to write\n", errno);

        s->in.idx += 1 + nmethods;
        s->key = 2;
        break;
      }
      case 2: { // VER (x05) + CMD (x01/x02/x03) + RSV (x00)
        if (s->in.len - s->in.idx < 3) return;
        if (s->in.buf[s->in.idx + 1] != 1) {
          s->key = -1;
          return;
        }
        s->in.idx += 3;
        s->key = 3;
        break;
      }
      case 3: { // ATYP (x01/x03/x04) + DST.ADDR (N)
        if (s->in.len - s->in.idx < 1) return;
        int atyp = s->in.buf[s->in.idx];
        sess->dest.atyp = atyp;
        switch (atyp) {
          case 1: {
            if (s->in.len - s->in.idx - 1 < 4) return;
            sess->dest.addr.in = *(struct in_addr *)(s->in.buf + s->in.idx + 1);
            s->in.idx += 5;
            break;
          }
          case 3: {
            int n = s->in.buf[s->in.idx + 1];
            if (s->in.len - s->in.idx - 1 < n) return;
            // sess->dest.addr.dn = strndup(s->in.buf + s->in.idx + 1, n);
            s->in.idx += 2 + n;
            break;
          }
          case 4: {
            if (s->in.len - s->in.idx - 1 < 16) return;
            sess->dest.addr.in6 = *(struct in6_addr *)(s->in.buf + s->in.idx + 1);
            s->in.idx += 17;
            break;
          }
          default: {
            s->key = -1;
            return;
          }
        }
        s->key = 4;
        break;
      }
      case 4: { // DST.PORT (2)
        if (s->in.len - s->in.idx < 2) return;
        sess->dest.port = *(unsigned short *)(s->in.buf + s->in.idx);

        switch (sess->dest.atyp) {
          case IPV4: {
            sess->peer = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
            struct sockaddr_in sin;
            sin.sin_len = sizeof(sin);
            sin.sin_family = AF_INET;
            sin.sin_port = sess->dest.port;
            sin.sin_addr = sess->dest.addr.in;

            
            if (-1 == connect(sess->peer, (struct sockaddr *)&sin, sizeof(sin))) {
              dprintf(2, "[%d] failed to connect\n", errno);
              s->key = -1;
              return;
            }
            printf("IPV4 connected\n");
            break;
          }
          case DOMAIN: {
            // TODO
            break;
          }
          case IPV6: {
            sess->peer = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
            struct sockaddr_in6 sin6;
            sin6.sin6_len = sizeof(sin6);
            sin6.sin6_family = AF_INET6;
            sin6.sin6_flowinfo = 0;
            sin6.sin6_port = sess->dest.port;
            sin6.sin6_addr = sess->dest.addr.in6;
            if (-1 == connect(sess->peer, (struct sockaddr *)&sin6, sizeof(sin6))) {
              dprintf(2, "[%d] failed to connect\n", errno);
              s->key = -1;
              return;
            }
            printf("IPV6 connected\n");
            break;
          }
          default:
            s->key = -1;
            return;
        }
        s->in.idx += 2;
        s->key = 5;
        break;
      }
      case 5: { // VER (x05) + REP (x00/x0X) + RSV (x00) + ATYP (x01)
        switch (sess->dest.atyp) {
          int sock;
          case IPV4: {
            struct sockaddr_in sin;
            socklen_t addr_len;
            getsockname(sess->peer, (struct sockaddr *)&sin, &addr_len);
            unsigned long buf = htonl(0x05000001);
            if (-1 == write(sess->socket, &buf, 4))
              dprintf(2, "[%d] failed to write\n", errno);
            if (-1 == write(sess->socket, &sin.sin_addr, 4))
              dprintf(2, "[%d] failed to write\n", errno);
            if (-1 == write(sess->socket, &sin.sin_port, 2))
              dprintf(2, "[%d] failed to write\n", errno);
            break;
          }
          case DOMAIN: {
            // TODO
            break;
          }
          case IPV6: {
            struct sockaddr_in6 sin;
            socklen_t addr_len;
            getsockname(sess->peer, (struct sockaddr *)&sin, &addr_len);
            unsigned long buf = htonl(0x05000004);
            if (-1 == write(sess->socket, &buf, 4))
              dprintf(2, "[%d] failed to write\n", errno);
            if (-1 == write(sess->socket, &sin.sin6_addr, 16))
              dprintf(2, "[%d] failed to write\n", errno);
            if (-1 == write(sess->socket, &sin.sin6_port, 2))
              dprintf(2, "[%d] failed to write\n", errno);
            break;
          }
          default:
            s->key = -1;
            return;
        }
        sess->status = CONNECTED;
        return;
      }
      default:
        return;
    }
    printf("->STATE [%d]\n", s->key);
  }

}














