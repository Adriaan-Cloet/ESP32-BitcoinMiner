/*
 * Line-oriented TCP client over BSD sockets.
 *
 * Stratum v1 is newline-framed JSON over a plain TCP connection, so this is the
 * only transport the miner needs. It is deliberately written against the POSIX
 * socket API and nothing else: lwIP on the ESP32 offers the same BSD sockets,
 * so this file compiles unchanged there. That is the whole reason the network
 * code can be developed and debugged on the laptop first.
 */
#ifndef NET_POSIX_H
#define NET_POSIX_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Receive buffer. A single mining.notify line stays well under this, even with
 * a long merkle branch. If a line ever exceeds it, net_recv_line reports an
 * error rather than silently splitting a message.
 */
#define NET_RECV_BUF_SIZE 8192

typedef struct {
    int    fd;
    char   buf[NET_RECV_BUF_SIZE];
    size_t len; /* bytes currently buffered but not yet returned as a line */
} net_conn_t;

/*
 * Connect to host:port (port as a string, e.g. "21496"). Resolves the name and
 * tries each address until one connects. Returns true on success; on failure
 * *conn is left unusable and must not be passed to the calls below.
 */
bool net_connect(net_conn_t *conn, const char *host, const char *port);

/* Send one line: the bytes of line followed by a single '\n'. */
bool net_send_line(net_conn_t *conn, const char *line);

/*
 * Switch the connection to non-blocking mode. After this, net_recv_line must
 * not be used (it would busy-return); use net_poll_line instead. Done once,
 * after the blocking handshake, so the mining loop can read jobs without
 * stalling. Returns false on failure.
 */
bool net_set_nonblocking(net_conn_t *conn);

/*
 * Non-blocking line read for the mining loop. Returns:
 *    1  a line was read into out
 *    0  no complete line is available right now (keep mining)
 *   -1  the connection was closed or errored
 */
int net_poll_line(net_conn_t *conn, char *out, size_t out_size);

/*
 * Read the next '\n'-terminated line into out, without the newline and null
 * terminated. A trailing '\r' (CRLF) is stripped. Blocks until a full line is
 * available. Returns:
 *    1  a line was read
 *    0  the peer closed the connection
 *   -1  a socket error, or the line did not fit in out
 */
int net_recv_line(net_conn_t *conn, char *out, size_t out_size);

void net_close(net_conn_t *conn);

#endif /* NET_POSIX_H */