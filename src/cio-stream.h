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
 * @addr: unix:///tmp/cio
 * @addr: unix://./text-cio
 * @addr: com:///dev/ttyUSB0?baud=9600&data_bit=8&stop_bit=1&parity=N
 * @addr: com://COM1?baud=9600&data_bit=8&stop_bit=1&parity=N
 * @baud: 110,300,600,1200,2400,4800,9600(default),19200,38400,57600,115200
 * @data_bit: 5,6,7,8(default)
 * @stop_bit: 1(default),2
 * @parity: n(default),e,o
 */
struct cio_stream *cio_stream_connect(const char *addr);

void cio_stream_drop(struct cio_stream *stream);
int cio_stream_getfd(struct cio_stream *stream);
int cio_stream_recv(struct cio_stream *stream, void *buf, size_t len);
int cio_stream_send(struct cio_stream *stream, const void *buf, size_t len);

/**
 * cio_listener_bind
 * @addr: tcp://127.0.0.1:3824
 * @addr: unix:///tmp/cio
 * @addr: unix://./text-cio
 */
struct cio_listener *cio_listener_bind(const char *addr);

void cio_listener_drop(struct cio_listener *listener);
int cio_listener_getfd(struct cio_listener *listener);
struct cio_stream *cio_listener_accept(struct cio_listener *listener);

#ifdef __cplusplus
}
#endif
#endif
