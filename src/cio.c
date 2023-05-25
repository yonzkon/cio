#include <unistd.h>

#ifndef __WIN32
#include <sys/select.h>
#else
#include <Winsock2.h>
#endif

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "cio.h"
#include "cio-event.h"
#include "stream.h"

struct cio {
    fd_set fds_read;
    int nfds_read;
    fd_set fds_write;
    int nfds_write;

    struct list_head streams;
    struct list_head events;

    struct timeval poll_ts;
    unsigned long idle_usec;
};

static void add_event(struct cio *ctx, struct stream *stream)
{
    //printf("[%p:add_event]: fd:%d, readable:%d, writable:%d\n",
    //       ctx, stream->fd, stream->state.bits.readable, stream->state.bits.writable);

    struct cio_event *pos;
    list_for_each_entry(pos, &ctx->events, ln) {
        if (pos->fin == 0 && pos->stream == stream &&
            pos->state.byte == stream->state.byte)
            return;
    }

    pos = malloc(sizeof(*pos));
    memset(pos, 0, sizeof(*pos));
    pos->fin = 0;
    pos->token = stream->token;
    pos->fd = stream->fd;
    pos->wrapper = stream->wrapper;
    pos->state.byte = stream->state.byte;
    gettimeofday(&pos->ts, NULL);
    pos->stream = stream;
    INIT_LIST_HEAD(&pos->ln);
    list_add_tail(&pos->ln, &ctx->events);
}

static void clear_event(struct cio *ctx)
{
    struct cio_event *pos, *n;
    list_for_each_entry_safe(pos, n, &ctx->events, ln) {
        if (pos->fin) {
            list_del(&pos->ln);
            free(pos);
        }
    }
}

struct cio *cio_new()
{
    struct cio *ctx = malloc(sizeof(*ctx));
    memset(ctx, 0, sizeof(*ctx));
    FD_ZERO(&ctx->fds_read);
    ctx->nfds_read = 0;
    FD_ZERO(&ctx->fds_write);
    ctx->nfds_write = 0;
    INIT_LIST_HEAD(&ctx->streams);
    INIT_LIST_HEAD(&ctx->events);
    return ctx;
}

void cio_drop(struct cio *ctx)
{
    struct stream *stream, *n_stream;
    list_for_each_entry_safe(stream, n_stream, &ctx->streams, ln) {
        FD_CLR(stream->fd, &ctx->fds_read);
        FD_CLR(stream->fd, &ctx->fds_write);
        list_del(&stream->ln);
        stream_drop(stream);
    }

    struct cio_event *event, *n_event;
    list_for_each_entry_safe(event, n_event, &ctx->events, ln) {
        list_del(&event->ln);
        free(event);
    }

    free(ctx);
}

int cio_register(struct cio *ctx, int fd, int token, int flags, void *wrapper)
{
    struct stream *pos;
    list_for_each_entry(pos, &ctx->streams, ln) {
        if (pos->fd == fd) {
            FD_CLR(fd, &ctx->fds_read);
            FD_CLR(fd, &ctx->fds_write);
            list_del(&pos->ln);
            stream_drop(pos); // it's safe as we break at next line
            break;
        }
    }

    struct stream *stream = stream_new(ctx, fd, token, wrapper);
    if (stream == NULL)
        return -1;
    list_add(&stream->ln, &ctx->streams);

    if ((flags & CIOF_READABLE) == CIOF_READABLE) {
        FD_SET(fd, &ctx->fds_read);
        if (fd + 1 > ctx->nfds_read)
            ctx->nfds_read = fd + 1;
    }

    if ((flags & CIOF_WRITABLE) == CIOF_WRITABLE) {
        FD_SET(fd, &ctx->fds_write);
        if (fd + 1 > ctx->nfds_write)
            ctx->nfds_write = fd + 1;
    }

    return 0;
}

