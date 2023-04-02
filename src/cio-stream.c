#include "cio-stream.h"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * cio_stream
 */

enum cio_stream_type {
    CIOS_T_LISTEN = 'l',
    CIOS_T_ACCEPT = 'a',
    CIOS_T_CONNECT = 'c',
};

struct cio_stream {
    int fd;
    char *addr;
    char type; /* stream_type */
};

static struct cio_stream *cio_stream_new(const char *addr, int fd, int type)
{
    struct cio_stream *stream = malloc(sizeof(*stream));
    memset(stream, 0, sizeof(*stream));

    stream->fd = fd;
    stream->addr = strdup(addr);
    stream->type = type;

    return stream;
}

static void cio_stream_drop(struct cio_stream *stream)
{
    close(stream->fd);
    free(stream->addr);
    free(stream);
}

/**
 * tcp_listener
 */

struct tcp_listener *tcp_listener_bind(const char *addr)
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

    return (struct tcp_listener *)cio_stream_new(addr, fd, CIOS_T_LISTEN);
}

struct tcp_stream *tcp_listener_accept(struct tcp_listener *listener)
{
    struct cio_stream *stream = (struct cio_stream *)listener;;
    int fd = accept(stream->fd, NULL, NULL);
    if (fd == -1) {
        perror("accept");
        return NULL;
    }

    return (struct tcp_stream *)cio_stream_new(stream->addr, fd, CIOS_T_ACCEPT);
}

void tcp_listener_drop(struct tcp_listener *listener)
{
    struct cio_stream *stream = (struct cio_stream *)listener;;
    cio_stream_drop(stream);
}

int tcp_listener_get_raw(struct tcp_listener *listener)
{
    struct cio_stream *stream = (struct cio_stream *)listener;;
    return stream->fd;
}

/**
 * tcp_stream
 */

struct tcp_stream *tcp_stream_connect(const char *addr)
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

    return (struct tcp_stream *)cio_stream_new(addr, fd, CIOS_T_CONNECT);
}

int tcp_stream_recv(struct tcp_stream *tcp_stream, char *buf, size_t len)
{
    struct cio_stream *stream = (struct cio_stream *)tcp_stream;;
    assert(stream->type != CIOS_T_LISTEN);
    return recv(stream->fd, buf, len, 0);
}

int tcp_stream_send(struct tcp_stream *tcp_stream, const char *buf, size_t len)
{
    struct cio_stream *stream = (struct cio_stream *)tcp_stream;;
    assert(stream->type != CIOS_T_LISTEN);
    return send(stream->fd, buf, len, 0);
}

void tcp_stream_drop(struct tcp_stream *tcp_stream)
{
    struct cio_stream *stream = (struct cio_stream *)tcp_stream;;
    cio_stream_drop(stream);
}

int tcp_stream_get_raw(struct tcp_stream *tcp_stream)
{
    struct cio_stream *stream = (struct cio_stream *)tcp_stream;;
    return stream->fd;
}

/**
 * unix_listener
 */

struct unix_listener *unix_listener_bind(const char *addr)
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

    return (struct unix_listener *)cio_stream_new(addr, fd, CIOS_T_LISTEN);
}

struct unix_stream *unix_listener_accept(struct unix_listener *listener)
{
    struct cio_stream *stream = (struct cio_stream *)listener;;
    int fd = accept(stream->fd, NULL, NULL);
    if (fd == -1) {
        perror("accept");
        return NULL;
    }

    return (struct unix_stream *)cio_stream_new(stream->addr, fd, CIOS_T_ACCEPT);
}

void unix_listener_drop(struct unix_listener *listener)
{
    struct cio_stream *stream = (struct cio_stream *)listener;;
    unlink(stream->addr);
    cio_stream_drop(stream);
}

int unix_listener_get_raw(struct unix_listener *listener)
{
    struct cio_stream *stream = (struct cio_stream *)listener;;
    return stream->fd;
}

/**
 * unix_stream
 */

struct unix_stream *unix_stream_connect(const char *addr)
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

    return (struct unix_stream *)cio_stream_new(addr, fd, CIOS_T_CONNECT);
}

int unix_stream_recv(struct unix_stream *unix_stream, char *buf, size_t len)
{
    struct cio_stream *stream = (struct cio_stream *)unix_stream;;
    assert(stream->type != CIOS_T_LISTEN);
    return recv(stream->fd, buf, len, 0);
}

int unix_stream_send(struct unix_stream *unix_stream, const char *buf, size_t len)
{
    struct cio_stream *stream = (struct cio_stream *)unix_stream;;
    assert(stream->type != CIOS_T_LISTEN);
    return send(stream->fd, buf, len, 0);
}

void unix_stream_drop(struct unix_stream *unix_stream)
{
    struct cio_stream *stream = (struct cio_stream *)unix_stream;;
    cio_stream_drop(stream);
}

int unix_stream_get_raw(struct unix_stream *unix_stream)
{
    struct cio_stream *stream = (struct cio_stream *)unix_stream;;
    return stream->fd;
}
