#include <unistd.h>

#ifndef WIN32
#include <fcntl.h>
#include <termios.h>
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

#include "cio-stream.h"
#include "types.h"

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
    int (*getfd)(struct cio_stream *stream);
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

int cio_stream_getfd(struct cio_stream *stream)
{
    assert(stream->ops->getfd);
    return stream->ops->getfd(stream);
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

int cio_listener_getfd(struct cio_listener *listener)
{
    return cio_stream_getfd((struct cio_stream *)listener);
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

static int __cio_stream_getfd(struct cio_stream *stream)
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
    .getfd = __cio_stream_getfd,
    .send = tcp_stream_send,
    .recv = tcp_stream_recv,
    .accept = NULL,
};

static struct cio_stream *tcp_stream_connect(const char *addr)
{
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return NULL;

    u32 host;
    u16 port;
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
    .getfd = __cio_stream_getfd,
    .send = NULL,
    .recv = NULL,
    .accept = tcp_listener_accept,
};

static struct cio_listener *tcp_listener_bind(const char *addr)
{
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return NULL;

    u32 host;
    u16 port;
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

#if defined __unix__ || __APPLE__

/**
 * unix_stream
 */

static struct cio_stream_operations unix_stream_ops = {
    .drop = __cio_stream_drop,
    .getfd = __cio_stream_getfd,
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
    .getfd = __cio_stream_getfd,
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
 * com_stream
 */

#if defined __unix__

static int param_get_int(const char *name, int *value, const char *param)
{
    char *start, *end;

    assert(name && value);
    if (!param) return -1;

    start = strstr(param, name);
    if (!start) return -1;

    start = strchr(start, '=');
    if (!start) return -1;
    start++;

    end = strchr(start, ' ');
    if (!end) end = strchr(start, '\0');

    void *tmp = malloc(end - start + 1);
    memset(tmp, 0, end - start + 1);
    memcpy(tmp, start, end - start);

    *value = atoi(tmp);
    free(tmp);
    return 0;
}

static int param_get_string(const char *name, void *buf, size_t size, const char *param)
{
    char *start, *end;

    assert(name && buf && size);
    if (!param) return -1;

    start = strstr(param, name);
    if (!start) return -1;

    start = strchr(start, '=');
    if (!start) return -1;
    start++;

    if (*start == '"') {
        start++;
        end = strchr(start, '"');
        if (!end) return -1;
    } else {
        end = strchr(start, ' ');
        if (!end) end = strchr(start, '\0');
    }

    memset(buf, 0, size);
    memcpy(buf, start, size - 1 < (size_t)(end - start) ? size - 1 : (size_t)(end - start));
    return 0;
}

static int com_stream_recv(struct cio_stream *stream, void *buf, size_t len)
{
    assert(stream->type == CIOS_T_CONNECT);
    return read(stream->fd, buf, len);
}

static int com_stream_send(struct cio_stream *stream, const void *buf, size_t len)
{
    assert(stream->type == CIOS_T_CONNECT);
    return write(stream->fd, buf, len);
}

static struct cio_stream_operations com_stream_ops = {
    .drop = __cio_stream_drop,
    .getfd = __cio_stream_getfd,
    .send = com_stream_send,
    .recv = com_stream_recv,
    .accept = NULL,
};

static struct cio_stream *com_stream_connect(const char *addr)
{
    int baud = 9600;
    int data_bit = 8;
    int stop_bit = 1;
    char parity[2] = "n";
    char com_addr[1024] = {0};

    assert(addr);
    param_get_int("baud", &baud, addr);
    param_get_int("data_bit", &data_bit, addr);
    param_get_int("stop_bit", &stop_bit, addr);
    param_get_string("parity", parity, sizeof parity, addr);
    assert(sscanf(addr, "%1023[^?]", com_addr) == 1);

    int fd = open(com_addr, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)
        return NULL;

    struct termios newtio, oldtio;

    if (tcgetattr(fd, &oldtio) != 0)
        goto err_out;

    memset(&newtio, 0, sizeof(newtio));
    newtio.c_cflag |= (CLOCAL | CREAD);
    newtio.c_cflag &= ~CSIZE;

    if (baud == 110) {
        cfsetispeed(&newtio, B110);
    } else if (baud == 300) {
        cfsetispeed(&newtio, B300);
    } else if (baud == 600) {
        cfsetispeed(&newtio, B600);
    } else if (baud == 1200) {
        cfsetispeed(&newtio, B1200);
    } else if (baud == 2400) {
        cfsetispeed(&newtio, B2400);
    } else if (baud == 4800) {
        cfsetispeed(&newtio, B4800);
    } else if (baud == 9600) {
        cfsetispeed(&newtio, B9600);
    } else if (baud == 19200) {
        cfsetispeed(&newtio, B19200);
    } else if (baud == 38400) {
        cfsetispeed(&newtio, B38400);
    } else if (baud == 57600) {
        cfsetispeed(&newtio, B57600);
    } else if (baud == 115200) {
        cfsetispeed(&newtio, B115200);
    } else {
        goto err_out;
    }

    if (data_bit == 5) {
        newtio.c_cflag |= CS5;
    } else if (data_bit == 6) {
        newtio.c_cflag |= CS6;
    } else if (data_bit == 7) {
        newtio.c_cflag |= CS7;
    } else if (data_bit == 8) {
        newtio.c_cflag |= CS8;
    } else {
        goto err_out;
    }

    if (parity[0] == 'O') {
        newtio.c_cflag |= PARENB;
        newtio.c_cflag |= PARODD;
        newtio.c_cflag |= (INPCK | ISTRIP);
    } else if (parity[0] == 'E') {
        newtio.c_cflag |= PARENB;
        newtio.c_cflag &= ~PARODD;
        newtio.c_cflag |= (INPCK | ISTRIP);
    } else if (parity[0] == 'N') {
        newtio.c_cflag &= ~PARENB;
    } else {
        goto err_out;
    }

    if (stop_bit == 1) {
        newtio.c_cflag &= ~CSTOPB;
    } else if (stop_bit == 2) {
        newtio.c_cflag |= CSTOPB;
    } else {
        goto err_out;
    }

    /* Raw input */
    newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    /*
     * C_OFLAG      Output options
     * OPOST        Postprocess output (not set = raw output)
     * ONLCR        Map NL to CR-NL
     *
     * ONCLR ant others needs OPOST to be enabled
     */

    /* Raw ouput */
    newtio.c_oflag &= ~OPOST;

    /* Software flow control is disabled */
    newtio.c_iflag &= ~(IXON | IXOFF | IXANY);

    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;
    tcflush(fd, TCIFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) != 0)
        goto err_out;

    return __cio_stream_new(addr, fd, CIOS_T_CONNECT, &com_stream_ops);

err_out:
    close(fd);
    return NULL;
}

#endif

/**
 * cio_stream_connect
 * cio_listener_bind
 */

#ifdef WIN32
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

#if defined __unix__ || __APPLE__
    if (strstr(addr, "unix://") == addr) {
        return unix_stream_connect(addr + strlen("unix://"));
    }
#endif

#if defined __unix__
    if (strstr(addr, "com://") == addr) {
        return com_stream_connect(addr + strlen("com://"));
    }
#endif

    return NULL;
}

struct cio_listener *cio_listener_bind(const char *addr)
{
    if (strstr(addr, "tcp://") == addr) {
        return tcp_listener_bind(addr + strlen("tcp://"));
    }

#if defined __unix__ || __APPLE__
    if (strstr(addr, "unix://") == addr) {
        return unix_listener_bind(addr + strlen("unix://"));
    }
#endif

    return NULL;
}
