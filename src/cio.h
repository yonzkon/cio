#ifndef __CIO_H
#define __CIO_H

#ifdef __cplusplus
extern "C" {
#endif

struct cio;
struct cio_event;

enum cio_fd_type {
    CIOF_T_LISTEN = 'l',
    CIOF_T_ACCEPT = 'a',
    CIOF_T_CONNECT = 'c',
};

enum cio_fd_flag {
    CIOF_READABLE = (1 << 0),
    CIOF_WRITABLE = (1 << 1),
};

enum cioe_code {
    CIOE_READABLE = 1,
    CIOE_WRITABLE = 2,
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
 * cio_register
 * @type: cio_fd_type
 * @flags: cio_fd_flag
 */
int cio_register(struct cio *ctx, int fd, char type, int flags);

/**
 * cio_unregister
 */
int cio_unregister(struct cio *ctx, int fd);

/**
 * cio_poll
 */
int cio_poll(struct cio *ctx, unsigned long usec);

/**
 * cioe_iter
 */
struct cio_event *cioe_iter(struct cio *ctx);

/**
 * cioe_get_code
 * @return: event_code: CIOE_READABLE:1, CIOE_WRITABLE:2
 */
int cioe_get_code(struct cio_event *ev);

/**
 * cioe_get_fd
 * @return: file descriptor
 */
int cioe_get_fd(struct cio_event *ev);

/**
 * cioe_get_type
 * @return: fd_type: CIOF_T_LISTEN:'l', CIOF_T_CONNECT:'c', CIOF_T_ACCEPT:'a'
 */
char cioe_get_fd_type(struct cio_event *ev);

/**
 * cioe_get_ts
 * @return: timestamp: usec
 */
unsigned long cioe_get_ts(struct cio_event *ev);

#ifdef __cplusplus
}
#endif
#endif
