#include <sched.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "cio.h"
#include "cio-stream.h"

#define TCP_ADDR "127.0.0.1:1224"

/**
 * client
 */

static int client_finished = 0;

static void *client_thread(void *args)
{
    (void)args;

    sleep(1);

    struct tcp_stream *stream = tcp_stream_connect(TCP_ADDR);
    assert_true(stream);

    struct cio *ctx = cio_new();
    cio_register(ctx, tcp_stream_get_raw(stream),
                 CIOF_T_CONNECT, CIOF_READABLE | CIOF_WRITABLE, stream);
    for (;;) {
        if (client_finished)
            break;
        assert_true(cio_poll(ctx, 100 * 1000) == 0);
        for (;;) {
            struct cio_event *ev = cioe_iter(ctx);
            if (!ev) break;
            printf("fetch a event on client: %c,%d\n",
                   cioe_get_fd_type(ev),
                   cioe_get_code(ev));
            switch (cioe_get_fd_type(ev)) {
                case CIOF_T_CONNECT: {
                    int fd = cioe_get_fd(ev);
                    int code = cioe_get_code(ev);
                    struct tcp_stream *stream = (struct tcp_stream *)cioe_get_wrapper(ev);
                    if (code == CIOE_WRITABLE) {
                        char *payload = "from client";
                        int nr = tcp_stream_send(stream, payload, strlen(payload));
                        printf("[client:send]: nr:%d, buf:%s\n", nr, payload);
                        cio_unregister(ctx, fd);
                        cio_register(ctx, fd, CIOF_T_CONNECT, CIOF_READABLE, stream);
                    } else if (code == CIOE_READABLE) {
                        char buf[256] = {0};
                        int nr = tcp_stream_recv(stream, buf, sizeof(buf));
                        if (nr == 0 || nr == -1) {
                            cio_unregister(ctx, tcp_stream_get_raw(stream));
                            tcp_stream_drop(stream);
                        } else {
                            printf("[client:recv]: nr:%d, buf:%s\n", nr, buf);
                            cio_unregister(ctx, tcp_stream_get_raw(stream));
                            tcp_stream_drop(stream);
                            client_finished = 1;
                        }
                    }
                    break;
                }
            }
        }
    }

    tcp_stream_drop(stream);
    sleep(1);
    cio_drop(ctx);
    return NULL;
}

/**
 * server
 */

static int server_finished = 0;

static void *server_thread(void *args)
{
    (void)args;

    struct tcp_listener *listener = tcp_listener_bind(TCP_ADDR);
    assert_true(listener);

    struct cio *ctx = cio_new();
    cio_register(ctx, tcp_listener_get_raw(listener),
                 CIOF_T_LISTEN, CIOF_READABLE, listener);
    for (;;) {
        if (server_finished)
            break;
        assert_true(cio_poll(ctx, 100 * 1000) == 0);
        for (;;) {
            struct cio_event *ev = cioe_iter(ctx);
            if (!ev) break;
            printf("fetch a event on server: %c,%d\n",
                   cioe_get_fd_type(ev),
                   cioe_get_code(ev));
            switch (cioe_get_fd_type(ev)) {
                case CIOF_T_LISTEN: {
                    int code = cioe_get_code(ev);
                    struct tcp_listener *listener =
                        (struct tcp_listener *)cioe_get_wrapper(ev);
                    if (code == CIOE_READABLE) {
                        struct tcp_stream *new_stream =
                            tcp_listener_accept(listener);
                        cio_register(ctx, tcp_stream_get_raw(new_stream),
                                     CIOF_T_ACCEPT, CIOF_READABLE, new_stream);
                    }
                    break;
                }
                case CIOF_T_ACCEPT: {
                    int code = cioe_get_code(ev);
                    struct tcp_stream *stream =
                        (struct tcp_stream *)cioe_get_wrapper(ev);
                    if (code == CIOE_READABLE) {
                        char buf[256] = {0};
                        int nr = tcp_stream_recv(stream, buf, sizeof(buf));
                        if (nr == 0 || nr == -1) {
                            cio_unregister(ctx, tcp_stream_get_raw(stream));
                            tcp_stream_drop(stream);
                        } else {
                            printf("[server:recv]: nr:%d, buf:%s\n", nr, buf);
                            char *payload = "from server";
                            tcp_stream_send(stream, payload, strlen(payload));
                            printf("[server:send]: nr:%d, buf:%s\n", nr, payload);
                            sleep(1);
                            cio_unregister(ctx, tcp_stream_get_raw(stream));
                            tcp_stream_drop(stream);
                            server_finished = 1;
                        }
                    }
                    break;
                }
            }
        }
    }

    tcp_listener_drop(listener);
    sleep(1);
    cio_drop(ctx);
    return NULL;
}

/**
 * test_cio
 */

static void test_cio(void **status)
{
    (void)status;

    pthread_t server_pid;
    pthread_create(&server_pid, NULL, server_thread, NULL);
    pthread_t client_pid;
    pthread_create(&client_pid, NULL, client_thread, NULL);

    for (;;) {
        if (client_finished && server_finished)
            break;
    }

    pthread_join(client_pid, NULL);
    pthread_join(server_pid, NULL);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_cio),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
