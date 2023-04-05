#include "stream.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

struct stream *stream_new(struct cio *ctx, int fd, int token, void *wrapper)
{
    struct stream *stream = malloc(sizeof(struct stream));
    memset(stream, 0, sizeof(*stream));

    stream->fd = fd;
    stream->token = token;
    stream->wrapper = wrapper;
    stream->state.byte = 0;
    stream->ctx = ctx;
    INIT_LIST_HEAD(&stream->ln);

    return stream;
}

void stream_drop(struct stream *stream)
{
    stream->ctx = NULL;
    list_del_init(&stream->ln);
    free(stream);
}
