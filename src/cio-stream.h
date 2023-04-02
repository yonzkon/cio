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

struct unix_listener *unix_listener_bind(const char *addr);
struct unix_stream *unix_listener_accept(struct unix_listener *listener);
void unix_listener_drop(struct unix_listener *listener);
int unix_listener_get_raw(struct unix_listener *listener);

struct unix_stream *unix_stream_connect(const char *addr);
int unix_stream_recv(struct unix_stream *unix_stream, char *buf, size_t len);
int unix_stream_send(struct unix_stream *unix_stream, const char *buf, size_t len);
void unix_stream_drop(struct unix_stream *unix_stream);
int unix_stream_get_raw(struct unix_stream *unix_stream);

#ifdef __cplusplus
}
#endif
#endif
