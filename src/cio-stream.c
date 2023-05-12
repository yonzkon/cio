#include "cio-stream.h"
#include <unistd.h>

#ifndef __WIN32
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <Winsock2.h>

#if defined(NO_SOCKLEN_T)
typedef int socklen_t;
#endif /* NO_SOCKLEN_T */

#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL (0)
#endif
#endif

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/**
 * cio_stream
 */

enum cio_stream_type {
    CIOS_T_LISTEN = 'l',
    CIOS_T_ACCEPT = 'a',
    CIOS_T_CONNECT = 'c',
};

struct cio_stream_operations {
    void (*drop)(struct cio_stream *stream);
    int (*get_fd)(struct cio_stream *stream);
    int (*send)(struct cio_stream *stream, const void *buf, size_t len);
    int (*recv)(struct cio_stream *stream, void *buf, size_t size);
    struct cio_stream *(*accept)(struct cio_listener *listener);
};

struct cio_stream {
    int fd;
    char *addr;
    char type; /* stream_type */
    const struct cio_stream_operations *ops;
};

void cio_stream_drop(struct cio_stream *stream)
{
    assert(stream->ops->drop);
    stream->ops->drop(stream);
}

int cio_stream_get_fd(struct cio_stream *stream)
{
    assert(stream->ops->get_fd);
    return stream->ops->get_fd(stream);
}

int cio_stream_recv(struct cio_stream *stream, void *buf, size_t len)
{
    if (stream->ops->recv) {
        return stream->ops->recv(stream, buf, len);
    } else {
        return -1;
    }
}

int cio_stream_send(struct cio_stream *stream, const void *buf, size_t len)
{
    if (stream->ops->send) {
        return stream->ops->send(stream, buf, len);
    } else {
        return -1;
    }
}

void cio_listener_drop(struct cio_listener *listener)
{
    cio_stream_drop((struct cio_stream *)listener);
}

int cio_listener_get_fd(struct cio_listener *listener)
{
    return cio_stream_get_fd((struct cio_stream *)listener);
}

struct cio_stream *cio_listener_accept(struct cio_listener *listener)
{
    struct cio_stream *stream = (struct cio_stream *)listener;
    if(stream->ops->accept)
        return stream->ops->accept(listener);
    else
        return NULL;
}

static struct cio_stream *__cio_stream_new(
    const char *addr, int fd, int type, const struct cio_stream_operations *ops)
{
    struct cio_stream *stream = malloc(sizeof(*stream));
    memset(stream, 0, sizeof(*stream));

    stream->fd = fd;
    stream->addr = strdup(addr);
    stream->type = type;
    stream->ops = ops;

    return stream;
}

static void __cio_stream_drop(struct cio_stream *stream)
{
    close(stream->fd);
    free(stream->addr);
    free(stream);
}

static int __cio_stream_get_fd(struct cio_stream *stream)
{
    return stream->fd;
}

/**
 * tcp_stream
 */

static int tcp_stream_recv(struct cio_stream *stream, void *buf, size_t len)
{
    assert(stream->type != CIOS_T_LISTEN);
    return recv(stream->fd, buf, len, 0);
}

static int tcp_stream_send(struct cio_stream *stream, const void *buf, size_t len)
{
    assert(stream->type != CIOS_T_LISTEN);
    return send(stream->fd, buf, len, MSG_NOSIGNAL);
}

static struct cio_stream_operations tcp_stream_ops = {
    .drop = __cio_stream_drop,
    .get_fd = __cio_stream_get_fd,
    .send = tcp_stream_send,
    .recv = tcp_stream_recv,
    .accept = NULL,
};

static struct cio_stream *tcp_stream_connect(const char *addr)
{
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return NULL;

    uint32_t host;
    uint16_t port;
    char *tmp = strdup(addr);
    char *colon = strchr(tmp, ':');
    *colon = 0;
    host = inet_addr(tmp);
    port = htons(atoi(colon + 1));
    free(tmp);

    int rc = 0;
    struct sockaddr_in sockaddr = {0};
    sockaddr.sin_family = PF_INET;
    sockaddr.sin_addr.s_addr = host;
    sockaddr.sin_port = port;

    rc = connect(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (rc == -1) {
        perror("connect");
        close(fd);
        return NULL;
    }

    return __cio_stream_new(addr, fd, CIOS_T_CONNECT, &tcp_stream_ops);
}

/**
 * tcp_listener
 */

struct cio_stream *tcp_listener_accept(struct cio_listener *listener)
{
    struct cio_stream *stream = (struct cio_stream *)listener;;
    int fd = accept(stream->fd, NULL, NULL);
    if (fd == -1) {
        perror("accept");
        return NULL;
    }

