#include "stream.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

struct stream *stream_new(struct cio *ctx, int fd, char type, void *wrapper)
{
    struct stream *stream = malloc(sizeof(struct stream));
    memset(stream, 0, sizeof(*stream));

    stream->fd = fd;
    stream->type = type;
    stream->state = STREAM_ST_OPENED;
    stream->wrapper = wrapper;

    stream->ev.byte = 0;
    stream->ev.bits.open = 1;

    stream->ctx = ctx;
    INIT_LIST_HEAD(&stream->ln);

    return stream;
}

void stream_drop(struct stream *stream)
{
    if (stream->state != STREAM_ST_FINISHED) {
        stream->ev.bits.close = 1;
        return;
    }

    assert(stream->state == STREAM_ST_FINISHED);

    stream->ctx = NULL;
    list_del_init(&stream->ln);
    free(stream);
}
