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

#define UNIX_ADDR "/tmp/cio-unix-stream-test"
#define TOKEN_LISTENER 1
#define TOKEN_STREAM 2

static int client_finished = 0;
static int server_finished = 0;

static void *client_thread(void *args)
{
    (void)args;

    struct cio_stream *stream = unix_stream_connect(UNIX_ADDR);
    assert_true(stream);

    struct cio *ctx = cio_new();
    cio_register(ctx, cio_stream_get_fd(stream),
                 TOKEN_STREAM, CIOF_READABLE | CIOF_WRITABLE, stream);

    for (;;) {
        if (client_finished)
            break;
        assert_true(cio_poll(ctx, 100 * 1000) == 0);
        for (;;) {
            struct cio_event *ev = cio_iter(ctx);
            if (!ev) break;
            printf("[client:iter]: fetch a event on client: token:%d, read:%d, write:%d\n",
                   cioe_get_token(ev), cioe_is_readable(ev), cioe_is_writable(ev));
            switch (cioe_get_token(ev)) {
                case TOKEN_STREAM: {
                    int fd = cioe_get_fd(ev);
                    struct cio_stream *stream = cioe_get_wrapper(ev);
                    if (cioe_is_writable(ev)) {
                        char *payload = "from client";
                        int nr = cio_stream_send(stream, payload, strlen(payload));
                        assert_true(nr > 0);
                        printf("[client:send]: nr:%d, buf:%s\n", nr, payload);
                        // send once, then wait response
                        cio_register(ctx, fd, TOKEN_STREAM, CIOF_READABLE, stream);
                    }
                    if (cioe_is_readable(ev)) {
                        char buf[256] = {0};
                        int nr = cio_stream_recv(stream, buf, sizeof(buf));
                        assert_true(nr > 0);
                        printf("[client:recv]: nr:%d, buf:%s\n", nr, buf);
                        client_finished = 1;
                    }
                    break;
                }
            }
        }
    }

    cio_stream_drop(stream);
    cio_drop(ctx);
    printf("[client]: eixt\n");
    return NULL;
}

static void *server_thread(void *args)
{
    (void)args;

    struct cio_listener *listener = unix_listener_bind(UNIX_ADDR);
    assert_true(listener);

    struct cio *ctx = cio_new();
    cio_register(ctx, cio_listener_get_fd(listener),
                 TOKEN_LISTENER, CIOF_READABLE, listener);

    for (;;) {
        if (server_finished && client_finished)
            break;
        assert_true(cio_poll(ctx, 100 * 1000) == 0);
        for (;;) {
            struct cio_event *ev = cio_iter(ctx);
            if (!ev) break;
            printf("[server:iter]: fetch a event on server: token:%d, read:%d, write:%d\n",
                   cioe_get_token(ev), cioe_is_readable(ev), cioe_is_writable(ev));
            switch (cioe_get_token(ev)) {
                case TOKEN_LISTENER: {
                    struct cio_listener *listener = cioe_get_wrapper(ev);
                    if (cioe_is_readable(ev)) {
                        struct cio_stream *new_stream = cio_listener_accept(listener);
                        cio_register(ctx, cio_stream_get_fd(new_stream),
                                     TOKEN_STREAM, CIOF_READABLE | CIOF_WRITABLE, new_stream);
                    }
                    break;
                }
                case TOKEN_STREAM: {
                    struct cio_stream *stream =
                        (struct cio_stream *)cioe_get_wrapper(ev);
                    if (cioe_is_readable(ev)) {
                        char buf[256] = {0};
                        int nr = cio_stream_recv(stream, buf, sizeof(buf));
                        if (nr == 0 || nr == -1) {
                            printf("[server:recv]: nr:%d, client fin\n", nr);
                            cio_unregister(ctx, cio_stream_get_fd(stream));
                            cio_stream_drop(stream);
                            server_finished = 1;
                        } else {
                            printf("[server:recv]: nr:%d, buf:%s\n", nr, buf);
                            if (cioe_is_writable(ev)) {
                                char *payload = "from server";
                                cio_stream_send(stream, payload, strlen(payload));
                                printf("[server:send]: nr:%d, buf:%s\n",
                                       (int)strlen(payload), payload);
                            }
                        }
                    }
                    break;
                }
            }
        }
    }

    cio_listener_drop(listener);
    cio_drop(ctx);
    printf("[server]: eixt\n");
    return NULL;
}

static void test_unix_stream(void **status)
{
    (void)status;

    pthread_t server_pid;
    pthread_create(&server_pid, NULL, server_thread, NULL);
    sleep(1);
    pthread_t client_pid;
    pthread_create(&client_pid, NULL, client_thread, NULL);

    for (;;) {
        if (client_finished && server_finished) {
            break;
        } else {
            sleep(1);
        }
    }

    pthread_join(client_pid, NULL);
    pthread_join(server_pid, NULL);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_unix_stream),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