    return __cio_stream_new(stream->addr, fd, CIOS_T_ACCEPT, &tcp_stream_ops);
}

static struct cio_stream_operations tcp_listener_ops = {
    .drop = __cio_stream_drop,
    .get_fd = __cio_stream_get_fd,
    .send = NULL,
    .recv = NULL,
    .accept = tcp_listener_accept,
};

static struct cio_listener *tcp_listener_bind(const char *addr)
{
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return NULL;

    uint32_t host;
    uint16_t port;
    char *tmp = strdup(addr);
    char *colon = strchr(tmp, ':');
    *colon = 0;
    host = inet_addr(tmp);
    port = htons(atoi(colon + 1));
    free(tmp);

    int rc = 0;
    struct sockaddr_in sockaddr = {0};
    sockaddr.sin_family = PF_INET;
    sockaddr.sin_addr.s_addr = host;
    sockaddr.sin_port = port;

    rc = bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (rc == -1) {
        perror("bind");
        close(fd);
        return NULL;
    }

    rc = listen(fd, 1000);
    if (rc == -1) {
        perror("listen");
        close(fd);
        return NULL;
    }

    return (struct cio_listener *)__cio_stream_new(
        addr, fd, CIOS_T_LISTEN, &tcp_listener_ops);
}

#ifndef __WIN32

/**
 * unix_stream
 */

static struct cio_stream_operations unix_stream_ops = {
    .drop = __cio_stream_drop,
    .get_fd = __cio_stream_get_fd,
    .send = tcp_stream_send,
    .recv = tcp_stream_recv,
    .accept = NULL,
};

static struct cio_stream *unix_stream_connect(const char *addr)
{
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
        return NULL;

    int rc = 0;
    struct sockaddr_un sockaddr = {0};
    sockaddr.sun_family = PF_UNIX;
    snprintf(sockaddr.sun_path, sizeof(sockaddr.sun_path), "%s", addr);

    rc = connect(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (rc == -1) {
        perror("connect");
        close(fd);
        return NULL;
    }

    return __cio_stream_new(addr, fd, CIOS_T_CONNECT, &unix_stream_ops);
}

/**
 * unix_listener
 */

static void unix_listener_drop(struct cio_stream *stream)
{
    unlink(stream->addr);
    __cio_stream_drop(stream);
}

static struct cio_stream_operations unix_listener_ops = {
    .drop = unix_listener_drop,
    .get_fd = __cio_stream_get_fd,
    .send = NULL,
    .recv = NULL,
    .accept = tcp_listener_accept,
};

static struct cio_listener *unix_listener_bind(const char *addr)
{
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
        return NULL;

    int rc = 0;
    struct sockaddr_un sockaddr = {0};
    sockaddr.sun_family = PF_UNIX;
    snprintf(sockaddr.sun_path, sizeof(sockaddr.sun_path), "%s", addr);

    unlink(addr);

    rc = bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (rc == -1) {
        perror("bind");
        close(fd);
        return NULL;
    }

    rc = listen(fd, 1000);
    if (rc == -1) {
        perror("listen");
        close(fd);
        return NULL;
    }

    return (struct cio_listener *)__cio_stream_new(
        addr, fd, CIOS_T_LISTEN, &unix_listener_ops);
}

#endif

/**
 * cio_stream_connect
 * cio_listener_bind
 */

#ifdef __WIN32
void __attribute__((constructor)) pre_main()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
}
#endif

struct cio_stream *cio_stream_connect(const char *addr)
{
    if (strstr(addr, "tcp://") == addr) {
        return tcp_stream_connect(addr + strlen("tcp://"));
    }

#ifndef __WIN32
    if (strstr(addr, "unix:/") == addr) {
        return unix_stream_connect(addr + strlen("unix:/"));
    }
#endif

    return NULL;
}

struct cio_listener *cio_listener_bind(const char *addr)
{
    if (strstr(addr, "tcp://") == addr) {
        return tcp_listener_bind(addr + strlen("tcp://"));
    }

#ifndef __WIN32
    if (strstr(addr, "unix:/") == addr) {
        return unix_listener_bind(addr + strlen("unix:/"));
    }
#endif

    return NULL;
}
