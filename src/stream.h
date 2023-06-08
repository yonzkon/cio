#ifndef __STREAM_H
#define __STREAM_H

#include <stdint.h>
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

union stream_state {
    uint8_t byte;
    struct {
        uint8_t readable:1;
        uint8_t writable:1;
    } bits;
};

struct stream {
    int fd;
    int token;
    void *wrapper; /* the fd wrapper */
    union stream_state state;
    struct cio *ctx;
    struct list_head ln;
};

struct stream *stream_new(struct cio *ctx, int fd, int token, void *wrapper);
void stream_drop(struct stream *stream);

#ifdef __cplusplus
}
#endif
#endif
