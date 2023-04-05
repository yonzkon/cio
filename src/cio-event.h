#ifndef __CIO_EVENT_H
#define __CIO_EVENT_H

#include <sys/time.h>
#include "list.h"
#include "cio.h"
#include "stream.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cio_event {
    int fin;
    int code;
    int token;
    int fd;
    void *wrapper;
    struct timeval ts;
    struct stream *stream;
    struct list_head ln;
};

#ifdef __cplusplus
}
#endif
#endif
