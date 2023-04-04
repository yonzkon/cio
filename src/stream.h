#ifndef __STREAM_H
#define __STREAM_H

#include "types.h"
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

struct stream {
    int fd;
    int token;
    void *wrapper; /* the fd wrapper */

    union {
        u8 byte;
        struct {
            u8 readable:1;
            u8 writable:1;
        } bits;
    } ev;

    struct cio *ctx;
    struct list_head ln;
};

struct stream *stream_new(struct cio *ctx, int fd, int token, void *wrapper);
void stream_drop(struct stream *stream);

#ifdef __cplusplus
}
#endif
#endif
