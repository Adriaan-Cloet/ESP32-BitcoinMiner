/*
 * POSIX implementation of the line-oriented TCP client. The feature-test macro
 * below exposes getaddrinfo and friends under -std=c11, which otherwise hides
 * them.
 */
#define _POSIX_C_SOURCE 200112L

#include "net_posix.h"

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

bool net_connect(net_conn_t *conn, const char *host, const char *port)
{
    if (conn == NULL || host == NULL || port == NULL) {
        return false;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;   /* IPv4 or IPv6, whichever resolves */
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        return false;
    }

    int fd = -1;
    for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) {
        return false;
    }

    conn->fd  = fd;
    conn->len = 0;
    return true;
}

/* Send exactly n bytes, looping over partial writes. */
static bool send_all(int fd, const char *data, size_t n)
{
    size_t sent = 0;
    while (sent < n) {
        ssize_t r = send(fd, data + sent, n - sent, 0);
        if (r <= 0) {
            return false;
        }
        sent += (size_t)r;
    }
    return true;
}

bool net_send_line(net_conn_t *conn, const char *line)
{
    if (conn == NULL || line == NULL) {
        return false;
    }
    return send_all(conn->fd, line, strlen(line)) &&
           send_all(conn->fd, "\n", 1);
}

int net_recv_line(net_conn_t *conn, char *out, size_t out_size)
{
    if (conn == NULL || out == NULL || out_size == 0) {
        return -1;
    }

    for (;;) {
        /* Is a complete line already buffered? */
        for (size_t i = 0; i < conn->len; i++) {
            if (conn->buf[i] != '\n') {
                continue;
            }
            size_t line_len = i;
            if (line_len > 0 && conn->buf[line_len - 1] == '\r') {
                line_len--; /* strip CR from a CRLF terminator */
            }
            if (line_len + 1 > out_size) {
                return -1;
            }
            memcpy(out, conn->buf, line_len);
            out[line_len] = '\0';

            /* Shift whatever followed the newline to the front of the buffer. */
            size_t consumed = i + 1;
            memmove(conn->buf, conn->buf + consumed, conn->len - consumed);
            conn->len -= consumed;
            return 1;
        }

        if (conn->len == sizeof conn->buf) {
            return -1; /* no newline in a full buffer: line too long */
        }

        ssize_t r = recv(conn->fd, conn->buf + conn->len,
                         sizeof conn->buf - conn->len, 0);
        if (r == 0) {
            return 0; /* peer closed */
        }
        if (r < 0) {
            return -1;
        }
        conn->len += (size_t)r;
    }
}

void net_close(net_conn_t *conn)
{
    if (conn != NULL && conn->fd >= 0) {
        close(conn->fd);
        conn->fd = -1;
    }
}