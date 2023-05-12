#ifndef __CIO_STREAM_H
#define __CIO_STREAM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cio_stream;
struct cio_listener;

/**
 * cio_stream_connect
 * @addr: tcp://127.0.0.1:3824
 * @addr: unix://tmp/cio
 * @addr: unix:/./text-cio
 */
struct cio_stream *cio_stream_connect(const char *addr);

void cio_stream_drop(struct cio_stream *stream);
int cio_stream_get_fd(struct cio_stream *stream);
int cio_stream_recv(struct cio_stream *stream, void *buf, size_t len);
int cio_stream_send(struct cio_stream *stream, const void *buf, size_t len);

/**
 * cio_listener_bind
 * @addr: tcp://127.0.0.1:3824
 * @addr: unix://tmp/cio
 * @addr: unix:/./text-cio
 */
struct cio_listener *cio_listener_bind(const char *addr);

void cio_listener_drop(struct cio_listener *listener);
int cio_listener_get_fd(struct cio_listener *listener);
struct cio_stream *cio_listener_accept(struct cio_listener *listener);

#ifdef __cplusplus
}
#endif
#endif
