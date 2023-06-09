#ifndef __CIO_H
#define __CIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cio;
struct cio_event;

enum cio_flag {
    CIOF_READABLE = (1 << 0),
    CIOF_WRITABLE = (1 << 1),
};

/**
 * cio_new
 */
struct cio *cio_new();

/**
 * cio_drop
 */
void cio_drop(struct cio *ctx);

/**
 * cio_register: next call with same fd will just update the type & flag & wrapper
 * @token: any value defined by user, maybe 1:LISENTER, 2:STREAM, 3:ACCEPT_STREAM
 * @flags: cio_fd_flag, CIOF_READABLE:(1<<0), CIOF_WRITABLE:(1<<1)
 * @wrapper: the wrapper of fd, maybe tcp_stream or something else
 */
int cio_register(struct cio *ctx, int fd, int token, int flags, void *wrapper);

/**
 * cio_unregister
 */
int cio_unregister(struct cio *ctx, int fd);

/**
 * cio_poll
 */
int cio_poll(struct cio *ctx, uint64_t usec);

/**
 * cioe_iter
 */
struct cio_event *cio_iter(struct cio *ctx);

/**
 * cioe_is_readable
 */
int cioe_is_readable(struct cio_event *ev);

/**
 * cioe_is_writable
 */
int cioe_is_writable(struct cio_event *ev);

/**
 * cioe_get_token
 * @return: token
 */
int cioe_get_token(struct cio_event *ev);

/**
 * cioe_getfd
 * @return: file descriptor
 */
int cioe_getfd(struct cio_event *ev);

/**
 * cioe_get_wrapper
 * @return: the wrapper of fd, maybe tcp_stream or something else
 */
void *cioe_get_wrapper(struct cio_event *ev);

/**
 * cioe_get_ts
 * @return: timestamp: usec
 */
uint64_t cioe_get_ts(struct cio_event *ev);

#ifdef __cplusplus
}
#endif
#endif
