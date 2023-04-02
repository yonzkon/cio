#ifndef __CIO_STREAM_H
#define __CIO_STREAM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tcp_listener;
struct tcp_stream;

struct tcp_listener *tcp_listener_bind(const char *addr);
struct tcp_stream *tcp_listener_accept(struct tcp_listener *listener);
void tcp_listener_drop(struct tcp_listener *listener);
int tcp_listener_get_raw(struct tcp_listener *listener);

struct tcp_stream *tcp_stream_connect(const char *addr);
int tcp_stream_recv(struct tcp_stream *stream, char *buf, size_t len);
int tcp_stream_send(struct tcp_stream *stream, const char *buf, size_t len);
void tcp_stream_drop(struct tcp_stream *stream);
int tcp_stream_get_raw(struct tcp_stream *stream);

#ifdef __cplusplus
}
#endif
#endif
