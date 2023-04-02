#ifndef __STREAM_H
#define __STREAM_H

#include "types.h"
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

enum stream_state {
    STREAM_ST_OPENED = 1,
    STREAM_ST_FINISHED,
    STREAM_ST_CLOSED,
};

struct stream {
    int fd;
    char type; /* cio_fd_type */
    int state; /* stream_state */
    void *wrapper; /* the fd wrapper */

    union {
        u8 byte;
        struct {
            u8 open:1;
            u8 close:1;
            u8 readable:1;
            u8 writable:1;
        } bits;
    } ev;

    struct cio *ctx;
    struct list_head ln;
};

struct stream *stream_new(struct cio *ctx, int fd, char type, void *wrapper);
void stream_drop(struct stream *stream);

#ifdef __cplusplus
}
#endif
#endif