int cio_unregister(struct cio *ctx, int fd)
{
    struct stream *pos, *n;
    list_for_each_entry_safe(pos, n, &ctx->streams, ln) {
        if (pos->fd == fd) {
            FD_CLR(fd, &ctx->fds_read);
            FD_CLR(fd, &ctx->fds_write);
            list_del(&pos->ln);
            stream_drop(pos);
            return 0;
        }
    }

    return -1;
}

static void cio_idle(struct cio *ctx, unsigned long usec)
{
    if (list_empty(&ctx->events)) {
        usleep(ctx->idle_usec);
        if (ctx->idle_usec != usec) {
            ctx->idle_usec += usec / 10;
            if (ctx->idle_usec > usec)
                ctx->idle_usec = usec;
        }
    } else {
        ctx->idle_usec = usec / 10;
    }
}

int cio_poll(struct cio *ctx, unsigned long usec)
{
    gettimeofday(&ctx->poll_ts, NULL);
    clear_event(ctx);

    struct timeval tv = { 0, 0 };
    fd_set fds_read;
    fd_set fds_write;
    memcpy(&fds_read, &ctx->fds_read, sizeof(fds_read));
    memcpy(&fds_write, &ctx->fds_write, sizeof(fds_write));

    int nr_fds_read = select(ctx->nfds_read, &fds_read, NULL, NULL, &tv);
    if (nr_fds_read == -1) {
        if (errno == EINTR)
            return 0;
        else
            return -1;
    }

#ifndef __WIN32
    int nr_fds_write = select(ctx->nfds_write, NULL, &fds_write, NULL, &tv);
    if (nr_fds_write == -1) {
        if (errno == EINTR)
            return 0;
        else
            return -1;
    }
#else
    int nr_fds_write = 0;
    if (fds_write.fd_count != 0) {
        nr_fds_write = select(ctx->nfds_write, NULL, &fds_write, NULL, &tv);
        if (nr_fds_write == -1) {
            if (errno == EINTR)
                return 0;
            else
                return -1;
        }
    }
#endif

    //printf("[%p:poll]: nr_fds_read:%d, nr_fds_write:%d\n",
    //       ctx, nr_fds_read, nr_fds_write);

    struct stream *pos;
    list_for_each_entry(pos, &ctx->streams, ln) {
        // save previous writable state
        u8 pre_writable = pos->state.bits.writable;

        // clear state
        pos->state.byte = 0;

        // set readable
        if (nr_fds_read != 0) {
            if (FD_ISSET(pos->fd, &fds_read)) {
                nr_fds_read--;
                pos->state.bits.readable = 1;
            }
        }

        // set writable
        if (nr_fds_write != 0) {
            if (FD_ISSET(pos->fd, &fds_write)) {
                nr_fds_write--;
                pos->state.bits.writable = 1;
            }
        }

        //printf("[%p:poll]: fd:%d, readable:%d, writable:%d\n",
        //       ctx, pos->fd, pos->state.bits.readable, pos->state.bits.writable);

        // if readable or writable from 0 to 1
        if (pos->state.bits.readable ||
            (pos->state.bits.writable && !pre_writable)) {
            add_event(ctx, pos);
        }
    }

    assert(nr_fds_read == 0);
    assert(nr_fds_write == 0);

    cio_idle(ctx, usec);
    return 0;
}

struct cio_event *cio_iter(struct cio *ctx)
{
    struct cio_event *pos;
    list_for_each_entry(pos, &ctx->events, ln) {
        if (pos->fin == 0) {
            pos->fin = 1;
            return pos;
        }
    }
    return NULL;
}

int cioe_is_readable(struct cio_event *ev)
{
    return ev->state.bits.readable;
}

int cioe_is_writable(struct cio_event *ev)
{
    return ev->state.bits.writable;
}

int cioe_get_token(struct cio_event *ev)
{
    return ev->token;
}

int cioe_getfd(struct cio_event *ev)
{
    return ev->fd;
}

void *cioe_get_wrapper(struct cio_event *ev)
{
    return ev->wrapper;
}

unsigned long cioe_get_ts(struct cio_event *ev)
{
    return ev->ts.tv_sec * 1000 * 1000 + ev->ts.tv_usec;
}
